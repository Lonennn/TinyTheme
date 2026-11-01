# 构建

> 状态：v0.1 已实现 ｜ 更新：2026-11-01

构建参数、目标定义、预设内容都在 `CMakeLists.txt`、`CMakePresets.json`、`tools/build.ps1` 里，本文只记录**取舍与坑**。

## 1. 工具链

| 组件 | 要求 |
|---|---|
| Visual Studio 2022 | 勾选"使用 C++ 的桌面开发"（v143 工具集） |
| Windows SDK | 10.0.26100 |
| CMake | 3.28+ |
| Ninja | 1.11+（默认生成器；没有时可改用 NMake 或 VS 生成器） |

`cl.exe` 默认不在 `PATH` 里，`tools/build.ps1` 会用 `vswhere` 定位 VS 并调用 `vcvarsall.bat` 注入环境。

## 2. 构建目标

| 目标 | 类型 | 说明 |
|---|---|---|
| `TinyTheme` | `WIN32` 可执行文件 | 发布物 |
| `tinytheme_tests` | 控制台可执行文件 | doctest 2.5.3（vendor 于 `tests/third_party`），不进发布物 |

## 3. 配置取舍

以下几处不是随手写死，改动前先看理由：

- **`/MT`**：静态链接 C 运行库，用户机器不需要 VC++ Redist。代价是体积增加几十 KB。
- **`/GR-`**：关闭 RTTI。代码里不用 `dynamic_cast` / `typeid`，关掉能减体积并让误用直接编译失败。
- **`/Brepro`**：可复现构建，便于日后签名与校验。
- **`/utf-8`**：源码与执行字符集统一 UTF-8，中文注释和字符串才不会乱码。
- **`/W4 /WX`**：警告即错误。新增代码必须零警告，否则 CI 直接挂。
- **`/LTCG` + `/OPT:REF` + `/OPT:ICF`**：全程序优化与去重，Release 专用；Debug 关掉以换取编译速度。
- **资源文件由 CMake 生成**：`.rc` 模板里用绝对路径引图标，`VERSIONINFO` 用 `project(VERSION ...)` 注入版本号——版本只有一个来源。
- **manifest 用 `/MANIFESTINPUT` 嵌入**：不额外产生外部文件，exe 仍是单文件。

## 4. 预设与生成器

### 4.1 `"generator": "Ninja"` 不指定编译器

这一点最容易踩。CMake 只是按 `PATH` 顺序找第一个可用编译器：

| 执行方式 | 编译器 |
|---|---|
| `pwsh -File tools/build.ps1` | 先执行 `vcvarsall.bat`，MSVC 在 `PATH` 头部 → **MSVC** ✅ |
| 普通终端直接 `cmake --preset x64-release` | 若 `PATH` 里先有 MinGW 的 `g++.exe` → **MinGW** ❌ |

所以三个 Ninja 预设都显式写了 `CMAKE_CXX_COMPILER=cl` 与 `CMAKE_RC_COMPILER=rc`：没有 `vcvars` 时会立刻报"cl 找不到"，而不是静默换编译器。

### 4.2 为什么主用 Ninja

- **产出 `compile_commands.json`**：clang-tidy 门禁依赖它，而 Visual Studio 生成器不产出该文件。这是决定性理由。
- 增量构建更快，启动开销更小。
- 单配置产物路径就是 `build/<preset>/TinyTheme.exe`，打包与 CI 都不用处理多出来的 `Release/` 层级。

### 4.3 预设一览

| 预设 | 生成器 | 何时用 |
|---|---|---|
| `x64-debug` / `x64-release` | Ninja | CI 与发布主路径；必须带 `vcvars` |
| `arm64-release` | Ninja | ARM64；需要 ARM64 工具集与 `vcvarsall arm64` |
| `vs2022-x64-release` | Visual Studio 17 2022 | 受限环境、IDE 调试；**不需要 `vcvars`**，但不产出 `compile_commands.json` |

## 5. 资源与 manifest

- `res/app.rc.in` 由 CMake 生成到构建目录；图标用绝对路径，避免 `rc.exe` 的工作目录差异。
- `res/app.manifest` 必须包含 `PerMonitorV2` 与 `asInvoker`：前者决定 DPI 行为，后者保证不触发 UAC。
- 图标资产（`res/*.ico`）预生成后入库，CI 只调 `rc.exe`，见 [图标生成](10-icon-pipeline.md)。

## 6. 构建命令

```powershell
pwsh -File tools/build.ps1 -Preset x64-release -Test          # Ninja
pwsh -File tools/build.ps1 -Preset x64-release -Generator vs2022 -Test
```

或手工：

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release

# 静态分析需要 Ninja / NMake 生成的编译数据库
clang-tidy -p build/x64-debug src/app.cpp
```

产物：`build/x64-release/TinyTheme.exe`（VS 生成器在 `build/vs2022-x64-release/Release/`）。

### 静态分析的两个前提

1. **编译数据库只有 Ninja / NMake 生成器会产出**，VS 生成器不产出 `compile_commands.json`。
2. **必须用 MSVC 目标的 clang-tidy**，即 VS 自带的那个：

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe" `
    -p build/tidy --quiet src\app.cpp
```

用 MinGW 目录下的 `clang-tidy`（默认 target `windows-gnu`）会报一串
`'windows.h' file not found`，看着像代码错误，其实是工具链不匹配——两种工具的分类结果完全不同（实测 63 条 vs 109 条）。

## 7. 打包

发布包是**扁平 zip**：`TinyTheme.exe` + `README.md` + `LICENSE`，外加一份 `SHA256SUMS.txt`。

```powershell
pwsh -File tools/package.ps1 -Version 0.1.0 -Generator vs2022
```

脚本会先校验 exe 的版本资源与参数一致，再打包，不会打出"版本号对不上"的包。

## 8. 发布前核对

| 项 | 方法 | 目标 |
|---|---|---|
| 体积 | `Get-Item TinyTheme.exe` | 记录实测值并与上一版对比；明显膨胀需说明 |
| 依赖 | `dumpbin /dependents` | 仅系统 DLL |
| 导入表 | `dumpbin /imports` | 无网络相关 DLL |
| 冷启动 | 任务管理器"启动影响"或屏幕录制 | < 100 ms 到托盘出现 |
| 常驻内存 | `Get-Process` 的 `WorkingSet64` | < 10 MB |
| 句柄 / GDI / 线程 | `Get-Process`、任务管理器 | < 50 / < 20 / 1 |
| 版本资源 | 文件属性 → 详细信息 | 与 tag 一致 |
| 签名 | `Get-AuthenticodeSignature` | `Valid` |

### 8.1 实测数据

> 只写实测值；目标值在上表，不要抄过来。每次发布追加一列或一行。

v0.1.0（2026-11-01，x64 Release，MSVC 19.44）：

| 项 | 实测 |
|---|---|
| 体积 | 257,024 字节（251 KB） |
| 依赖 | USER32 / SHELL32 / ADVAPI32 / ole32 / KERNEL32（无 VCRUNTIME） |
| 导入表 | 无 WS2_32 / WININET / WINHTTP / URLMON |
| 常驻工作集 | 6.7 MB |
| 版本资源 | 0.1.0 |
| 签名 | NotSigned（本地 RC 阶段） |
| 冷启动 / 句柄 / GDI / 空闲 CPU | 待测 |

## 9. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `cl.exe` 找不到 | 没进 vcvars 环境 | 用 `tools/build.ps1`，或 Native Tools 命令提示符 |
| 中文乱码 | 源码编码或 `/utf-8` 缺失 | 统一 UTF-8 无 BOM + `/utf-8` |
| `wWinMain` 链接错误 | 目标没加 `WIN32` | 检查 `add_executable(... WIN32 ...)` |
| 资源编译失败 | 图标路径不对 | `.rc` 由 CMake 生成，确认 `res/*.ico` 存在 |
| ARM64 链接失败 | 没装 ARM64 组件 | 在 VS Installer 里补装，或交给 CI |
| `ninja` 卡在编译器 ABI 检测 | 受限环境禁止子进程命名管道 | 改用 `-Generator vs2022` 或 `nmake` |
| 运行时报"无法创建托盘图标"，日志是 `NIM_ADD` 错误 5 | 在低权限目录或受限沙箱里运行，进程无法与资源管理器交互 | 把 exe 复制到普通目录再运行；这不是程序缺陷 |
| 双击 exe 提示"Windows 已保护你的电脑" | 未签名触发 SmartScreen | 本地测试选"更多信息 → 仍要运行"；对外发布必须先签名 |
