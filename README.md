# Margin / 息间

<p align="center">
  <img src="docs/assets/tray.svg" width="72" alt="Margin 托盘图标">
</p>

<p align="center">
  光标留白，是"息间"；托盘一隅，是"Margin"。<br>
  余白养神，边界护心。
</p>

<p align="center">
  <em>The cursor's pause is "息间"; the tray's corner is "Margin".<br>
  Where margins nurture the mind, and boundaries guard the heart.</em>
</p>

<p align="center">
  <img src="docs/assets/dashboard_home.png" width="780" alt="Margin 主面板首页">
</p>

---

一款常驻系统托盘的轻量级数字健康辅助工具，专为开发者和高频电脑使用者设计。采用 Host + Plugin + EventBus 架构，承诺**完全本地化运行、零网络访问、开源透明**。

---

## What is this (English short)

Margin is a cross-platform Qt 6 / C++ / QML desktop companion that lives in the
system tray. It is built around three pillars of digital health:

- **Aura Locker** — auto-lock the workstation when a paired Bluetooth device
  (phone, watch, headphone) moves out of range.
- **Screen Time Tracker** — passive foreground-app duration logging with
  field-level AES-256-GCM encryption for sensitive window titles.
- **Rhythm & Health** — pomodoro timer with guided stretch breaks; pauses when
  Aura reports the user stepped away.

100% on-device. Zero network calls, zero telemetry, no accounts. Source under
LGPL-3.0-or-later. Windows 10 1809+ supported today; macOS backend is in
progress for v1.1.

---

## 这是什么

Margin 采用插件化架构，主要围绕久坐健康、注意力专注以及隐私保护三大核心场景提供服务。

**当前版本**：已发布 v0.1.0 MVP 版本，Windows 端核心功能已完整上线；macOS 端后端适配正在进行中，预计在 v1.1 版本提供支持。

### Aura Locker — 离座即锁屏

基于蓝牙 RSSI 邻近检测，无需 GATT 连接，亦无需持续轮询。配对设备离开
5 米 + 30 秒后自动锁屏；回座后设有 60 秒冷却时间（Cooldown），在此期间不会重复锁屏，避免频繁打断工作节奏。

> 配对设置位于"设置中心 → Aura"，截图见 [设置中心](#设置中心） 章节。

### Screen Time Tracker — 不打扰的时长统计

<p align="center">
  <img src="docs/assets/app_time_stats.png" width="640" alt="屏幕时间统计">
</p>

通过系统事件（Windows 下的 `SetWinEventHook`）**被动**接收前台应用切换通知，无需轮询，绝非键盘记录器（Keylogger），保障系统性能与安全。针对敏感数据（如窗口标题 `window_title`）采用 AES-256-GCM 加密存储，而应用名称（`app_name`）及分类（`category`）等非敏感字段以明文保存，兼顾隐私安全与本地数据统计分析。

### Rhythm & Health — 番茄钟 + 颈椎操引导

<p align="center">
  <img src="docs/assets/health_rhythm_timer.png" width="640" alt="番茄钟计时">
</p>

采用经典的番茄工作法（默认工作 25 分钟，休息 5 分钟），并与 Aura 模块联动：检测到离座时自动暂停，回座后继续计时。休息时段将自动弹出**颈椎操引导窗口**，提供 8 节动作指引、倒计时以及文字示范：

<p align="center">
  <img src="docs/assets/neck_stretch_exercise.png" width="640" alt="颈椎操引导窗口">
</p>

---

## 隐私承诺

| 维度 | 承诺 |
|---|---|
| **网络** | **零网络连接**——支持通过 Wireshark 或 Little Snitch 等网络分析工具验证。绝无任何后台心跳、更新检查或崩溃日志上报行为 |
| **遥测** | **零收集**——不收集任何形式的使用数据，不上报崩溃，所有日志仅在本地保存 |
| **账号** | **无需注册**——无登录、无 Token 校验，开箱即用 |
| **加密** | 敏感数据采用 AES-256-GCM 加密，密钥托管在系统原生密钥环中（Windows DPAPI / macOS Keychain） |
| **数据所有权** | 数据完全归用户所有，支持随时一键导出（JSON / CSV 格式）或彻底删除 |

详细方案见 [docs/07-privacy-security.md](docs/07-privacy-security.md)。

---

## 设置中心

所有配置均集中于本地设置中心。**没有所谓的云同步开关**——因为根本不存在云端存储。
提供权限审计日志，直观可视化地展示每个插件申请及使用过的系统权限。

<p align="center">
  <img src="docs/assets/settings_about.png" width="640" alt="设置中心 — 关于">
</p>

---

## 运行边界

| 维度 | 要求 |
|---|---|
| **Windows** | Windows 10 1809+(10.0.17763+),x64 架构 |
| **macOS** | macOS 11 Big Sur+,Universal build(macOS 后端开发中，计划于 v1.1 版本提供） |
| **Linux** | v1.1+ 版本起提供支持 |
| **安装权限** | **无需管理员或 sudo**（用户级安装，与 Chrome 一致） |
| **磁盘占用** | 安装体积 < 80 MB，运行时数据 < 50 MB |
| **内存** | 常驻内存 < 80 MB |
| **CPU** | 空闲态 < 1% |
| **网络** | **零** |
| **遥测** | **零** |

**安装位置**:

- Windows:`%LOCALAPPDATA%\Margin\`（用户级）
- macOS:`~/Applications/Margin.app/`（用户级）

---

## 安装

预编译版本发布在 GitHub Releases。Windows 用户可选择下载 `.exe` 安装包（基于 NSIS 制作，支持用户级免管理员安装）或免安装的绿色版（解压即可使用）。

详细安装步骤与首次启动引导请参阅 [docs/02-install.md](docs/02-install.md)。

---

## 从源码构建

面向贡献者及希望自行验证的用户。需要 Qt 6.7+、CMake 3.21+、vcpkg、Visual Studio 2022(Windows）或 Xcode 14+(macOS)。

完整步骤参阅 [docs/03-build-from-source.md](docs/03-build-from-source.md)。

---

## 文档导航

| 编号 | 文档 | 主题 |
|---|---|---|
| 01 | [docs/01-architecture.md](docs/01-architecture.md) | 整体架构与启动、退出时序 |
| 02 | [docs/02-install.md](docs/02-install.md) | 安装与首次运行 |
| 03 | [docs/03-build-from-source.md](docs/03-build-from-source.md) | 从源码构建 |
| 04 | [docs/04-plugin-spec.md](docs/04-plugin-spec.md) | 插件 ABI、manifest 与权限模型 |
| 05 | [docs/05-host-services.md](docs/05-host-services.md) | Host 服务 API 参考 |
| 06 | [docs/06-platform-support.md](docs/06-platform-support.md) | Windows 与 macOS 平台支持矩阵 |
| 07 | [docs/07-privacy-security.md](docs/07-privacy-security.md) | 隐私承诺、加密方案与威胁模型 |
| 09 | [docs/09-testing.md](docs/09-testing.md) | 测试策略与本地验证流程 |
| - | [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) | 贡献指南 |

---

## 贡献

欢迎通过 GitHub Pull Request 贡献代码、文档或问题反馈。

- 贡献流程、commit 规范与测试要求：[docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)
- 报告 bug 或提交建议：[GitHub Issues](https://github.com/U13tor/Margin/issues)

---

## License

LGPL-3.0-or-later。每个 `.h` 与 `.cpp` 文件头均含
`// SPDX-License-Identifier: LGPL-3.0-or-later`。完整协议文本见仓库根级
[LICENSE](LICENSE)。

---

## 联系

- 仓库：<https://github.com/U13tor/Margin>
- 问题反馈：<https://github.com/U13tor/Margin/issues>
