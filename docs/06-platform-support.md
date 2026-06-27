# 06 — 平台支持

Margin 当前的 Windows 与 macOS 支持矩阵、平台 API 对照及权限要求。

---

## 平台支持矩阵

| 平台 | 版本 | 架构 | 状态 |
|---|---|---|---|
| **Windows 10** | 1809+(10.0.17763+) | x64 | v0.1.0 已发布 |
| **Windows 11** | 任意 | x64 与 arm64 | x64 已发布；arm64 计划于 v1.1+ 提供 |
| **macOS** | 11 Big Sur+ | Intel 与 Apple Silicon | 平台后端开发中，计划于 v1.1 版本提供 |
| Linux | Ubuntu 22.04+ 或 Fedora 36+ | x64 | 计划于 v1.1+ 版本提供 |

**不支持**:

- Windows 8、8.1 与 7（已 EOL)
- macOS 10.15 及以下（已 EOL)
- 32 位 x86（资源不足以承载 Qt 6)

---

## 功能与 API 对照

| 功能 | Windows API | macOS API |
|---|---|---|
| 系统托盘 | `QSystemTrayIcon` | `QSystemTrayIcon`(`NSStatusItem`) |
| OS 密钥环 | `CryptProtectData` 与 `CryptUnprotectData`(DPAPI) | `SecItemAdd` 与 `SecItemCopyMatching`(Keychain) |
| AES-256-GCM | Windows CNG（BCrypt，系统自带） | macOS Keychain / CommonCrypto（v1.1 跟进） |
| 锁屏 | `LockWorkStation()` | AppleScript（临时方案，v1.1+ 评估更优 API) |
| 前台应用监听 | `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` | `NSWorkspaceDidActivateApplicationNotification` |
| 全局键鼠闲置检测 | `SetWindowsHookExW(WH_KEYBOARD_LL 与 WH_MOUSE_LL)` | `CGEventTapCreateForAnnotatedSession` |
| 蓝牙 LE 邻近监控 | `BluetoothLEAdvertisementWatcher`(C++/WinRT) | `CBCentralManager scanForPeripherals:`(CoreBluetooth) |
| 开机自启 | 注册表 `HKCU\...\Run` | `SMLoginItemSetEnabled` 或 LaunchAgents plist(v1.1) |
| 暗色窗口标题栏 | `DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` | `[NSWindow setAppearance:NSAppearanceNameDarkAqua]`(v1.1) |

---

## 关键决策

### 蓝牙 LE 邻近监控——未采用 Qt Bluetooth

Qt 未提供连续 advertisement 监听封装（`QBluetoothDeviceDiscoveryAgent` 仅支持
一次性 discovery)，且 GATT 连接方式存在 4 个不可接受的问题：

1. 持续 GATT 连接耗电较高（iPhone 一晚掉电约 10%)
2. iOS LE Privacy 每 15 分钟轮换 MAC，绑定 MAC 的连接随时可能断开
3. iOS 拒绝非 paired host 的 connect（即便配对设备仍有 30%+ 失败率）
4. 重新连接较慢（2–5 秒），导致锁屏判定延迟

**Margin 的方案**：被动监听 advertisement 广播包。每个广播包自带 RSSI 与
发射功率，**完全无需连接**。OS 在配对时获取 IRK(Identity Resolving Key),
此后即使 iOS 更换 MAC 也能解析回稳定 ID。

### 平台代码隔离

`#ifdef Q_OS_WIN` 与 `#ifdef Q_OS_MACOS` 仅允许出现在
`src/host/platform/<os>/` 目录下，共享代码中不含平台宏。

```cpp
// 正确
// src/host/platform/windows/PlatformBackendWin.cpp
#ifdef _WIN32
#include <windows.h>
// Windows 实现
#endif

// 错误
// src/host/core/HostCore.cpp
#include <windows.h>   // macOS 编译失败
```

`PlatformBackend` 为统一接口，工厂方法 `create()` 返回当前平台的实现。

---

## 权限要求

### Windows

| 权限 | 需要 | 备注 |
|---|---|---|
| 管理员 | 否 | 用户级安装，所有 API 均为普通用户权限 |
| Bluetooth LE | 否 | Windows 10 自动允许扫描已配对设备 |
| 辅助功能 | 否 | LL hook 为用户级 |
| 摄像头与麦克风 | 否 | Margin 不使用 |

**结论**:Windows 上无任何权限弹窗。

### macOS

| 权限 | 需要 | 首次触发 |
|---|---|---|
| 管理员 | 否 | 用户级安装 |
| Bluetooth | 是 | v1.1 蓝牙功能落地时 |
| 辅助功能 | 是 | v1.1 闲置检测时，需用户在"系统偏好 → 隐私"中勾选 |
| Keychain | 是 | 首次写入 master key 时，系统弹出密码框，允许一次后记忆 |

---

## macOS 后端进度（v1.1)

Phase 1 MVP(v0.1.0）仅完成 Windows 后端。macOS 后端正在开发中，各项进度如下：

| 后端 | 状态 |
|---|---|
| Keychain（对应 Windows DPAPI) | 尚未实现 |
| 锁屏（AppleScript) | 尚未实现 |
| CoreBluetooth 邻近监控 | 尚未实现 |
| NSWorkspace 前台应用监听 | 尚未实现 |
| CGEventTap 闲置检测 | 尚未实现 |
| LaunchAgents 开机自启 | 尚未实现 |

macOS 后端落地后，Phase 1 已发布的 Windows 功能将同步可用。

---

## Linux 进度（v1.1+)

Linux 延后至 v1.1+。主要阻塞原因：

- BLE advertisement 监听需通过 BlueZ D-Bus，与 Windows 及 macOS 行为差异较大
- 桌面环境多样（X11、Wayland 以及不同发行版），窗口监听 API 不统一
- 资源有限，需在 Windows 与 macOS v1.0 稳定后开放

v1.1+ 将优先支持 Ubuntu 22.04 LTS 与 Fedora 36+,Wayland 支持计划于 v1.2+ 提供。

---

## 进一步阅读

- 架构与平台隔离原则：[01-architecture.md](01-architecture.md)
- 加密与 OS 密钥环细节：[07-privacy-security.md](07-privacy-security.md)
- 从源码构建（macOS 暂未完整支持）:[03-build-from-source.md](03-build-from-source.md)
