# 02 — 安装与首次运行

面向终端用户。从 0 到看到 Margin 在系统托盘跑起来,需要做什么。

想从源码构建看 [03-build-from-source.md](03-build-from-source.md)。

---

## 下载

预编译安装包发布在
[GitHub Releases](https://github.com/U13tor/Margin/releases)。

| 平台 | 安装包 | 大小 |
|---|---|---|
| Windows 10 1809+ | `Margin-Setup-<version>.exe`(NSIS 安装器) | ~30 MB |
| macOS 11+ | `.dmg`(v1.1 上线) | 暂无 |

---

## Windows 安装

1. 双击 `Margin-Setup-<version>.exe`。
2. 选择安装位置(默认 `%LOCALAPPDATA%\Margin\`,**无需管理员**)。
3. 选择是否创建桌面快捷方式 / 开机自启。
4. 点击"安装"——通常 5 秒内完成。
5. 点击"完成"——Margin 自动启动,系统托盘出现图标。

**无需 UAC 提示**。Margin 所有功能(锁屏、蓝牙、应用监听、密钥环)都是
用户级 API,不需要管理员权限。

### 卸载

- "设置 → 应用 → Margin → 卸载",或
- 找到 `uninstall.exe` 双击

卸载保留用户数据(配置 / 数据库 / 日志)。彻底删除见文末"彻底清除数据"。

---

## 首次启动会做什么

首次启动 Margin 会:

1. **创建运行时目录**:
   - `%APPDATA%\Margin\config\`(配置)
   - `%APPDATA%\Margin\data\`(SQLite 数据)
   - `%LOCALAPPDATA%\Margin\logs\`(日志)
   - `%LOCALAPPDATA%\Margin\cache\`(缓存)

2. **生成 master key 并存入 OS 密钥环**:
   - Windows:DPAPI `CryptProtectData`(绑当前 Windows 账户)
   - macOS:Keychain Services(v1.1)

3. **加载官方插件**:Hello / Aura Locker / Screen Time / Rhythm。首次
   加载会弹权限申请弹窗,逐项授权。

4. **显示系统托盘图标**——三种状态:
   - **Normal**(紫色):空闲
   - **Locked**:Aura 锁屏中
   - **Stretching**:Rhythm 健操引导中

---

## 主面板

点击托盘图标 → 主面板出现。4 个 Tab:

| Tab | 内容 |
|---|---|
| **Overview** | 跨插件活动摘要(最近事件、Aura 状态、今日番茄数) |
| **Aura** | 配对蓝牙设备、RSSI 阈值、away 延迟、cooldown |
| **Screen Time** | 今日 / 本周 / 本月应用时长、分类、导出 |
| **Rhythm** | 番茄钟进度、推迟次数、休息时长 |

设置中心(Settings)单独一个窗口,各插件有自己的设置页 + Host 的通用设置
(语言、主题、开机自启)。

---

## Aura Locker 配对指引

Aura Locker 是 Phase 1 唯一需要用户配置才能生效的插件。步骤:

1. 打开主面板 → Tab "Aura"。
2. 点"配对设备"——开始扫描附近的 BLE 设备。
3. 把手机 / 手表 / 耳机设为可发现模式。
4. 在扫描列表里选中你的设备,点"配对"。
5. 把手机放在电脑旁,RSSI 应在 `-40 ~ -60 dBm`。
6. 带手机走开 5+ 米,RSSI 下降到阈值以下(默认 `-65 dBm`)。
7. 持续 30 秒(默认 `away_delay`)后自动锁屏。

**安全下限**(防止误触发):

- `away_delay_seconds`:10–120 秒(下限防止有人经过时误锁)
- `cooldown_seconds`:30–300 秒(下限防止锁屏与解锁反复抖动)

直接编辑 `settings.json` 也绕不过这些下限——Host 在加载时 clamp。

---

## 数据存储位置

| 用途 | Windows |
|---|---|
| 安装目录 | `%LOCALAPPDATA%\Margin\` |
| 配置 | `%APPDATA%\Margin\config\settings.json` |
| 数据(SQLite) | `%APPDATA%\Margin\data\margin.db` |
| 日志 | `%LOCALAPPDATA%\Margin\logs\margin.log` |
| 缓存 | `%LOCALAPPDATA%\Margin\cache\` |

代码中始终通过 `Margin::Paths` 获取,不直接调 `QStandardPaths`。

---

## 彻底清除数据

设置中心 → Data & Export → "清除全部数据"。这会:

- 清空 SQLite 数据库(所有插件的数据)
- 删除 `settings.json`
- 删除日志文件
- 从 OS 密钥环删除 master key

执行前会弹确认对话框,需要二次输入"DELETE"才能执行。

---

## 排错 FAQ

### 托盘图标看不到

Windows 11:设置 → 个性化 → 任务栏 → 其他系统托盘图标 → 开启 Margin。

### Margin 启动后立即退出

打开 `%LOCALAPPDATA%\Margin\logs\margin.log`,看最后几行的 ERROR。
最常见原因是 OS 密钥环不可用(罕见,通常是系统策略限制)。

### Aura 不锁屏

- 确认蓝牙模块开启(任务栏蓝牙图标)
- 确认手机蓝牙开启且在范围内
- 看主面板 Tab "Aura" 的 RSSI 是否在更新(若不更新,看 `margin.log`
  是否有 `aura.warning bt_disabled`)
- 确认未处于 cooldown 期(刚解锁后 60 秒内不再触发)

### 提 Issue

仍解决不了,见 [CONTRIBUTING.md](CONTRIBUTING.md) "报告 Bug" 段。
