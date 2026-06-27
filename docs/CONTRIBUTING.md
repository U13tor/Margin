# 贡献指南

感谢您考虑为 Margin 贡献代码、文档或反馈。本指南说明如何高效协作。

---

## 行为准则

请保持友善、尊重与建设性。任何形式的骚扰、歧视或人身攻击均不予接纳。
讨论请聚焦于代码与设计，不针对个人。

---

## 我能贡献什么

| 类型 | 适合谁 |
|---|---|
| Bug 修复 | 任何贡献者（优先参阅 Issues 中标记 `good first issue` 的条目） |
| 文档改进 | 任何贡献者（typo、链接失效与措辞优化均欢迎） |
| 平台后端（macOS 与 Linux) | 熟悉 CoreBluetooth、CGEvent 或 BlueZ 的 C++ 开发者 |
| 新插件 | 希望为自己定制工具的开发者（参阅 [04-plugin-spec.md](04-plugin-spec.md)) |
| 测试用例补充 | 熟悉 Qt Test 的开发者 |
| UI 与 UX 设计 | 设计师（所有界面基于 QML，便于迭代） |
| 翻译（i18n) | 任意语言的母语者 |

---

## 报告 Bug

提交 Issue 前：

1. 在 [Issues](https://github.com/U13tor/Margin/issues) 中搜索是否已有相同报告
2. 升级至最新 release，确认问题仍然存在
3. 收集以下信息：
   - Margin 版本（设置 → 关于）
   - 操作系统版本
   - 复现步骤（请尽量详细）
   - 期望行为与实际行为
   - 日志文件路径：
     - Windows:`%LOCALAPPDATA%\Margin\logs\margin.log`
     - macOS:`~/Library/Logs/Margin/margin.log`

**安全相关 bug**（可能影响用户隐私或加密强度）请不要在公开 Issue 中提交，请参阅
[07-privacy-security.md](07-privacy-security.md) 末尾的安全披露渠道。

---

## 提交 Pull Request

### 工作流

```bash
# 1. Fork 仓库至个人 GitHub 账号,然后 clone 至本地
git clone https://github.com/<your-account>/Margin.git
cd Margin

# 2. 添加上游
git remote add upstream https://github.com/U13tor/Margin.git

# 3. 基于 main 创建新分支
git checkout -b fix/some-bug

# 4. 编写代码与测试,并运行测试
# 本地构建步骤详见 03-build-from-source.md
# Windows 下必须在 Git Bash 中执行(不要使用 PowerShell 或 cmd)
bash scripts/build-smoke.sh

# 5. 提交并推送至个人 fork
git push -u origin fix/some-bug

# 6. 在 GitHub 上向 upstream main 发起 Pull Request
```

### Commit 规范

采用 [Conventional Commits](https://www.conventionalcommits.org/):

```text
<type>(<scope>): <subject>

[可选 body 说明 why]
```

| type | 用途 |
|---|---|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档改动 |
| `refactor` | 重构（无功能变化） |
| `test` | 测试改动 |
| `chore` | 构建、工具链或杂项 |
| `perf` | 性能优化 |

示例：`feat(rhythm): add postponed counter to toast`、
`fix(aura): cooldown was not enforced after manual unlock`。

### PR 检查清单

提交前请自检：

- [ ] 本地 `bash scripts/build-smoke.sh` 全部通过（Windows 请在 Git Bash 中执行；包含 configure、build、test 与审计）
- [ ] 新功能附带测试（Qt Test 单元或集成测试）
- [ ] 不引入新的网络调用（grep `QNetworkAccessManager\|QNetworkRequest` 结果应为零）
- [ ] 不引入占位加密或 XOR(grep `placeholder\|XOR` 结果应为零）
- [ ] 不引入闭源或 GPL-only 依赖
- [ ] commit message 符合 Conventional Commits
- [ ] 改动范围聚焦，一次 PR 只解决一件事

### PR 规模

PR 越小越便于 review。一般控制在 500 行以内（代码、测试与文档总计）；超出时
建议拆分为多个 PR。该数值为指导而非硬性上限，功能完整性优先。

---

## 代码风格

- C++:`PascalCase` 类名与函数名，`camelCase` 变量与方法，`m_` 前缀成员，
  `k` 前缀常量
- 命名空间：`Margin::`(host 与 API 层）或 `Margin::Plugins::<id>::`（插件内部）
- QML 模块：`Margin.Ui.*`
- 文件名：`kebab-case.md`（文档）或 `PascalCase.{h,cpp}`(C++)
- 命名需有意义，避免泛化占位词（`Helper`、`Manager`、`Engine` 等无信息量后缀
  应配合具体语义，如 `PlatformBackend`、`Keyring`)

---

## 加新功能前的检查

Margin 有几条不可违反的承诺，违反这些承诺的 PR 将不予合并：

1. **100% 本地，零网络**：任何联网代码（版本检查、崩溃上报、云同步、
   遥测）均不予接纳。
2. **加密禁止占位**：不允许使用 XOR、空实现或 `// TODO crypto`。
   敏感数据加密需走 OS 密钥环与 AES-256-GCM。
3. **不引入闭源依赖**：所有第三方库必须为 LGPL、MIT、BSD、Apache 或 MPL 兼容。
4. **测试与代码同 PR**：新功能未附测试不予合并。

---

## 测试要求

详见 [09-testing.md](09-testing.md)。摘要：

| 层级 | 工具 | 适用范围 |
|---|---|---|
| Unit | Qt Test | 单类或单函数 |
| Integration | Qt Test + 真实 SQLite、DPAPI、Keychain | 跨类协作 |
| Smoke | 自定义 runner | 启动 → 运行 → 退出 |
| Manual | checklist | UI、蓝牙、锁屏等需真人观察的场景 |

PR 至少包含 1 个 unit test（覆盖新功能的正常路径与 1 个 failure 路径）。

---

## 贡献协议

提交 PR 即视为同意将该贡献按 LGPL-3.0-or-later 许可发布，无需额外签署 CLA。

---

## 需要帮助

- 提交 Issue 并添加 `question` 标签
- 在 PR 描述中 @ 维护者

维护者尽量在 7 天内完成 review，大型 PR 可能更久。若等待时间过长，可直接在 Issue 或 PR 中 @ 维护者提醒。
