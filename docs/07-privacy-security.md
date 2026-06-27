# 07 — 隐私与安全

Margin 向用户的核心承诺。本文档解释这些承诺、对应的实现、威胁模型与加密方案。

---

## 隐私承诺

| 编号 | 承诺 | 验证方式 |
|---|---|---|
| 1 | **绝对本地化**——所有数据（数据库、配置与日志）仅存储于本地磁盘 | 见"数据存储位置"章节 |
| 2 | **零网络**——Margin 主程序永不发起任何网络连接 | `netstat -an \| findstr ESTABLISHED`(Margin.exe 运行时应为空）；或使用 Wireshark 或 Little Snitch 抓包 5 分钟 |
| 3 | **开源透明**——LGPL-3.0-or-later，接受社区审计 | 全部源码公开 |
| 4 | **无遥测**——不收集使用数据，不上报崩溃 | 同编号 2 |
| 5 | **用户可导出可删除**——支持一键 JSON 或 CSV 导出，以及一键彻底删除 | 设置 → Data & Export |

---

## 威胁模型

| 编号 | 威胁 | 攻击者能力 | 缓解措施 |
|---|---|---|---|
| T1 | 本地文件窃取 | 能读取用户主目录所有文件（恶意软件或物理接触） | 敏感字段采用 AES-256-GCM 加密，密钥存放于操作系统密钥环（攻击者无法读取） |
| T2 | 第三方恶意插件 | 用户安装了带后门的第三方插件 | deny-by-default 权限模型；首次加载弹窗确认；权限审计日志 |
| T3 | 第三方依赖投毒 | 攻击者污染 vcpkg 中某个包 | vcpkg.json 锁定版本与 SHA;CI 依赖审计；SBOM 随 release 发布 |
| T4 | 蓝牙协议分析 | 攻击者在用户附近监听蓝牙流量 | Margin 不发送任何蓝牙数据，仅接收 RSSI；配对 MAC 本地加密 |
| T5 | 内存 dump | 攻击者获得进程内存读权限 | AES master key 仅在内存中短暂存在，退出时通过 SecureZeroMemory 清零 |

不在威胁模型范围内（超出本文档讨论边界）:

- 0day 内核漏洞
- 攻击者拥有管理员权限且能修改 Margin 二进制
- 用户主动用调试器附加 Margin 进程提取密钥

---

## 加密方案

决策：采用 OS 密钥环与字段级 AES-256-GCM 组合方案。

未采用的其他候选方案对比：

| 方案 | 评价 |
|---|---|
| SQLCipher 全库加密 | 重量级依赖且性能开销显著；Margin 数据量较小（小于 50 MB)，在此场景下性能开销与收益不成比例 |
| 用户密码派生（PBKDF2 或 Argon2) | 用户体验较差（每次启动需输入密码）；密码遗忘等同于数据丢失 |
| 自实现 AES | 禁止自行实现加密算法，应使用 Qt HMAC（RFC 5869 HKDF）与 Windows CNG（AES-256-GCM）等经充分审计的实现 |
| **OS 密钥环 + 字段级 AES-256-GCM**（选定方案） | 行业标准（1Password、Bitwarden 桌面版均采用）；由操作系统保护密钥；灵活，可仅加密敏感字段 |

### 加密流程

```text
Host 启动 → Keyring::getOrCreateMasterKey()
   ├─ Windows: DPAPI CryptProtectData(用户级,绑当前账户)
   └─ macOS:   SecItemAdd(Keychain Services)
   ▼ 拿到 32 字节 master key(每次启动从 OS 取)
   ▼
[插件写敏感字段]
   ▼
CryptoService::encryptString(plaintext)
   ├─ Host 内部 HKDF(master_key, plugin_id) 派生专属密钥
   ├─ 生成 12 字节随机 IV
   ├─ AES-256-GCM 加密
   └─ 返回 base64(iv ‖ ciphertext ‖ tag)
   ▼
插件调 Database::exec 把 base64 字符串存入 SQLite
```

解密为反向流程。

### 密钥生命周期

1. 创建：首次运行时，Host 生成 32 字节随机 master key，存入 OS 密钥环
2. 使用：每次启动从 OS 密钥环读出，缓存在内存中
3. 销毁：Host 退出时通过 `SecureZeroMemory` 或 `explicit_bzero` 清零
4. 用户撤销：用户在"设置 → 删除全部数据"时，同时从 OS 密钥环中删除

### 安全隔离（HKDF 派生）

Host 内部的 `Keyring` 与 `master_key` **永不**暴露给插件。Host 为每个插件构造的 `CryptoService` 通过 `HKDF(master_key, plugin_id)` 派生专属密钥，即使恶意插件 B 通过 `database-read` 权限直接读取 SQLite 获得插件 A 的密文，也无法解密：

- HKDF 派生密钥不同
- AES-GCM tag 校验失败时，`decryptString` 直接返回失败

该设计可抵御"解密预言机攻击"（即恶意插件通过 master_key 接口尝试解密任意密文）。

---

## 两条加密通道

| 通道 | 触发方式 | 密钥来源 |
|---|---|---|
| CryptoService（写 Database) | 插件显式调用 `services->crypto().encryptString()` | `HKDF(master_key, plugin_id)` |
| Settings 透明加密 | 插件在 manifest 中声明 `encrypted_settings`，由 Host 自动加密 | `master_key` 直接派生 |

两条通道均基于 AES-256-GCM，但密钥派生方式不同，从而隔离了第三方插件对根密钥的直接访问。

---

## 敏感字段清单

| 字段 | 所属表 | 敏感 | 理由 |
|---|---|---|---|
| `window_title` | `screen_time_session` | 是 | 可能含文档名、邮件主题或网页标题 |
| `app_name`、`category` | `screen_time_session` | 否 | 进程名不敏感（如 Chrome 或 VS Code) |
| `paired_device_identifier` | `aura` settings | 是 | 设备标识 |
| `device_name` | `aura` settings | 是 | 用户自定义设备名可能含个人信息 |
| 番茄钟时长与计数 | `rhythm` settings | 否 | 行为偏好，非敏感画像 |

Settings 中的敏感字段由插件作者在 `manifest.json.encrypted_settings` 中显式声明。Host 不维护全局敏感键白名单，每个插件作者自行决定。

---

## 被动监听边界

Margin 的两个平台监听服务均为**被动事件驱动**，绝不轮询：

- `WindowMonitorService`:Windows 端使用 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, WINEVENT_OUTOFCONTEXT)`，由系统在窗口切换时推送回调，不调用 `GetForegroundWindow()` 进行轮询。
- `InputMonitorService`:Windows 端使用 `WH_KEYBOARD_LL` 与 `WH_MOUSE_LL` 全局钩子，始终通过 `CallNextHookEx(0, ...)` 转发，仅以 tick 时间戳判定闲置状态；不消费、不阻塞亦不记录键值。

这两项权限需在 `manifest.json.permissions` 中显式声明为 `active-window-monitor` 与 `input-monitor`。

---

## 零网络验证

### 使用 Wireshark

1. 启动 Margin 并完成常规使用（切窗、闲置、蓝牙锁屏、番茄钟）
2. 在 Wireshark 中选择本机网卡，以 `tcp or udp` 为过滤条件抓包 5 分钟
3. 观察 Margin.exe 是否产生出站数据包，预期为**零**

### 使用 Little Snitch(macOS)

授权 Margin 进程后，观察是否存在任何出站连接尝试，预期为**零**。

### 使用 netstat(Windows)

```powershell
# Margin 运行时
Get-Process Margin | ForEach-Object {
    Get-NetTCPConnection -OwningProcess $_.Id -ErrorAction SilentlyContinue
}
# 期望:无输出
```

### 代码审计

```bash
# 任何联网类禁止
grep -rn "QNetworkAccessManager\|QNetworkRequest\|QTcpSocket\|QUdpSocket" src/
# 必须返回 0 条

# 占位或 XOR 禁令
grep -rEi "placeholder|XOR" src/
# 必须返回 0 条
```

---

## 权限审计日志

每次插件申请权限与每次使用权限均写入审计日志：

- Windows:`%LOCALAPPDATA%\Margin\logs\permissions.log`
- macOS:`~/Library/Logs/Margin/permissions.log`

格式示例：

```text
[2026-06-15 10:23:45] [INFO] Plugin "aura_locker" granted: bluetooth-scan, system-lock
[2026-06-15 10:24:01] [INFO] Plugin "aura_locker" used bluetooth-scan (RSSI read iPhone X rssi=-52)
[2026-06-15 10:24:30] [INFO] Plugin "aura_locker" used system-lock (LockWorkStation)
```

用户可在"设置中心 → Privacy → Permission Audit Log"查看最近 7 天记录，并支持按插件筛选。

---

## 依赖审计

所有非 Qt 的第三方依赖均在 `vcpkg.json` 中显式声明并锁定版本。Qt 本身通过官方安装器或 aqt 获取。

License 兼容性：

| License | 与 LGPL-3.0-or-later 兼容 |
|---|---|
| MIT、BSD-2、BSD-3、Apache-2.0 | 兼容 |
| LGPL-2.1+、LGPL-3.0+ | 兼容 |
| MPL-2.0 | 兼容（文件级 copyleft，可隔离） |
| GPL-2、GPL-3、AGPL-3 | 不兼容（Host 与 plugin_api 禁止引入） |

CI 会生成 SBOM(SPDX 格式）随 release 发布，用户可审计 Margin 使用的第三方库及版本。

---

## 安全披露

发现安全漏洞（可能影响用户隐私或加密强度）时，请不要在公开 Issue 中提交细节。

请直接发送邮件至 GitHub 维护者账号，或在 Issue 描述中省略漏洞细节并请求私下沟通渠道。维护方承诺：

- 48 小时内确认收到
- 7 天内评估影响
- 30 天内发布修复版本
- 在 release notes 中致谢报告者（除非报告者要求匿名）

---

## 进一步阅读

- 加密机制与 Settings 透明加密：[05-host-services.md](05-host-services.md)
- 权限模型：[04-plugin-spec.md](04-plugin-spec.md)
- 平台 API(DPAPI 与 Keychain):[06-platform-support.md](06-platform-support.md)
