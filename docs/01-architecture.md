# 01 — 架构

Margin 是单进程、多插件、事件驱动的桌面应用。本文档解释 Host / Plugin /
EventBus 三层的关系,以及进程的启动与退出时序。

---

## 设计原则

| 原则 | 含义 |
|---|---|
| **plugin-first** | 业务逻辑全部在插件里。Host 只提供基础设施(EventBus、Database、Logger 等),不实现"什么时候锁屏""番茄钟多长"。 |
| **deny-by-default** | 插件能用的能力必须在 `manifest.json` 显式声明,且首次加载时用户授权。未授权的句柄是 `nullptr`。 |
| **platform-isolated** | 平台 API(Win32 / macOS)只在 `src/host/platform/<os>/`,共享代码不含 `#ifdef Q_OS_WIN`。 |
| **one-bus** | 所有跨组件通信走单一 EventBus。插件之间不能直接调用对方的符号。 |
| **100% local** | 零网络调用。所有数据本地存储。 |

---

## 进程模型

```text
┌─────────────────────────────────────────────────────────────┐
│                    Margin.exe(单进程)                       │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              HostCore(主控制器)                       │  │
│  │  EventBus / PluginManager / PlatformBackend /         │  │
│  │  Database / Logger / Settings / Tray / Keyring        │  │
│  └──────────────────────▲───────────────────────────────┘  │
│                          │ Plugin ABI(C++ vtable)           │
│  ┌───────────────────────┴───────────────────────────────┐ │
│  │              Plugin DLLs(动态加载)                    │ │
│  │   hello.dll   aura.dll   screen_time.dll   rhythm.dll │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              UI Layer(QML)                            │  │
│  │   Dashboard Window / Settings Window / System Tray /  │  │
│  │   Toast & Overlay                                     │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

关键决策:

1. **单进程**——Host 与所有插件跑在同一进程。不用子进程隔离(避免 IPC 复杂度)。
2. **DLL 动态加载**——插件用 `LoadLibrary` / `dlopen` 加载,通过 C++ vtable 调用。
3. **EventBus 是唯一通道**——跨插件协作必须通过 EventBus,不能直接调用其他插件符号。
4. **UI 在主线程**——所有 QML 在主线程渲染;后台任务用 `QThread` 或 `QtConcurrent`。

---

## 启动时序

```text
[main.cpp]
   │  QApplication + Qt 初始化
   ▼
[HostCore::bootstrap()]
   1. Paths::ensureDirs()         # config / data / logs / cache 目录
   2. Logger::initialize()        # 必须在 Paths 之后
   3. Keyring::getOrCreateMasterKey()   # OS 密钥环取/建 master key
   4. Settings::load()            # 读 settings.json
   5. EventBus::wire()            # 注册系统主题
   6. PluginManager::scan()       # 扫描 plugins/ 目录 + 解析 manifest
   7. PluginManager::loadAll()    # 按 priority 排序 + 调 onLoad()
   8. SystemTray::show()          # 托盘图标 + 菜单
   9. DashboardWindow::load()     # QML 主面板
   ▼
[QCoreApplication::exec()]
   # 事件循环,插件开始工作
```

时序约束(违反会导致启动失败或数据丢失):

- `Paths::ensureDirs()` 必须最先——Logger 要写 `logs/margin.log`
- `Keyring` 必须在 `Database` 之前——敏感字段加解密需要 master key
- `EventBus` 必须在 `PluginManager` 之前——插件 `onLoad` 时要订阅主题
- `SystemTray` 必须在所有插件 `onLoad` 之后——托盘菜单要各插件的贡献项

---

## 退出时序

```text
[用户点 Quit / 系统关机]
   ▼
1. SystemTray::hide()                  # 立即给视觉反馈
2. PluginManager::unloadAll()          # 反序调 onUnload(),每个 5s 超时
3. EventBus::shutdown()                # 拒绝新 publish,等队列排空(2s 超时)
4. Database::checkpoint()              # SQLite WAL checkpoint + 关句柄
5. Keyring::clearMemory()              # SecureZeroMemory 清零 master key
6. Logger::flush()                     # 写 "[shutdown complete]" + 关文件
   ▼
[QCoreApplication::quit()]
```

验证方法:

```bash
# Windows
tasklist | findstr Margin.exe           # 退出后应返回空
type %LOCALAPPDATA%\Margin\logs\margin.log | findstr "shutdown complete"

# macOS
ps aux | grep Margin                    # 应返回空
tail -1 ~/Library/Logs/Margin/margin.log  # 应输出 [shutdown complete]
```

---

## 三种角色

| 角色 | 职责 | 不能做什么 |
|---|---|---|
| **Host** | 进程入口、生命周期、基础设施服务、平台抽象 | 不实现业务逻辑("什么时候锁屏"等) |
| **Plugin** | 业务逻辑、`manifest.json` 声明权限、EventBus 协作 | 不直接调 OS API(走 PlatformBackend)、不直接访问文件系统(走 HostServices) |
| **User** | 安装 / 卸载、授权权限、配置、查看 / 导出 / 删除数据 | — |

---

## 进一步阅读

- 插件如何写:[04-plugin-spec.md](04-plugin-spec.md)
- 插件能调什么 API:[05-host-services.md](05-host-services.md)
- 隐私与加密细节:[07-privacy-security.md](07-privacy-security.md)
- 平台支持矩阵:[06-platform-support.md](06-platform-support.md)
