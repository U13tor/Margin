# 03 — 从源码构建

面向贡献者及希望自行验证的用户。终端用户安装请参阅 [02-install.md](02-install.md)。

---

## 前置依赖

### 通用

| 项 | 版本 |
|---|---|
| Git | 任意 |
| CMake | 3.21+ |
| vcpkg | 最新 master(manifest mode) |
| Qt | 6.7+ |

### Windows

| 项 | 版本 | 备注 |
|---|---|---|
| Visual Studio 2022 | 17.x | 需含"Desktop development with C++"工作负载 |
| Windows SDK | 10.0.19041+ | 通过 VS Installer 勾选 |
| 系统版本 | Windows 10 1809+ | 必需 |

VS Installer 需勾选以下组件：MSVC v143 build tools、Windows 10/11 SDK、CMake tools for Windows;**不要**安装 MinGW。

### macOS

| 项 | 版本 |
|---|---|
| Xcode | 14.0+ |
| Command Line Tools | 最新版本（`xcode-select --install`) |
| Homebrew | 最新版本 |
| CMake | 3.21+(`brew install cmake`) |
| Ninja | 1.10+(`brew install ninja`) |
| 系统版本 | macOS 11+ |

---

## Qt 安装（必读）

Margin **不通过 vcpkg 安装 Qt**。可从以下两种方案中任选其一，安装完成后通过
`Qt6_DIR` 环境变量告知 CMake Qt 的位置。

### 方案 A:Qt 官方在线安装器

适合个人开发机，首次使用需注册 Qt 账号（免费）。

1. 注册账号：<https://login.qt.io/register>
2. 下载 open-source installer:<https://www.qt.io/download-open-source>
3. 登录后选择组件：`Qt 6.7.x` 与 `MSMC 2019 64-bit`(Windows）或 `macOS`(macOS)。VS 2022 toolset 与 msvc2019_64 目录 ABI 兼容,Qt 官方在 6.7.x 仍沿用此目录名
4. 设置环境变量：

   ```powershell
   # Windows PowerShell(持久化)
   [Environment]::SetEnvironmentVariable("Qt6_DIR", "C:\Qt\6.7.3\msvc2019_64", "User")
   ```

   ```bash
   # macOS zsh
   echo 'export Qt6_DIR=~/Qt/6.7.3/macos' >> ~/.zshrc
   ```

下载体积约 3–4 GB，安装完成后约 10 GB。

### 方案 B:aqt（命令行，推荐）

`aqtinstall` 是 Qt 官方 binary 的命令行下载工具，GitHub Actions CI 亦采用此方案。
首次安装速度优于方案 A。

```powershell
# Windows
pip install aqtinstall
python -m aqt install-qt windows desktop 6.7.3 win64_msvc2019_64 -O C:\Qt
[Environment]::SetEnvironmentVariable("Qt6_DIR", "C:\Qt\6.7.3\msvc2019_64", "User")
```

```bash
# macOS
pip3 install aqtinstall
python3 -m aqt install-qt mac desktop 6.7.3 clang_64 -O ~/Qt
echo 'export Qt6_DIR=~/Qt/6.7.3/macos' >> ~/.zshrc
```

下载体积约 1.5 GB。

### 验证

```powershell
# Windows
echo $env:Qt6_DIR
Test-Path "$env:Qt6_DIR\lib\Qt6Core.lib"   # True 表示配置正确
```

```bash
# macOS
test -f "$Qt6_DIR/lib/Qt6Core.framework/Qt6Core" && echo OK
```

---

## 克隆与配置

```bash
git clone https://github.com/U13tor/Margin.git
cd Margin
```

设置 vcpkg（路径不限，只需保证 `VCPKG_ROOT` 一致）:

```powershell
# Windows PowerShell
$env:VCPKG_ROOT = "C:\dev\vcpkg"   # 请替换为实际路径
```

```bash
# macOS
export VCPKG_ROOT=~/dev/vcpkg
```

如尚未克隆 vcpkg:

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh   # Windows 下使用 bootstrap-vcpkg.bat
```

---

## 一键构建（推荐）

> **Windows 用户**:`build-smoke.sh` 是 bash 脚本，**必须在 Git Bash、
> MSYS2 或 WSL 中执行**，不要在 PowerShell 或 cmd 中运行（脚本依赖 bash
> 语法，并会自动调用 `vcvars64.bat` 引入 MSVC 环境）。Git Bash 随
> [Git for Windows](https://git-scm.com/download/win) 一并安装。

```bash
bash scripts/build-smoke.sh
```

`build-smoke.sh` 会自动检测平台、配置环境，并依次执行 5 个步骤：configure、build、ctest、grep 审计与启动二进制验证。全部通过后方可合并。

显式指定 preset:

```bash
bash scripts/build-smoke.sh win64-msvc-debug    # Windows
bash scripts/build-smoke.sh macos-clang-debug   # macOS
```

---

## 手动构建

如需分步执行，可参考以下流程：

```powershell
# Windows PowerShell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
$env:Qt6_DIR = "C:\Qt\6.7.3\msvc2019_64"
# ctest 运行测试时,Qt DLL 必须在 PATH 中,否则会报"找不到 Qt6Cored.dll"
$env:PATH = "$env:Qt6_DIR\bin;$env:PATH"

cmake --preset win64-msvc-debug
cmake --build build/win64-msvc-debug
ctest --test-dir build/win64-msvc-debug --output-on-failure
```

```bash
# macOS
export VCPKG_ROOT=~/dev/vcpkg
export Qt6_DIR=~/Qt/6.7.3/macos

cmake --preset macos-clang-debug
cmake --build build/macos-clang-debug
ctest --test-dir build/macos-clang-debug --output-on-failure
```

> **Windows 注意**:`Qt6_DIR` 仅用于让 **CMake configure** 阶段找到 Qt。
> **测试运行时**仍需将 `Qt6_DIR\bin` 加入 PATH（见上方第 3 行），否则
> `ctest` 加载 `Qt6Cored.dll` 会失败。不建议在外部 Git Bash 中直接运行
> `ctest`（其 PATH 中不含 Qt)。可选择在 PowerShell 中运行（如上），或
> 直接使用 `bash scripts/build-smoke.sh` 由脚本自动处理环境。

---

## 启动

```powershell
# Windows
.\build\win64-msvc-debug\src\host\Margin.exe
```

```bash
# macOS
./build/macos-clang-debug/src/host/Margin
```

---

## 构建产物

| 平台 | 产物 |
|---|---|
| Windows | `Margin.exe` 与 `plugins/*.dll`(`hello.dll`、`aura.dll` 等） |
| macOS | `Margin` 与 `plugins/*.dylib` |

---

## C++/WinRT(Windows 蓝牙后端）

Aura Locker 的 BLE 邻近监控采用 Windows 原生 C++/WinRT，而非 Qt Bluetooth。
配置阶段会自动调用 `cppwinrt.exe` 生成 projection 头文件至 `build/_cppwinrt/`,
无需手动操作。

如遇 `'winrt/base.h': file not found` 错误：

```bash
ls build/_cppwinrt/winrt/   # 应能看到 windows.devices.bluetooth.* 头文件
```

如目录为空，请删除 `build/` 后重新执行 configure。

---

## 排错 FAQ

### `Could not find Qt6`

请检查 `Qt6_DIR` 是否指向 `msvc2019_64`(Windows）或 `macos`(macOS),
且目录中存在 `lib/Qt6Core.lib` 或 `lib/Qt6Core.framework/Qt6Core`。
若不存在，说明 Qt 安装不完整。

### `vcpkg manifest mode requires vcpkg-execute`

通常为 `vcpkg` 未完成克隆，或 `VCPKG_ROOT` 路径配置有误。请重新克隆 vcpkg 并执行 bootstrap。

### 测试 `0xc0000135` 启动失败

PATH 中混入了旧版 Qt DLL。请从构建目录直接运行 ctest，不要在外部 shell 中设置全局 Qt PATH。

### 其他问题

遇到本文档未涵盖的构建、启动或测试问题，请直接提交 Issue 并描述症状，
维护者会复现并补充至本文档。

---

## 下一步

构建成功后，可继续参阅：

- [01-architecture.md](01-architecture.md) — 理解整体架构
- [04-plugin-spec.md](04-plugin-spec.md) — 编写第一个插件
- [09-testing.md](09-testing.md) — 测试策略
- [CONTRIBUTING.md](CONTRIBUTING.md) — PR 流程
