# 01 — 架构

Margin 是单进程、多插件、事件驱动的桌面应用。本文档说明 Host、Plugin
与 EventBus 三层之间的关系，以及进程的启动与退出时序。

---

## 设计原则

| 原则 | 含义 |
|---|---|
| **plugin-first** | 业务逻辑全部位于插件中。Host 仅提供基础设施（EventBus、Database、Logger 等），不实现"何时锁屏"或"番茄钟时长"等业务策略 |
| **deny-by-default** | 插件可使用的能力必须在 `manifest.json` 中显式声明，且首次加载时需用户授权。未授权的句柄为 `nullptr` |
| **platform-isolated** | 平台 API(Windows 与 macOS）仅位于 `src/host/platform/<os>/`，共享代码中不含 `#ifdef Q_OS_WIN` |
| **one-bus** | 所有跨组件通信走单一 EventBus。插件之间不能直接调用对方的符号 |
| **100% local** | 零网络调用，所有数据本地存储 |

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

关键决策：

1. **单进程**:Host 与所有插件运行在同一进程中，不采用子进程隔离（以避免 IPC 引入的复杂度）。
2. **DLL 动态加载**：插件通过 `LoadLibrary` 或 `dlopen` 加载，经 C++ vtable 调用。
3. **EventBus 为唯一通道**：跨插件协作必须通过 EventBus，不允许直接调用其他插件的符号。
4. **UI 在主线程**：所有 QML 在主线程渲染；后台任务使用 `QThread` 或 `QtConcurrent`。

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

时序约束（违反将导致启动失败或数据丢失）:

- `Paths::ensureDirs()` 必须最先执行——Logger 需要写入 `logs/margin.log`
- `Keyring` 必须在 `Database` 之前完成——敏感字段加解密依赖 master key
- `EventBus` 必须在 `PluginManager` 之前就绪——插件 `onLoad` 时需订阅主题
- `SystemTray` 必须在所有插件 `onLoad` 完成之后创建——托盘菜单需要各插件贡献的菜单项

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

验证方法：

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

| 角色 | 职责 | 限制 |
|---|---|---|
| **Host** | 进程入口、生命周期管理、基础设施服务与平台抽象 | 不实现业务逻辑（"何时锁屏"等） |
| **Plugin** | 业务逻辑实现、在 `manifest.json` 中声明权限、通过 EventBus 协作 | 不直接调用 OS API（需走 PlatformBackend)、不直接访问文件系统（需走 HostServices) |
| **User** | 安装与卸载、权限授权、配置、查看与导出或删除数据 | — |

---

## 进一步阅读

- 插件编写方式：[04-plugin-spec.md](04-plugin-spec.md)
- 插件可调用的 API:[05-host-services.md](05-host-services.md)
- 隐私与加密细节：[07-privacy-security.md](07-privacy-security.md)
- 平台支持矩阵：[06-platform-support.md](06-platform-support.md)
