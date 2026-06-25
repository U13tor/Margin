# 07 — 隐私与安全

Margin 向用户的核心承诺。本文档解释这些承诺、对应的实现、威胁模型、
加密方案。

---

## 隐私承诺

| # | 承诺 | 怎么验证 |
|---|---|---|
| 1 | **绝对本地化**——所有数据(数据库 / 配置 / 日志)只在本地磁盘 | 见数据存储位置 |
| 2 | **零网络**——Margin 主程序永不发起任何网络连接 | `netstat -an \| findstr ESTABLISHED`(Margin.exe 运行时应为空);或用 Wireshark / Little Snitch 抓包 5 分钟 |
| 3 | **开源透明**——LGPL-3.0-or-later,接受社区审计 | 全部源码公开 |
| 4 | **无遥测**——不收集使用数据,不上报崩溃 | 同 #2 |
| 5 | **用户可导出可删除**——一键 JSON / CSV 导出,一键彻底删除 | 设置 → Data & Export |

---

## 威胁模型

| # | 威胁 | 攻击者能力 | 缓解 |
|---|---|---|---|
| T1 | 本地文件窃取 | 能读用户主目录所有文件(恶意软件 / 物理接触) | 敏感字段 AES-256-GCM 加密,密钥在 OS 密钥环(攻击者读不到) |
| T2 | 第三方恶意插件 | 用户安装了带后门的第三方插件 | deny-by-default 权限模型;首次加载弹窗;权限审计日志 |
| T3 | 第三方依赖投毒 | 攻击者污染 vcpkg 某个包 | vcpkg.json 锁版本 + SHA;CI 依赖审计;SBOM 随 release 发布 |
| T4 | 蓝牙协议分析 | 攻击者在用户附近监听蓝牙流量 | Margin 不发送任何蓝牙数据,只接收 RSSI;配对 MAC 本地加密 |
| T5 | 内存 dump | 攻击者获得进程内存读权限 | AES master key 只在内存短暂存在;退出时 SecureZeroMemory 清零 |

**不在威胁模型中**(超出范围):

- 0day 内核漏洞
- 攻击者拥有管理员权限且能修改 Margin 二进制
- 用户主动用调试器附加 Margin 进程提取密钥

---

## 加密方案

**决策**:OS 密钥环 + 字段级 AES-256-GCM。

为什么不选其他方案:

| 方案 | 评价 |
|---|---|
| SQLCipher 全库加密 | 重量级依赖 + 性能开销;Margin 数据量小(< 50 MB),杀鸡用牛刀 |
| 用户密码派生(PBKDF2 / Argon2) | 用户体验差(每次启动输密码);密码忘记 = 数据丢失 |
| 自实现 AES | **绝对禁止**。永远不要自己造加密;用 OpenSSL |
| **OS 密钥环 + 字段级 AES-256-GCM** | 行业标准(1Password / Bitwarden 桌面版都用);OS 保护密钥;灵活(只加密敏感字段) |

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

解密是反向流程。

### 密钥生命周期

1. **创建**:首次运行,Host 生成 32 字节随机 master key,存入 OS 密钥环
2. **使用**:每次启动从 OS 密钥环读出,缓存内存
3. **销毁**:Host 退出时 `SecureZeroMemory` / `explicit_bzero` 清零
4. **用户撤销**:用户"设置 → 删除全部数据"时,同时从 OS 密钥环删除

### 安全隔离(HKDF 派生)

Host 内部的 `Keyring` 与 `master_key` **永不**暴露给插件。Host 为每个
插件构造的 `CryptoService` 用 `HKDF(master_key, plugin_id)` 派生专属
密钥——即使恶意插件 B 拿到插件 A 的密文(通过 `database-read` 权限直接
读 SQLite),也无法解密:

- HKDF 派生密钥不同
- AES-GCM tag 校验失败,`decryptString` 直接返回失败

这免疫了"解密预言机攻击"(恶意插件用 master_key 接口尝试解密任意密文)。

---

## 两条加密通道

| 通道 | 触发方式 | 密钥来源 |
|---|---|---|
| **CryptoService**(写 Database) | 插件显式调 `services->crypto().encryptString()` | `HKDF(master_key, plugin_id)` |
| **Settings 透明加密** | 插件在 manifest 声明 `encrypted_settings`,Host 自动加密 | `master_key` 直接 |

两条通道都基于 AES-256-GCM,但密钥派生不同——隔离了第三方插件对根密钥
的直接访问。

---

## 敏感字段清单

| 字段 | 表 | 敏感 | 理由 |
|---|---|---|---|
| `window_title` | `screen_time_session` | **是** | 可能含文档名 / 邮件主题 / 网页标题 |
| `app_name` / `category` | `screen_time_session` | 否 | 进程名不敏感(Chrome / VS Code) |
| `paired_device_identifier` | `aura` settings | **是** | 设备标识 |
| `device_name` | `aura` settings | **是** | 用户起的设备名可能含个人信息 |
| 番茄钟时长 / 计数 | `rhythm` settings | 否 | 行为偏好非敏感画像 |

Settings 中的敏感字段由插件作者在 `manifest.json.encrypted_settings`
显式声明。**Host 不维护全局敏感键白名单**——每个插件作者自行决定。

---

## 被动监听边界

Margin 的两个平台监听服务都是**被动事件驱动**,**绝不轮询**:

- `WindowMonitorService`:Win 用 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
  WINEVENT_OUTOF_CONTEXT)`——系统在窗口切换时推送回调。**不调**
  `GetForegroundWindow()` 轮询。
- `InputMonitorService`:Win 用 `WH_KEYBOARD_LL` + `WH_MOUSE_LL` 全局
  钩子,始终 `CallNextHookEx(0, ...)` 转发——**不消费 / 不阻塞 / 不
  记录键值**,仅用 tick 时间戳判定闲置。

这两个权限在 `manifest.json.permissions` 显式声明为 `active-window-monitor`
+ `input-monitor`。

---

## 零网络验证

### 用 Wireshark

1. 启动 Margin + 完成正常使用(切窗 / 闲置 / 蓝牙锁屏 / 番茄钟)
2. 在 Wireshark 选你的网卡,过滤 `tcp or udp` 抓 5 分钟
3. 观察是否有 Margin.exe 的 outbound 包——预期**零**

### 用 Little Snitch(macOS)

授权 Margin 进程,观察是否有任何 outbound 连接尝试——预期**零**。

### 用 netstat(Windows)

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
# 必须 0 hits

# 占位 / XOR 禁令
grep -rEi "placeholder|XOR" src/
# 必须 0 hits
```

---

## 权限审计日志

每次插件申请权限 + 每次使用权限都写入审计日志:

- Win:`%LOCALAPPDATA%\Margin\logs\permissions.log`
- Mac:`~/Library/Logs/Margin/permissions.log`

格式示例:

```text
[2026-06-15 10:23:45] [INFO] Plugin "aura_locker" granted: bluetooth-scan, system-lock
[2026-06-15 10:24:01] [INFO] Plugin "aura_locker" used bluetooth-scan (RSSI read iPhone X rssi=-52)
[2026-06-15 10:24:30] [INFO] Plugin "aura_locker" used system-lock (LockWorkStation)
```

用户可在设置中心 → Privacy → Permission Audit Log 查看(最近 7 天,
支持按插件筛选)。

---

## 依赖审计

所有第三方 **非-Qt** 依赖在 `vcpkg.json` 显式声明,锁版本。Qt 本身走
官方安装器或 aqt。

License 兼容性:

| License | 与 LGPL-3.0-or-later 兼容 |
|---|---|
| MIT / BSD-2 / BSD-3 / Apache-2.0 | ✓ |
| LGPL-2.1+ / LGPL-3.0+ | ✓ |
| MPL-2.0 | ✓(文件级 copyleft,可隔离) |
| GPL-2 / GPL-3 / AGPL-3 | ✗(Host 与 plugin_api 禁止) |

CI 生成 SBOM(SPDX 格式)随 release 发布,用户可审计 Margin 用了哪些
第三方库及版本。

---

## 安全披露

发现安全漏洞(可能影响用户隐私 / 加密强度)请**不要**公开提 Issue。

直接发邮件到 GitHub 维护者账号,或在 Issue 里描述时省略漏洞细节,请求
私下沟通渠道。我们承诺:

- 48 小时内确认收到
- 7 天内评估影响
- 30 天内发布修复版本
- 在 release notes 致谢报告者(除非你要求匿名)

---

## 进一步阅读

- 加密机制与 Settings 透明加密:[05-host-services.md](05-host-services.md)
- 权限模型:[04-plugin-spec.md](04-plugin-spec.md)
- 平台 API(DPAPI / Keychain):[06-platform-support.md](06-platform-support.md)
