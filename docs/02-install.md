# 02 — 安装与首次运行

面向终端用户。介绍从安装到 Margin 在系统托盘中正常运行所需的全部步骤。

如需从源码构建，请参阅 [03-build-from-source.md](03-build-from-source.md)。

---

## 下载

预编译安装包发布在
[GitHub Releases](https://github.com/U13tor/Margin/releases)。

| 平台 | 安装包或分发格式 | 大小 |
|---|---|---|
| Windows 10 1809+ | `Margin-Setup-<version>.exe`(NSIS 安装器）<br>`Margin-Portable-<version>.zip`（绿色免安装版） | 约 30 MB |
| macOS 11+ | `.dmg`（计划于 v1.1 版本提供） | 暂无 |

---

## Windows 安装

- **安装版**:
  1. 双击 `Margin-Setup-<version>.exe`。
  2. 选择安装位置（默认 `%LOCALAPPDATA%\Margin\`,**无需管理员权限**)。
  3. 选择是否创建桌面快捷方式或开机自启。
  4. 点击"安装"，通常 5 秒内完成。
  5. 点击"完成",Margin 将自动启动，系统托盘显示图标。

- **绿色版（免安装）**:
  1. 下载并解压 `Margin-Portable-<version>.zip` 到任意具有读写权限的目录（建议不要放在系统保护文件夹内，无需管理员权限）。
  2. 双击 `Margin.exe` 即可直接运行。

**无需 UAC 提示**。Margin 所有功能（锁屏、蓝牙、应用监听与密钥环）均为
用户级 API，不需要管理员权限。

### 卸载

- 通过"设置 → 应用 → Margin → 卸载"完成，或
- 找到 `uninstall.exe` 双击执行

卸载默认保留用户数据（配置、数据库与日志）。彻底删除方式参见文末"彻底清除数据"章节。

---

## 首次启动会做什么

首次启动 Margin 时将执行以下操作：

1. **创建运行时目录**:
   - `%APPDATA%\Margin\config\`（配置）
   - `%APPDATA%\Margin\data\`(SQLite 数据）
   - `%LOCALAPPDATA%\Margin\logs\`（日志）
   - `%LOCALAPPDATA%\Margin\cache\`（缓存）

2. **生成 master key 并存入 OS 密钥环**:
   - Windows:DPAPI `CryptProtectData`（绑定当前 Windows 账户）
   - macOS:Keychain Services(v1.1 版本提供）

3. **加载官方插件**:Aura Locker、Screen Time、Rhythm 与 LlamaPet。首次
   加载会弹出权限申请对话框，请逐项授权（LlamaPet 申请 `localhost-http` 本地回环权限）。

4. **显示系统托盘图标**，共三种状态：
   - **Normal**（紫色）：空闲
   - **Locked**:Aura 锁屏中
   - **Stretching**:Rhythm 健操引导中

---

## 主面板

点击托盘图标打开主面板，共包含 5 个标签页：

| 标签页 | 内容 |
|---|---|
| **Overview** | 跨插件活动摘要（最近事件、Aura 状态、今日番茄数） |
| **Aura** | 配对蓝牙设备、RSSI 阈值、away 延迟、cooldown |
| **Screen Time** | 今日、本周或本月应用时长、分类与导出 |
| **Rhythm** | 番茄钟进度、推迟次数与休息时长 |
| **LlamaPet** | 本地推理遥测 GeekHUD、GPU 显存/利用率、60点走势曲线、SLOT 槽位明细与 KV 释放 |

设置中心（Settings）为独立窗口，各插件设有专属配置页，Host 提供语言、主题、开机自启等通用设置。

---

## Aura Locker 配对指引

Aura Locker 需要用户配对蓝牙设备才能生效。配置步骤如下：

1. 打开主面板，切换到"Aura"标签页。
2. 点击"配对设备"，开始扫描附近的 BLE 设备。
3. 将手机、手表或耳机设为可发现模式。
4. 在扫描列表中选中目标设备，点击"配对"。
5. 将手机放置在电脑旁，RSSI 应在 `-40 ~ -60 dBm` 之间。
6. 携带手机离开 5 米以上，RSSI 将下降到阈值以下（默认 `-65 dBm`)。
7. 持续 30 秒（默认 `away_delay`）后自动锁屏。

**安全下限**（用于防止误触发）:

- `away_delay_seconds`:10–120 秒（下限用于防止有人经过时误锁）
- `cooldown_seconds`:30–300 秒（下限用于防止锁屏与解锁反复抖动）

即使直接编辑 `settings.json` 也无法绕过这些下限，Host 在加载时会执行数值钳制（clamp)。

---

## LlamaPet 快速配置指引

LlamaPet 提供本地大模型推理与 GPU 实时遥测，默认开箱即用：

1. **默认端点**：出厂预设端点为 `http://127.0.0.1:1802`（兼容常见 llama-server 或本地聚合网关端口）。如果使用原生 llama.cpp（默认端口 8080）或 Ollama（默认端口 11434），可在「设置中心 → LlamaPet」中随时修改。
2. **明文配置自动迁移**：若您此前使用过独立版 LlamaPet，Margin 首次加载时会自动读取 `%APPDATA%\LlamaPet\config\config.toml`，无缝导入旧版端点与配置，并将 API Key 自动迁移至系统密钥环（DPAPI）加密保存。
3. **托盘快捷操作**：
   - 切换桌宠悬浮窗显隐
   - MiniPet (96×96 点阵羊驼) ↔ Dock (260×50 仪表盘) 模式切换
   - 窗口置顶、鼠标穿透、贴边自动半隐藏
   - 清空空闲 KV 缓存
   - 复制重启命令到剪贴板
4. **快捷键**：全局热键 `Ctrl+Alt+P` 可在任意应用前台瞬间呼出 / 隐藏 LlamaPet 桌宠悬浮窗。

> 详尽的服务启动推荐参数、KV 缓存优化与进阶排错，请参阅 [10-llamapet.md](10-llamapet.md)。

---

## 数据存储位置

| 用途 | Windows |
|---|---|
| 安装目录 | `%LOCALAPPDATA%\Margin\` |
| 配置 | `%APPDATA%\Margin\config\settings.json` |
| 数据（SQLite) | `%APPDATA%\Margin\data\margin.db` |
| 日志 | `%LOCALAPPDATA%\Margin\logs\margin.log` |
| 缓存 | `%LOCALAPPDATA%\Margin\cache\` |

代码中始终通过 `Margin::Paths` 获取上述路径，不直接调用 `QStandardPaths`。

---

## 彻底清除数据

进入"设置中心 → Data & Export → 清除全部数据"。该操作将：

- 清空 SQLite 数据库（所有插件的数据）
- 删除 `settings.json`
- 删除日志文件
- 从 OS 密钥环中删除 master key

执行前会弹出确认对话框，需二次输入"DELETE"才会真正执行。

---

## 排错 FAQ

### 托盘图标看不到

Windows 11：进入"设置 → 个性化 → 任务栏 → 其他系统托盘图标"，开启 Margin。

### Margin 启动后立即退出

打开 `%LOCALAPPDATA%\Margin\logs\margin.log`，查看最后几行的 ERROR 记录。
最常见原因是 OS 密钥环不可用（较为罕见，通常由系统策略限制引起）。

### Aura 不锁屏

- 确认蓝牙模块已开启（任务栏蓝牙图标）
- 确认手机蓝牙开启且在范围内
- 在主面板"Aura"标签页查看 RSSI 是否在更新；若未更新，检查 `margin.log`
  中是否出现 `aura.warning bt_disabled`
- 确认当前未处于 cooldown 期（刚解锁后 60 秒内不再触发）

### 提交 Issue

如仍无法解决，请参阅 [CONTRIBUTING.md](CONTRIBUTING.md) 中的"报告 Bug"章节。
