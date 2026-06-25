# 03 — 从源码构建

面向贡献者与想自行验证的用户。终端用户安装看 [02-install.md](02-install.md)。

---

## 前置依赖

### 通用

| 项 | 版本 |
|---|---|
| Git | 任意 |
| CMake | 3.21+ |
| vcpkg | 最新 master(manifest mode) |
| Qt | 6.5+ |

### Windows

| 项 | 版本 | 备注 |
|---|---|---|
| Visual Studio 2022 | 17.x | "Desktop development with C++" workload |
| Windows SDK | 10.0.19041+ | VS Installer 勾选 |
| 系统版本 | Win 10 1809+ | 必需 |

VS Installer 必勾:MSVC v143 build tools、Windows 10/11 SDK、C++ CMake
tools for Windows、**不要** MinGW。

### macOS

| 项 | 版本 |
|---|---|
| Xcode | 14.0+ |
| Command Line Tools | 最新(`xcode-select --install`) |
| Homebrew | 最新 |
| CMake | 3.21+(`brew install cmake`) |
| Ninja | 1.10+(`brew install ninja`) |
| 系统版本 | macOS 11+ |

---

## Qt 安装(必读)

Margin **不通过 vcpkg 安装 Qt**。从下面两种方案任选其一,装完都通过
`Qt6_DIR` 环境变量告诉 CMake 去哪儿找。

### 方案 A:Qt 官方在线安装器

适合个人开发机,首次需注册 Qt 账号(免费)。

1. 注册:<https://login.qt.io/register>
2. 下载 open-source installer:<https://www.qt.io/download-open-source>
3. 登录后选组件:`Qt 6.5.x` + `MSVC 2019 64-bit`(Windows)/ `macOS`(Mac)
4. 设置环境变量:

   ```powershell
   # Windows PowerShell(持久化)
   [Environment]::SetEnvironmentVariable("Qt6_DIR", "C:\Qt\6.5.3\msvc2019_64", "User")
   ```

   ```bash
   # macOS zsh
   echo 'export Qt6_DIR=~/Qt/6.5.3/macos' >> ~/.zshrc
   ```

下载约 3-4 GB,装好约 10 GB。

### 方案 B:aqt(命令行,推荐)

`aqtinstall` 是 Qt 官方 binary 的命令行下载器,GitHub Actions CI 也用它。
首次比方案 A 快。

```powershell
# Windows
pip install aqtinstall
python -m aqt install-qt windows desktop 6.5.3 win64_msvc2019_64 -O C:\Qt
[Environment]::SetEnvironmentVariable("Qt6_DIR", "C:\Qt\6.5.3\msvc2019_64", "User")
```

```bash
# macOS
pip3 install aqtinstall
python3 -m aqt install-qt mac desktop 6.5.3 clang_64 -O ~/Qt
echo 'export Qt6_DIR=~/Qt/6.5.3/macos' >> ~/.zshrc
```

下载约 1.5 GB。

### 验证

```powershell
# Windows
echo $env:Qt6_DIR
Test-Path "$env:Qt6_DIR\lib\Qt6Core.lib"   # True = OK
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

设置 vcpkg(任意路径,只需 `VCPKG_ROOT` 一致):

```powershell
# Windows PowerShell
$env:VCPKG_ROOT = "C:\dev\vcpkg"   # 改成你的实际路径
```

```bash
# macOS
export VCPKG_ROOT=~/dev/vcpkg
```

如果你还没 clone vcpkg:

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh   # or bootstrap-vcpkg.bat on Windows
```

---

## 一键构建(推荐)

> **Windows 用户**:`build-smoke.sh` 是 bash 脚本,**必须在 Git Bash /
> MSYS2 / WSL 里跑**,不要在 PowerShell 或 cmd 里执行(脚本依赖 bash
> 语法 + 自动调 `vcvars64.bat` 引入 MSVC 环境)。Git Bash 随
> [Git for Windows](https://git-scm.com/download/win) 一起装。

```bash
bash scripts/build-smoke.sh
```

`build-smoke.sh` 自动检测平台、配置环境、跑 5 步:configure → build →
ctest → grep 审计 → 启动二进制验证。全绿才能合并。

显式指定 preset:

```bash
bash scripts/build-smoke.sh win64-msvc-debug    # Windows
bash scripts/build-smoke.sh macos-clang-debug   # macOS
```

---

## 手动构建

如果你想分步跑:

```powershell
# Windows PowerShell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
$env:Qt6_DIR = "C:\Qt\6.5.3\msvc2019_64"
# ctest 跑测试时 Qt DLL 必须在 PATH 里,否则会报"找不到 Qt6Cored.dll"
$env:PATH = "$env:Qt6_DIR\bin;$env:PATH"

cmake --preset win64-msvc-debug
cmake --build build/win64-msvc-debug
ctest --test-dir build/win64-msvc-debug --output-on-failure
```

```bash
# macOS
export VCPKG_ROOT=~/dev/vcpkg
export Qt6_DIR=~/Qt/6.5.3/macos

cmake --preset macos-clang-debug
cmake --build build/macos-clang-debug
ctest --test-dir build/macos-clang-debug --output-on-failure
```

> **Windows 注意**:`Qt6_DIR` 只让 **CMake configure** 找到 Qt。**测试运行时**
> 还需要 `Qt6_DIR\bin` 在 PATH 里(上面第 3 行已加),否则 `ctest` 加载
> `Qt6Cored.dll` 失败。**不要**在外部 Git Bash 直接跑 `ctest`——
> 它的 PATH 没有 Qt。要么在 PowerShell 里跑(如上),要么直接用
> `bash scripts/build-smoke.sh` 让脚本帮你处理。

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
| Windows | `Margin.exe` + `plugins/*.dll`(`hello.dll` / `aura.dll` 等) |
| macOS | `Margin` + `plugins/*.dylib` |

---

## C++/WinRT(Windows 蓝牙后端)

Aura Locker 的 BLE 邻近监控用 Windows 原生 C++/WinRT 而非 Qt Bluetooth。
配置阶段会自动调 `cppwinrt.exe` 生成 projection 头到 `build/_cppwinrt/`,
无需手动操作。

如果遇到 `'winrt/base.h': file not found`:

```bash
ls build/_cppwinrt/winrt/   # 应能看到 windows.devices.bluetooth.* 头
```

如果目录为空,删除 `build/` 重新 configure。

---

## 排错 FAQ

### `Could not find Qt6`

检查 `Qt6_DIR` 是否指向 `msvc2019_64`(Win)或 `macos`(Mac),且目录里
存在 `lib/Qt6Core.lib` 或 `lib/Qt6Core.framework/Qt6Core`。不存在说明
Qt 没装好。

### `vcpkg manifest mode requires vcpkg-execute`

`vcpkg` 没 clone 或 `VCPKG_ROOT` 指错。重新 clone vcpkg 并 bootstrap。

### 测试 `0xc0000135` 启动失败

PATH 中混入了旧版 Qt DLL。从构建目录直接跑 ctest,不要在外部 shell 设
全局 Qt PATH。

### 更多坑

遇到本文档没列出的构建 / 启动 / 测试坑,直接提 Issue 描述症状,
维护者会复现并补到本文档。

---

## 下一步

构建成功后,看:

- [01-architecture.md](01-architecture.md) — 理解整体架构
- [04-plugin-spec.md](04-plugin-spec.md) — 写第一个插件
- [09-testing.md](09-testing.md) — 测试策略
- [CONTRIBUTING.md](CONTRIBUTING.md) — PR 流程
