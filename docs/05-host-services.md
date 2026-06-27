# 05 — Host 服务 API

Host 通过 `HostServices` 句柄向插件提供一个服务面。本文档列出全部服务
及其 API 摘要。插件 ABI 详见 [04-plugin-spec.md](04-plugin-spec.md)。

---

## HostServices 概览

`onLoad(const PluginContext& ctx)` 时，Host 注入 `ctx.host`(HostServices*),
插件保存该指针即可访问所有服务。

```cpp
namespace Margin {

class HostServices {
public:
    virtual ~HostServices() = default;

    // 引用:Host 保证非空
    virtual Logger&       logger()   = 0;
    virtual EventBus&     eventBus() = 0;
    virtual Settings&     settings() = 0;
    virtual TrayService&  tray()     = 0;

    // 指针:某些 Milestone 前可能为 nullptr,插件需 null 校验
    virtual CryptoService* crypto()   = 0;
    virtual QmlService*    qml()      = 0;
    virtual Database*      database() = 0;
};

} // namespace Margin
```

前 4 个方法返回引用，后 3 个返回指针，原因如下：Host 保证前 4 个服务始终存在；
后 3 个在某些阶段可能未就绪，返回 nullptr 供插件显式判空。

---

## 服务清单

| 服务 | 跨平台 | 线程安全 | 说明 |
|---|---|---|---|
| Logger | 是 | 是（lock-free 队列） | stdout 与文件双 sink，自动轮转 |
| EventBus | 是 | 是（主线程派发） | pub/sub，支持 `*` 与 `#` 通配符 |
| Settings | 是 | 是（QMutex) | JSON，敏感字段自动加密 |
| TrayService | 是 | 主线程 | toast、三态图标与插件菜单刷新 |
| CryptoService | 是 | 是 | 每插件 HKDF 密钥，AES-256-GCM |
| QmlService | 是 | 主线程 | 插件 QObject 注入到 QML 上下文 |
| Database | 是 | 是（WAL) | SQLite，每个插件具有表前缀 |

---

## Logger

```cpp
class Logger {
public:
    enum class Level { Debug, Info, Warn, Error, Fatal };
    virtual void log(Level level, const QString& tag, const QString& msg) = 0;

    void info(const QString& tag, const QString& msg);  // 便捷封装
    // debug、warn、error、fatal 同理
};
```

- 路径：`<logs>/margin.log`(Windows:`%LOCALAPPDATA%\Margin\logs\`)
- 格式：`[YYYY-MM-DD HH:MM:SS] [LEVEL] [tag] message`
- 轮转：按天轮转，保留 7 天，单文件上限 5 MB
- 最低级别可配：`general.log_level`（默认为 `Info`)

```cpp
ctx.host->logger().info("hello", "HelloPlugin loaded");
```

---

## EventBus

```cpp
class EventBus {
public:
    virtual void subscribe(const QString& topic,
                           std::function<void(const QJsonObject&)> handler,
                           QObject* subscriber = nullptr) = 0;
    virtual void publish(const QString& topic, const QJsonObject& payload) = 0;
    virtual void unsubscribeAll(QObject* subscriber) = 0;
};
```

- `publish()` 线程安全，可在任意线程调用
- `handler` 派发到主线程执行（`Qt::QueuedConnection`)
- `subscriber` 用于插件卸载时批量清理（传入 `ctx.subscriberIdentity`)

主题命名：`margin.<plugin_id>.<event_name>`，例如
`margin.aura_locker.away` 或 `margin.rhythm.break_due`。

通配符：

- `*` 单层匹配：`margin.aura_locker.*` 匹配 `margin.aura_locker.away`，但不匹配 `margin.aura_locker.command.set`
- `#` 多层匹配：`margin.aura_locker.#` 匹配上述两者

---

## Settings

```cpp
class Settings {
public:
    virtual QVariant get(const QString& key,
                         const QVariant& defaultValue = {}) const = 0;
    virtual void set(const QString& key, const QVariant& value) = 0;
    virtual void onChange(const QString& key,
                          std::function<void(const QVariant&)> handler) = 0;
    virtual void remove(const QString& key) = 0;
};
```

命名空间：

| 类别 | 前缀 |
|---|---|
| 全局 | `general.*`(`general.language`、`general.theme`) |
| 插件配置 | `plugins.<plugin_id>.*` |
| 用户隐私 | `privacy.*`（永远为 false，占位字段） |

**命名空间隔离**：插件只能写入自身的 `plugins.<id>.*` 命名空间，Host 在 set 时校验。

**加密字段**（透明处理）：在 manifest 中声明 `encrypted_settings` 的键，Host
在写盘时自动执行 AES-256-GCM 加密，读取时自动解密，对插件代码完全透明：

```cpp
// manifest 声明 "plugins.aura_locker.paired_device_identifier" 为加密字段
ctx.host->settings().set("plugins.aura_locker.paired_device_identifier", deviceId);
// 落盘为密文,代码中拿到的是明文
QString id = ctx.host->settings().get(
    "plugins.aura_locker.paired_device_identifier").toString();
```

详见 [07-privacy-security.md](07-privacy-security.md)。

---

## TrayService

```cpp
class TrayService {
public:
    enum class IconState { Normal, Locked, Stretching };

    virtual void showToast(const QString& title,
                           const QString& message,
                           int timeoutMs = 5000) = 0;
    virtual void setIconState(IconState state) = 0;
    virtual void setTooltip(const QString& text) = 0;
    virtual void refreshPluginMenu(const QString& pluginId) = 0;
};
```

托盘菜单项通过 `TrayMenuContributor` 提供，详见
[04-plugin-spec.md](04-plugin-spec.md)。

`refreshPluginMenu` 用于动态刷新菜单（例如"Aura: ON"与"Aura: OFF"状态切换）。

---

## CryptoService

```cpp
class CryptoService {
public:
    virtual QByteArray encryptString(const QString& plaintext) = 0;
    virtual QString decryptString(const QByteArray& ciphertext) = 0;
};
```

**安全隔离设计**:

- Host 内部的 `Keyring` 与 `master_key` **永不**暴露给插件
- 每个 `HostServices` 实例的 `crypto()` 通过 `HKDF(master_key, plugin_id)`
  派生该插件专属密钥
- 恶意插件 B 即使获得插件 A 的密文，也无法解密（HKDF 派生密钥不同，AES-GCM
  tag 校验失败）
- 即便插件具有 `database-read` 权限并读取了其他插件的 BLOB，也无法还原明文

与 Database 配合使用：

```cpp
// 写入
QByteArray ct = ctx.host->crypto().encryptString(windowTitle);
ctx.host->database()->exec(
    "INSERT INTO screen_time_session (title_enc) VALUES (:t)",
    {{":t", ct}});

// 读取
auto rows = ctx.host->database()->query("SELECT title_enc FROM screen_time_session");
for (auto& row : rows) {
    QString pt = ctx.host->crypto().decryptString(row["title_enc"].toByteArray());
}
```

Settings 的自动加密无需调用 CryptoService，由 Host 在底层处理。

---

## Database

```cpp
class Database {
public:
    virtual bool exec(const QString& sql, const QVariantMap& params = {}) = 0;
    virtual QList<QVariantMap> query(const QString& sql,
                                     const QVariantMap& params = {}) = 0;
    virtual bool transaction() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;
};
```

- SQLite + WAL 模式（支持并发读）
- 同步级别 `NORMAL`（性能与安全兼顾）
- 外键 ON,5 秒超时
- **表前缀隔离**：每个插件使用 `<plugin_id>_*` 前缀
  - Screen Time:`screen_time_*`
  - Aura:`aura_*`
  - Rhythm:`rhythm_*`
  - Host 自用：`host_*`

---

## QmlService（可选 QObject 注入）

EventBus 已能处理绝大多数 UI 与 C++ 之间的通信。适用 QmlService 的场景包括：

- 命令参数超过 5 个，或需要 IDE 自动补全
- 需要同步返回值
- 大量属性绑定

可通过 QmlService 注册一个 `Q_INVOKABLE` 控制器：

```cpp
Result<void, std::string> onLoad(const PluginContext& ctx) override {
    m_controller = new AuraController(this);
    ctx.host->qml()->registerContextProperty("auraController", m_controller);
    // QML: auraController.applyThreshold(-70)
    return {};
}
```

**沙箱**:Host 为每个插件创建独立的 `QQmlContext`。`registerContextProperty`
注册的对象**仅对该插件的 QML 可见**。onUnload 时 Host 自动 `deleteLater()`
所有注册对象。

---

## 平台监听服务

两个被动事件流服务，由 `PlatformBackend` 持有：

### WindowMonitorService

```cpp
class WindowMonitorService : public QObject {
signals:
    void activeWindowChanged(qint64 pid,
                             const QString& processName,
                             const QString& windowTitle);
};
```

- Windows 实现：`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`（系统推送，非轮询）
- macOS 实现：v1.1+(`NSWorkspaceDidActivateApplicationNotification`)

### InputMonitorService

```cpp
class InputMonitorService : public QObject {
signals:
    void userIdleStateChanged(bool idle);  // edge-triggered
};
```

- Windows 实现：`SetWindowsHookExW(WH_KEYBOARD_LL + WH_MOUSE_LL)`，始终通过
  `CallNextHookEx` 转发（并非 keylogger，不记录键值）
- macOS:v1.1+(`CGEventTap`，需"辅助功能"权限）

通过 HostServices 访问（可能为 nullptr):

```cpp
auto* wm = ctx.host->windowMonitor();   // nullptr 表示该平台不支持
if (wm) connect(wm, &WindowMonitorService::activeWindowChanged, ...);
```

---

## 服务生命周期

```text
[HostCore::bootstrap()]
   Paths::ensureDirs()
   Logger::initialize()
   Keyring::getOrCreateMasterKey()         # Host 内部,不暴露给插件
   Settings::load()
   EventBus::wire()
   QmlService::initialize()
   TrayService::initialize()
   Database::open()                        # M2+
      ▼
[PluginManager::loadAll()]
   为每个插件构造带专属 CryptoService 的 HostServices
   调 onLoad(ctx) → 插件订阅 EventBus、注册 Contributor
      ▼
[QCoreApplication::exec()]
      ▼ (退出时,反序执行)
[unloadAll → EventBus::shutdown → Database::checkpoint
   → Keyring::clearMemory → Logger::flush]
```

---

## 进一步阅读

- 插件 ABI 与 Contributor 接口：[04-plugin-spec.md](04-plugin-spec.md)
- 加密细节与 HKDF 派生：[07-privacy-security.md](07-privacy-security.md)
- 平台后端 API:[06-platform-support.md](06-platform-support.md)
