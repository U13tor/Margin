# 贡献指南

感谢你考虑为 Margin 贡献代码 / 文档 / 反馈。本指南说明如何高效协作。

---

## 行为准则

请保持友善、尊重、建设性。任何形式的骚扰 / 歧视 / 人身攻击都会被立即
拒绝。讨论针对代码与设计,不针对人。

---

## 我能贡献什么

| 类型 | 适合谁 |
|---|---|
| Bug 修复 | 任何人——优先看 Issues 标 `good first issue` 的 |
| 文档改进 | 任何人——typo、链接失效、措辞优化都欢迎 |
| 平台后端(macOS / Linux) | 熟悉 CoreBluetooth / CGEvent / BlueZ 的 C++ 开发者 |
| 新插件 | 任何想给自己写工具的开发者(参考 [04-plugin-spec.md](04-plugin-spec.md)) |
| 测试用例补充 | 熟悉 Qt Test 的人 |
| UI / UX 设计 | 设计师——所有界面都基于 QML,易于迭代 |
| 翻译(i18n) | 任何母语使用者 |

---

## 报告 Bug

提 Issue 前请:

1. 在 [Issues](https://github.com/U13tor/Margin/issues) 搜索是否已有人提过
2. 升级到最新 release,看是否已修复
3. 收集以下信息:
   - Margin 版本(设置 → 关于)
   - 操作系统版本
   - 复现步骤(越详细越好)
   - 期望行为 vs 实际行为
   - 日志文件路径:
     - Windows:`%LOCALAPPDATA%\Margin\logs\margin.log`
     - macOS:`~/Library/Logs/Margin/margin.log`

**安全相关 bug**(可能影响用户隐私 / 加密强度)请不要公开提 Issue,见
[07-privacy-security.md](07-privacy-security.md) 末尾的安全披露渠道。

---

## 提交 Pull Request

### 工作流

```bash
# 1. Fork 仓库到自己的 GitHub 账号,然后 clone 到本地
git clone https://github.com/<your-account>/Margin.git
cd Margin

# 2. 添加上游
git remote add upstream https://github.com/U13tor/Margin.git

# 3. 新建分支(基于 main)
git checkout -b fix/some-bug

# 4. 改代码 + 写测试 + 跑测试
# 见 03-build-from-source.md 了解本地构建步骤
# Windows:必须在 Git Bash 里执行(不在 PowerShell / cmd)
bash scripts/build-smoke.sh

# 5. Commit 并推到自己的 fork
git push -u origin fix/some-bug

# 6. 在 GitHub 上发起 Pull Request 到 upstream main
```

### Commit 规范

使用 [Conventional Commits](https://www.conventionalcommits.org/):

```text
<type>(<scope>): <subject>

[可选 body 说明 why]
```

| type | 用途 |
|---|---|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档改动 |
| `refactor` | 重构(无功能变化) |
| `test` | 测试改动 |
| `chore` | 构建 / 工具链 / 杂项 |
| `perf` | 性能优化 |

示例:`feat(rhythm): add postponed counter to toast`、
`fix(aura): cooldown was not enforced after manual unlock`。

### PR 检查清单

提交前自检:

- [ ] 本地 `bash scripts/build-smoke.sh` 全绿(Windows 在 Git Bash 里执行;configure + build + test + 审计)
- [ ] 新功能附带测试(Qt Test 单元 / 集成测试)
- [ ] 不引入新的网络调用(grep `QNetworkAccessManager\|QNetworkRequest` 应为 0 hits)
- [ ] 不引入占位加密 / XOR(grep `placeholder\|XOR` 应为 0 hits)
- [ ] 不引入闭源 / GPL-only 依赖
- [ ] commit message 符合 Conventional Commits
- [ ] 改动范围聚焦——一次 PR 只做一件事

### PR 规模

PR 越小越好 review。一般控制在 500 行内(代码 + 测试 + 文档总计);超出时
考虑拆分为多个 PR。这条是指导而非硬上限——核心功能完整性优先。

---

## 代码风格

- C++:`PascalCase` 类名 / 函数名、`camelCase` 变量与方法、`m_` 前缀成员、
  `k` 前缀常量
- 命名空间:`Margin::`(host / API 层)或 `Margin::Plugins::<id>::`(插件内)
- QML 模块:`Margin.Ui.*`
- 文件名:`kebab-case.md`(文档) / `PascalCase.{h,cpp}`(C++)
- 命名需有意义,避免泛化占位词(`Helper`、`Manager`、`Engine` 这类
  无信息量的后缀尽量配合具体语义,如 `PlatformBackend`、`Keyring`)

---

## 加新功能前的检查

Margin 有几条不可违反的承诺,任何 PR 触碰都会被拒绝:

1. **100% 本地,零网络**——任何联网代码(版本检查、崩溃上报、云同步、
   遥测)都会被拒绝。
2. **加密禁止占位**——不能用 XOR、不能用空实现、不能用 `// TODO crypto`。
   敏感数据加密走 OS 密钥环 + AES-256-GCM。
3. **不引入闭源依赖**——所有第三方库必须 LGPL/MIT/BSD/Apache/MPL 兼容。
4. **测试与代码同 PR**——新功能没测试 = 不合并。

---

## 测试要求

详见 [09-testing.md](09-testing.md)。摘要:

| 层级 | 工具 | 适用 |
|---|---|---|
| Unit | Qt Test | 单类 / 单函数 |
| Integration | Qt Test + 真实 SQLite / DPAPI / Keychain | 跨类协作 |
| Smoke | 自定义 runner | 启动 → 运行 → 退出 |
| Manual | checklist | UI / 蓝牙 / 锁屏等需真人观察的 |

PR 至少包含 1 个 unit test(覆盖新功能的 happy + 1 个 failure 路径)。

---

## 贡献协议

提交 PR 即表示你同意你的贡献按 LGPL-3.0-or-later 许可。无需额外签 CLA。

---

## 需要帮助

- 提 Issue 标 `question` 标签
- 在 PR 描述里 @维护者

我们尽量在 7 天内 review,大 PR 可能更久。如果延迟太久,直接 ping。
