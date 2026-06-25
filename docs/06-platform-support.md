# 06 — 平台支持

Margin 当前的 Windows / macOS 支持矩阵、平台 API 对照、权限要求。

---

## 平台支持矩阵

| 平台 | 版本 | 架构 | 状态 |
|---|---|---|---|
| **Windows 10** | 1809+(10.0.17763+) | x64 | ✅ v0.1.0 已发布 |
| **Windows 11** | 任意 | x64 / arm64 | ✅ x64 已发布;arm64 v1.1+ |
| **macOS** | 11 Big Sur+ | Intel + Apple Silicon | ⏳ 平台后端开发中,v1.1 |
| Linux | Ubuntu 22.04+ / Fedora 36+ | x64 | ⏳ v1.1+ |

**不支持**:

- Windows 8 / 8.1 / 7(已 EOL)
- macOS 10.15 及以下(已 EOL)
- 32 位 x86(资源不足以承担 Qt 6)

---

## 功能 → API 对照

| 功能 | Windows API | macOS API |
|---|---|---|
| 系统托盘 | `QSystemTrayIcon` | `QSystemTrayIcon`(`NSStatusItem`) |
| OS 密钥环 | `CryptProtectData` / `CryptUnprotectData` (DPAPI) | `SecItemAdd` / `SecItemCopyMatching` (Keychain) |
| AES-256-GCM | OpenSSL(随 vcpkg) | OpenSSL(随 vcpkg) |
| 锁屏 | `LockWorkStation()` | AppleScript(临时方案,v1.1+ 评估更快 API) |
| 前台应用监听 | `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` | `NSWorkspaceDidActivateApplicationNotification` |
| 全局键鼠闲置检测 | `SetWindowsHookExW(WH_KEYBOARD_LL / WH_MOUSE_LL)` | `CGEventTapCreateForAnnotatedSession` |
| 蓝牙 LE 邻近监控 | `BluetoothLEAdvertisementWatcher`(C++/WinRT) | `CBCentralManager scanForPeripherals:`(CoreBluetooth) |
| 开机自启 | 注册表 `HKCU\...\Run` | `SMLoginItemSetEnabled` 或 LaunchAgents plist(v1.1) |
| 暗色窗口标题栏 | `DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` | `[NSWindow setAppearance:NSAppearanceNameDarkAqua]`(v1.1) |

---

## 关键决策

### 蓝牙 LE 邻近监控——不用 Qt Bluetooth

Qt 没有连续 advertisement 监听封装(`QBluetoothDeviceDiscoveryAgent` 是
一次性 discovery),且 GATT 连接方式有 4 个不可接受的问题:

1. 持续 GATT 连接耗电(iPhone 一晚掉 ~10%)
2. iOS LE Privacy 每 15 分钟轮换 MAC,绑定 MAC 的连接随时断
3. iOS 拒绝非 paired host 的 connect(配对设备也 30%+ 失败率)
4. 重新连接慢(2-5 秒),锁屏判定延迟

**Margin 的方案**:被动监听 advertisement 广播包。每个广播包自带 RSSI +
发射功率,**完全无需连接**。OS 在配对时拿到 IRK(Identity Resolving Key),
之后即使 iOS 换 MAC 也能解析回稳定 ID。

### 平台代码隔离

`#ifdef Q_OS_WIN` / `#ifdef Q_OS_MACOS` 只允许出现在
`src/host/platform/<os>/` 下。共享代码不含平台宏。

```cpp
// ✅ 正确
// src/host/platform/windows/PlatformBackendWin.cpp
#ifdef _WIN32
#include <windows.h>
// Win 实现
#endif

// ❌ 错误
// src/host/core/HostCore.cpp
#include <windows.h>   // Mac 编译失败
```

`PlatformBackend` 是统一接口,工厂方法 `create()` 返回当前平台实现。

---

## 权限要求

### Windows

| 权限 | 需要 | 备注 |
|---|---|---|
| 管理员 | ❌ | 用户级安装,所有 API 普通用户 |
| Bluetooth LE | ❌ | Win 10 自动允许扫描已配对设备 |
| 辅助功能 | ❌ | LL hook 是用户级 |
| 摄像头 / 麦克风 | ❌ | Margin 不使用 |

**结论**:Windows 上零权限弹窗。

### macOS

| 权限 | 需要 | 首次触发 |
|---|---|---|
| 管理员 | ❌ | 用户级安装 |
| Bluetooth | ✅ | v1.1 蓝牙落地时 |
| 辅助功能 | ✅ | v1.1 闲置检测时,需用户在"系统偏好 → 隐私"勾选 |
| Keychain | ✅ | 首次写入 master key,系统弹密码框,允许一次后记忆 |

---

## macOS 后端进度(v1.1)

Phase 1 MVP(v0.1.0)只完成了 Windows 后端。macOS 后端正在开发中:

| 后端 | 状态 |
|---|---|
| Keychain | 待落地(对应 Win DPAPI) |
| 锁屏(AppleScript) | 待落地 |
| CoreBluetooth 邻近监控 | 待落地 |
| NSWorkspace 前台应用监听 | 待落地 |
| CGEventTap 闲置检测 | 待落地 |
| LaunchAgents 开机自启 | 待落地 |

macOS 后端落地后,Phase 1 已发布的 Windows 功能会同步可用。

---

## Linux 进度(v1.1+)

Linux 延后到 v1.1+。关键阻塞:

- BLE advertisement 监听需走 BlueZ D-Bus,与 Win/Mac 行为差异大
- 桌面环境多样(X11 / Wayland / 不同发行版),窗口监听 API 不统一
- 资源有限,Win/Mac v1.0 稳定后开放

v1.1+ 会优先支持 Ubuntu 22.04 LTS / Fedora 36+,Wayland v1.2+。

---

## 进一步阅读

- 架构与平台隔离原则:[01-architecture.md](01-architecture.md)
- 加密与 OS 密钥环细节:[07-privacy-security.md](07-privacy-security.md)
- 从源码构建(macOS 暂未支持完整跑通):[03-build-from-source.md](03-build-from-source.md)
