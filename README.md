# Margin / 息间

<p align="center">
  <img src="docs/assets/tray.svg" width="72" alt="Margin">
</p>

<p align="center">
  光标留白，是"息间"；托盘一隅，是"Margin"。<br>
  余白养神，边界护心。
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-LGPL--3.0--or--later-blue.svg" alt="License"></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%201809+-0078D4.svg?logo=windows" alt="Platform">
  <img src="https://img.shields.io/badge/Qt-6.7+-41CD52.svg?logo=qt&logoColor=white" alt="Qt 6">
  <img src="https://img.shields.io/badge/network-ZERO-critical.svg" alt="Zero Network">
</p>

<p align="center">
  <em>A privacy-first digital wellness companion that lives in your system tray.<br>
  100% on-device · Zero network calls · Zero telemetry · Open source.</em>
</p>

<p align="center">
  <img src="docs/assets/dashboard_home.png" width="780" alt="Margin 主面板首页">
</p>

---

常驻系统托盘的轻量级数字健康工具，专为开发者和高频电脑使用者设计。采用 Host + Plugin + EventBus 插件化架构，**完全本地运行、零网络访问、开源透明**。

## 核心特性

### 🔒 Aura Locker — 离座即锁屏

基于蓝牙 RSSI 邻近检测，无需 GATT 连接。配对设备（手机、手表、耳机）离开约 5 米超过 30 秒后自动锁屏，回座后 60 秒冷却期内不重复触发。**无感知守护，不打断工作节奏。**

### 📊 Screen Time Tracker — 静默时长统计

通过系统事件被动接收前台应用切换，非轮询、非键盘记录器。敏感数据（如窗口标题）采用 **AES-256-GCM** 加密存储，密钥托管于系统密钥环（Windows DPAPI / macOS Keychain）。统计分析与隐私保护兼得。

### 🍅 Rhythm & Health — 番茄钟 + 颈椎操引导

经典番茄工作法（25 分钟工作 / 5 分钟休息），与 Aura 联动：离座自动暂停，回座继续。休息时弹出颈椎操引导窗口，提供 **8 节动作指引**与倒计时，帮你在间隙中恢复精力。

> 📸 各功能详细截图见 [docs/02-install.md](docs/02-install.md)（首次运行引导）。

---

## 隐私承诺

- **零网络** — 无后台心跳、无更新检查、无崩溃上报，可用 Wireshark 验证
- **零遥测** — 不收集任何使用数据，所有日志仅本地保存
- **无需账号** — 无登录、无 Token，开箱即用
- **端到端加密** — 敏感字段 AES-256-GCM 加密，密钥留在系统密钥环
- **数据归你** — 支持 JSON / CSV 一键导出或彻底删除

> 详细隐私方案与威胁模型见 [docs/07-privacy-security.md](docs/07-privacy-security.md)。

---

## 🚀 快速开始

**安装预编译版本**（推荐）：前往 [GitHub Releases](https://github.com/U13tor/Margin/releases) 下载。Windows 用户可选 `.exe` 安装包（免管理员）或绿色版（解压即用）。详见 [docs/02-install.md](docs/02-install.md)。

**从源码构建**：需要 Qt 6.7+、CMake 3.21+、vcpkg、Visual Studio 2022。详见 [docs/03-build-from-source.md](docs/03-build-from-source.md)。

| 维度 | 规格 |
|---|---|
| **系统** | Windows 10 1809+ (x64)；macOS / Linux 支持见路线图 |
| **资源** | 安装 < 80 MB · 内存 < 80 MB · CPU 空闲 < 1% |
| **权限** | 用户级安装，无需管理员 |

---

##  路线图 / Roadmap

### v1.0 — 当前版本 ✅

Windows MVP 已发布。三大核心插件（Aura Locker / Screen Time / Rhythm & Health）功能完整上线，插件化架构与权限模型稳定运行。

### v1.1 — 跨平台扩展

- 🍎 **macOS 后端适配** — 蓝牙、屏幕时间、番茄钟全模块适配 macOS
- 🐧 **Linux 支持** — 基于 BlueZ / D-Bus 的蓝牙后端
- 📤 **数据导出增强** — 可视化报表导出、周报 / 月报聚合
- 🌐 **i18n 完善** — 完整英文本地化，社区翻译支持

### v2.0 — 远期展望

- 🧩 **插件市场** — 开放第三方插件分发与管理，拓展功能生态
- 🤖 **智能疲劳检测** — 探索基于本地模型的用眼疲劳与姿态分析（保持零网络承诺）
- 📱 **移动端联动** — 通过局域网与手机配对，同步健康数据与提醒

> 路线图会根据社区反馈持续调整，欢迎在 [GitHub Issues](https://github.com/U13tor/Margin/issues) 提出你想看到的功能。

---

## 文档导航

| 文档 | 主题 |
|---|---|
| [01-architecture.md](docs/01-architecture.md) | 整体架构与启动、退出时序 |
| [02-install.md](docs/02-install.md) | 安装与首次运行 |
| [03-build-from-source.md](docs/03-build-from-source.md) | 从源码构建 |
| [04-plugin-spec.md](docs/04-plugin-spec.md) | 插件 ABI、manifest 与权限模型 |
| [05-host-services.md](docs/05-host-services.md) | Host 服务 API 参考 |
| [06-platform-support.md](docs/06-platform-support.md) | 平台支持矩阵 |
| [07-privacy-security.md](docs/07-privacy-security.md) | 隐私承诺、加密方案与威胁模型 |
| [09-testing.md](docs/09-testing.md) | 测试策略与本地验证 |

---

## 贡献 & 联系

欢迎 Pull Request、Issue 或任何形式的反馈！

- 贡献指南：[CONTRIBUTING.md](docs/CONTRIBUTING.md)
- 问题反馈：[GitHub Issues](https://github.com/U13tor/Margin/issues)
- 仓库地址：[github.com/U13tor/Margin](https://github.com/U13tor/Margin)

## 📄 License

[LGPL-3.0-or-later](LICENSE) — 每个源文件头均含 `SPDX-License-Identifier: LGPL-3.0-or-later`。
