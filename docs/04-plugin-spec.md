# 04 — 插件规范

Margin 的扩展能力通过插件实现。每个插件是一个动态库（`.dll` 或 `.dylib`),
通过 C ABI 入口被 Host 加载，在 Host 提供的服务上实现业务逻辑。

本文档说明插件 ABI、manifest、权限模型与最小示例。Host 提供的服务
API 详见 [05-host-services.md](05-host-services.md)。

---

## 整体结构

一个插件由 4 部分组成：

```text
src/plugins/<your_plugin>/
├── manifest.json          # 元数据 + 权限声明(嵌入 Qt resource)
├── entry.cpp              # C 入口函数 margin_plugin_entry()
├── <YourPlugin>.h/.cpp    # 实现 PluginInterface + 可选 Contributor
├── qml/                   # 插件的 QML 文件(Tab、设置页、Toast)
├── icons/                 # SVG 或 PNG 资源
└── i18n/                  # 翻译文件 .ts(可选)
```

---

## manifest.json

每个插件的动态库中必须通过 Qt resource 嵌入一个 `manifest.json`，用于描述
元数据、权限与入口点。

```json
{
  "manifest_version": 1,
  "id": "aura_locker",
  "name": "Aura Locker",
  "version": "0.1.0",
  "min_host_version": "0.1.0",
  "abi_version": "0.2.0",
  "entry_point": "margin_plugin_entry",
  "priority": 20,
  "permissions": ["bluetooth-scan", "system-lock"],
  "encrypted_settings": [
    "plugins.aura_locker.paired_device_identifier"
  ],
  "events": {
    "publish": ["margin.aura_locker.away", "margin.aura_locker.back"],
    "subscribe": []
  },
  "ui_contributions": ["TrayMenuContributor", "DashboardTabContributor"]
}
```

### 关键字段

| 字段 | 说明 |
|---|---|
| `id` | snake_case，作为 EventBus 主题前缀 |
| `abi_version` | 与 Host 编译时 `MARGIN_ABI_VERSION` 严格相等 |
| `entry_point` | C 函数符号名，详见下文 |
| `priority` | 加载顺序，数字小者优先（默认 100) |
| `permissions` | 详见"权限模型"章节 |
| `encrypted_settings` | 需 Host 透明加密的 Settings 键全路径，需匹配 `^plugins\.<id>\.[a-z][a-z0-9_]*$` |
| `events.publish` | 发布的主题，需匹配 `^margin\.<id>\.` 前缀 |
| `events.subscribe` | 订阅的主题 |
| `ui_contributions` | 实现的 Contributor 接口名 |

校验失败时插件不予加载，并输出 WARN 级别日志。

---

## C++ ABI

### PluginInterface(3 个钩子）

```cpp
namespace Margin {

class PluginInterface {
public:
    virtual ~PluginInterface() = default;

    virtual std::string id() const = 0;
    virtual std::string version() const = 0;

    virtual Result<void, std::string> onLoad(const PluginContext& ctx) = 0;
    virtual void onConfigChange(const QJsonObject& newConfig) = 0;
    virtual void onUnload() = 0;

    // 可选 Contributor 访问器,默认返回 nullptr
    virtual TrayMenuContributor*     asTrayMenu()     { return nullptr; }
    virtual SettingsPageContributor* asSettingsPage() { return nullptr; }
    virtual DashboardTabContributor* asDashboardTab() { return nullptr; }
    virtual OverlayContributor*      asOverlay()      { return nullptr; }
};

} // namespace Margin
```

插件在自身的翻译单元（Translation Unit, TU）内 `return this;`（通过 `static_cast` 在编译期解析多重继承偏移，不依赖 RTTI，因为跨 DLL 边界的 `dynamic_cast` 不可靠）。

### PluginContext(onLoad 注入）

```cpp
struct PluginContext {
    std::string id;
    std::string version;
    std::vector<std::string> grantedPermissions;
    HostServices* host = nullptr;             // 服务总线句柄
    QObject* subscriberIdentity = nullptr;    // EventBus 订阅身份
};
```

`host->crypto()` 返回的 `CryptoService` 实例已用当前插件 id 派生专属
HKDF 密钥，因此恶意插件无法解密其他插件的密文。

### C 入口（跨编译器稳定）

```cpp
// entry.cpp
extern "C" {
    __declspec(dllexport)  // Windows
    // __attribute__((visibility("default")))  // macOS
    Margin::PluginInterface* margin_plugin_entry() {
        static YourPlugin instance;
        return &instance;
    }
}
```

符号名必须与 manifest 的 `entry_point` 一致。

---

## 加载机制

```text
Host 启动
   ▼
PluginManager::scan()
   ├─ 扫描 <install>/plugins/  (官方;Windows: %LOCALAPPDATA%\Margin\current\<ver>\plugins\)
   └─ 扫描 <AppData>/plugins/  (用户级;Windows: %APPDATA%\Margin\plugins\)
   ▼
对每个 *.dll / *.dylib:
   1. 读 embedded manifest.json(Qt resource)
   2. 校验 schema + abi_version + min_host_version
   3. 校验 permissions 是否在白名单
   4. LoadLibrary / dlopen
   5. 调 entry_point() 拿 PluginInterface*
   6. 调 onLoad(ctx)
   7. 通过访问器查询 Contributor(asTrayMenu、asDashboardTab 等)
   ▼
插件进入 Active 状态
```

**核心原则**：单个插件失败不影响其他插件，Host 始终能启动。

加载顺序由 `priority` 决定，卸载时按反序执行：

| priority | 用途 |
|---|---|
| 10 | 系统级（Hello 示例） |
| 20 | Aura Locker（其他插件可能订阅其事件） |
| 30 | Screen Time |
| 40 | Rhythm（订阅 Aura 事件） |
| 100 | 默认（第三方插件） |

---

## 权限模型

**deny by default**。插件可使用的能力必须在 manifest 中声明，并在首次加载时
经用户授权，未授权的句柄为 `nullptr`。

| 权限 | 含义 |
|---|---|
| `database-read`、`database-write` | 访问 SQLite |
| `bluetooth-scan` | BLE 邻近扫描（Aura) |
| `system-lock` | 锁屏（Aura) |
| `active-window-monitor` | 监听前台应用切换（Screen Time) |
| `input-monitor` | 全局键鼠闲置检测（Screen Time 与 Rhythm) |
| `notification-show` | toast 通知 |
| `overlay-fullscreen` | 全屏遮罩 |

每次插件申请权限与每次使用权限均写入 `permissions.log`，用户可在
"设置 → Privacy"中查看。

---

## UI 集成（4 个 Contributor)

插件通过实现以下**可选**多继承接口，在 Host UI 中获得展示位置：

| 接口 | 展示位置 |
|---|---|
| `TrayMenuContributor` | 系统托盘右键菜单 |
| `SettingsPageContributor` | "设置中心 → 插件名"标签页 |
| `DashboardTabContributor` | 主面板 Tab 导航 |
| `OverlayContributor` | 全屏遮罩（需 `overlay-fullscreen` 权限） |

最小实现（仅贡献托盘菜单）:

```cpp
class HelloPlugin
    : public PluginInterface,
      public TrayMenuContributor {
public:
    Result<void, std::string> onLoad(const PluginContext& ctx) override {
        m_ctx = ctx;
        return {};
    }
    void onConfigChange(const QJsonObject&) override {}
    void onUnload() override {}

    TrayMenuContributor* asTrayMenu() override { return this; }

    QList<TrayItem> contributeTrayItems() override {
        return { TrayItem{ .id = "say_hello", .label = "Say Hello" } };
    }

    void onTrayItemClicked(const std::string& id) override {
        if (id == "say_hello") {
            m_ctx.host->eventBus().publish(
                "margin.hello.ping", QJsonObject{{"msg", "hello"}});
        }
    }

private:
    PluginContext m_ctx;
};
```

---

## EventBus 跨插件协作

EventBus 为 fire-and-forget pub/sub 模型，所有跨插件通信均通过其完成。

```cpp
// 发布
ctx.host->eventBus().publish("margin.aura_locker.away", QJsonObject{
    {"device", "iPhone"}, {"rssi", -75}
});

// 订阅
ctx.host->eventBus().subscribe(
    "margin.aura_locker.away",
    [this](const QJsonObject& payload) {
        // 处理 away 事件
    },
    ctx.subscriberIdentity  // PluginManager 在 unload 时按身份清理
);
```

主题命名约定：`margin.<plugin_id>.<event_name>`。子命名空间约定如下：

| 子段 | 方向 |
|---|---|
| `margin.<plugin>.<event>` | 普通事件（默认） |
| `margin.<plugin>.command.<verb>` | UI → 插件 C++ |
| `margin.<plugin>.state.<name>` | 插件 C++ → UI 状态回执 |

通配符 `*` 匹配单层，`#` 匹配多层。

---

## 最小完整插件（Hello)

参考实现位于仓库 `src/plugins/hello/`。完整文件清单：

```text
src/plugins/hello/
├── CMakeLists.txt
├── manifest.json
├── hello.qrc
├── entry.cpp
├── HelloPlugin.h
├── HelloPlugin.cpp
└── icons/tray-menu.svg
```

CMakeLists 关键片段：

```cmake
add_library(hello SHARED entry.cpp HelloPlugin.cpp)
target_link_libraries(hello PRIVATE margin_plugin_api Qt::Core)
install(TARGETS hello LIBRARY DESTINATION plugins)
```

启动 Margin 后，托盘菜单中将出现"Say Hello"，点击后日志输出
`Hello from HelloPlugin`，并通过 EventBus 发布 `margin.hello.ping` 事件。

---

## 进一步阅读

- Host 提供的 API:[05-host-services.md](05-host-services.md)
- 权限审计与加密：[07-privacy-security.md](07-privacy-security.md)
- 平台 API:[06-platform-support.md](06-platform-support.md)
- 贡献流程：[CONTRIBUTING.md](CONTRIBUTING.md)
