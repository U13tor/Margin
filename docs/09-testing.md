# 09 — 测试

Margin 的 5 层测试策略，以及贡献者在本地验证改动的方法。

---

## 测试原则

| 原则 | 含义 |
|---|---|
| **同 PR 落地** | 改代码必须同 commit 加测试，禁止"先合并后补测试" |
| **金字塔优先** | 单元测试优于集成测试，集成测试优于手动测试；避免"倒金字塔"（全部依赖手动验证） |
| **真实环境** | 集成测试使用真实 SQLite、真实 DPAPI 与 Keychain，不 mock 系统调用 |

---

## 5 层测试矩阵

```text
                ▲
                │
       手动验收(Manual)         Layer 5   真人操作,跨平台实机
                │
       端到端(Smoke)            Layer 4   启动 Margin,验证启动 + 退出
                │
       集成(Integration)        Layer 3   多模块协作,真实依赖
                │
       单元(Unit)               Layer 2   单函数 / 单类,隔离依赖
                │
       静态(Static Audit)       Layer 1   grep / 编译检查
                │
                ▼
```

### Layer 1 — 静态审计

```bash
grep -rEi "placeholder|XOR" src/ && exit 1   # 禁止占位 / XOR
cmake --build build/<preset>                   # 必须编译通过
```

每个 PR 必须执行。

### Layer 2 — 单元测试

- 框架：Qt Test(`QTest::qExec`)
- 范围：单函数或单类，依赖通过 `QSignalSpy` 模拟
- 运行：`ctest --test-dir build/<preset> -L unit`
- 目标：每个核心类至少 3 个用例（正常、边界、异常）

### Layer 3 — 集成测试

- 框架：Qt Test + 真实依赖
- 关键：不 mock 平台 API(DPAPI、Keychain、SQLite 均为真实调用）
- 运行：`ctest --test-dir build/<preset> -L integration`

### Layer 4 — 烟雾测试

- 框架：自定义 `tests/smoke/`
- 范围：启动 Margin 子进程，验证启动与退出无残留
- 运行：`ctest --test-dir build/<preset> -L smoke`

### Layer 5 — 手动验收

每个 Milestone 完成时，维护者实机执行 `tests/manual/M*_ACCEPTANCE.md`
清单（实机 UI、蓝牙配对、锁屏触发等）。

---

## 框架与目录结构

```text
tests/
├── CMakeLists.txt
├── unit/                          # Layer 2
│   ├── test_event_bus.cpp
│   ├── test_plugin_loader.cpp
│   ├── test_keyring.cpp
│   ├── test_settings.cpp
│   ├── test_database.cpp
│   ├── test_bluetooth_proximity.cpp
│   └── test_pomodoro_timer.cpp
├── integration/                   # Layer 3
│   ├── test_plugin_load_unload.cpp
│   ├── test_encryption_roundtrip.cpp
│   ├── test_aura_locker_e2e.cpp
│   └── test_screen_time_pipeline.cpp
├── smoke/                         # Layer 4
│   └── smoke_runner.py            # 启动 Margin → 退出 → 验证
└── manual/                        # Layer 5
    ├── M0_ACCEPTANCE.md
    ├── M1_ACCEPTANCE.md
    ├── M2_ACCEPTANCE.md
    └── M3_ACCEPTANCE.md
```

---

## 本地验证（推荐）

> Windows 下请在 **Git Bash** 中执行（不要使用 PowerShell 或 cmd)，环境说明详见
> [03-build-from-source.md](03-build-from-source.md)。

```bash
bash scripts/build-smoke.sh
```

该脚本依次执行 5 个步骤，全部通过后方可合并：configure、build、ctest、grep 审计与启动二进制验证。

显式指定 preset:

```bash
bash scripts/build-smoke.sh win64-msvc-debug    # Windows
bash scripts/build-smoke.sh macos-clang-debug   # macOS
```

---

## 手动运行测试

> **Windows 前置**:`ctest` 加载测试进程时需要 Qt DLL 处于 PATH 中。运行前
> 请确认 `Qt6_DIR\bin` 已加入 PATH（示例参见
> [03-build-from-source.md](03-build-from-source.md) 手动构建章节）,
> 否则会出现"找不到 Qt6Cored.dll"错误。不建议在外部 Git Bash 中直接执行
> `ctest`（其 PATH 中不含 Qt)，推荐在 **Developer PowerShell for VS** 中运行，
> 或直接使用 `bash scripts/build-smoke.sh`（脚本会自动配置环境）。

```bash
# 全部测试
ctest --test-dir build/win64-msvc-debug --output-on-failure

# 仅运行 unit
ctest --test-dir build/win64-msvc-debug -L unit

# 运行指定测试
ctest --test-dir build/win64-msvc-debug -R test_event_bus --output-on-failure
```

---

## CI 矩阵

GitHub Actions 在 Windows 与 macOS 上分别运行 Debug 与 Release:

```yaml
strategy:
  matrix:
    include:
      - { os: windows-latest, preset: win64-msvc-debug }
      - { os: windows-latest, preset: win64-msvc-release }
      - { os: macos-latest,   preset: macos-clang-debug }
      - { os: macos-latest,   preset: macos-clang-release }
```

PR 准入门槛：

- Configure 成功
- Build 成功
- Static Audit(grep 无 placeholder 或 XOR)
- Unit、Integration、Smoke 全部 100% 通过

---

## 编写测试

### Qt Test 模板

```cpp
#include <QTest>
#include <QSignalSpy>
#include "Margin/EventBus.h"

class TestEventBus : public QObject {
    Q_OBJECT
private slots:
    void testPublishSubscribe();
    void testWildcardTopic();
};

void TestEventBus::testPublishSubscribe() {
    EventBus bus;
    int received = 0;
    bus.subscribe("margin.hello.ping", [&](const QJsonObject&) {
        received++;
    });
    bus.publish("margin.hello.ping", {});

    // publish 经 Qt::QueuedConnection 派发,需要 event loop 才能落地
    // 使用 QTRY_COMPARE 而非 QTest::qWait(固定 sleep):条件达成即返回,
    // 在 CI 环境差异下也不会出现不稳定结果
    QTRY_COMPARE(received, 1);   // 默认 5 秒超时
}

QTEST_MAIN(TestEventBus)
#include "test_event_bus.moc"
```

### 测试反模式（禁止）

| 反模式 | 后果 | 正确做法 |
|---|---|---|
| "先合并后补测试" | 永远补不上 | 同 PR 必须有测试 |
| Mock 系统调用 | 生产时失败但 mock 时通过 | 集成测试使用真实 DPAPI 与 SQLite |
| `QTest::qWait(固定 ms)` | CI 慢时易出现不稳定结果，CI 快时浪费时长 | 使用 `QSignalSpy::wait()`、`QTRY_VERIFY` 或 `QTRY_COMPARE`（条件达成即返回） |
| 仅在 Windows 测试 | macOS 上崩溃 | CI 需 Windows 与 macOS 均执行 |
| 仅在 Debug 测试 | Release 优化下出现缺陷 | CI 需 Debug 与 Release 均执行 |

---

## 加密 round-trip 测试

`tests/integration/test_encryption_roundtrip.cpp` 是关键守护测试，确保
Settings 透明加密与 CryptoService 在真实 DPAPI 或 Keychain 下能完成往返：

```cpp
void TestEncryptionRoundtrip::testSettingsRoundtrip() {
    // 1. set 加密字段 → 实际落盘是密文
    // 2. get → 拿回明文
    // 3. 直接读 settings.json → 应看到 __encrypted__: true
}

void TestEncryptionRoundtrip::testCryptoServiceIsolation() {
    // 1. plugin A encryptString("hello")
    // 2. plugin B decryptString(ct) → 失败(AES-GCM tag 校验)
}
```

---

## 验收清单（每个 Milestone)

每个 Milestone 完成时，维护者按 `tests/manual/M*_ACCEPTANCE.md` 实机执行
一次。示例（M1 Aura Locker 核心三条）:

1. 配对后离开 5 米 + 30 秒 → 屏幕锁定
2. 解锁后 60 秒 cooldown 内不重锁
3. 关闭蓝牙模块 → warning + 不锁屏

每个 Milestone 还包含"零网络流量"用例（Wireshark 抓包 5 分钟，预期出站流量为零）。

---

## 进一步阅读

- 从源码构建：[03-build-from-source.md](03-build-from-source.md)
- PR 流程与测试要求：[CONTRIBUTING.md](CONTRIBUTING.md)
- 加密测试要求：[07-privacy-security.md](07-privacy-security.md)
