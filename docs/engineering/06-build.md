# 构建

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 1. 工具链

| 组件 | 版本 | 获取方式 |
|---|---|---|
| Visual Studio Build Tools | 2022 17.10+ | 官方安装器，勾选"使用 C++ 的桌面开发" |
| MSVC | v143，19.40+ | 随 Build Tools |
| Windows SDK | 10.0.26100 | 随 Build Tools |
| CMake | 3.28+ | 官方安装包或 VS 内置 |
| Ninja | 1.11+ | VS 内置或独立安装 |
| Git | 2.40+ | 官方安装包 |

构建需要在 "x64 Native Tools Command Prompt for VS 2022" 中执行，或使用 VS Code 的 CMake Tools 自动注入环境。

## 2. 目录与目标

```
TinyTheme/
├── CMakeLists.txt
├── CMakePresets.json
├── src/
├── res/
├── tests/
└── build/            # 生成目录，不入库
```

两个目标：

| 目标 | 类型 | 说明 |
|---|---|---|
| `TinyTheme` | `WIN32` 可执行文件 | 发布物 |
| `tinytheme_tests` | 控制台可执行文件 | 单元测试，仅本地与 CI 构建；使用 `tests/third_party/doctest` |

> v0.1 的 `tests/` 只覆盖配置解析、模式映射等纯逻辑；`solar` / `scheduler` / `hotkey` 测试随 v0.2 模块引入。

## 3. CMake 配置

```cmake
cmake_minimum_required(VERSION 3.28)
project(TinyTheme VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 供 clang-tidy 读取

add_executable(TinyTheme WIN32
    src/main.cpp
    src/app.cpp
    src/tray.cpp
    src/menu.cpp
    src/theme.cpp
    src/config.cpp
    src/toast.cpp
    src/log.cpp
    src/util.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc)

# v0.1 不加入 scheduler.cpp / solar.cpp / hotkey.cpp；v0.2 再加。

target_compile_definitions(TinyTheme PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

target_compile_options(TinyTheme PRIVATE
    /W4 /WX /permissive- /utf-8 /EHsc /GR- /Brepro
    /O2 /Oi /Gy /Gw /GL)

target_link_options(TinyTheme PRIVATE
    /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF
    /INCREMENTAL:NO /MANIFEST:EMBED
    /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/res/app.manifest
    /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)

set_property(TARGET TinyTheme PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(TinyTheme PRIVATE
    user32 shell32 advapi32 gdi32 dwmapi shlwapi ole32)

enable_testing()
add_subdirectory(tests)
```

## 4. 编译与链接选项

| 选项 | 作用 | 备注 |
|---|---|---|
| `/W4 /WX` | 高警告等级并视为错误 | 保持零警告 |
| `/permissive-` | 标准一致性 | 必开 |
| `/std:c++latest` | C++23 语言与库 | 由 `CMAKE_CXX_STANDARD 23` 注入，不重复写进 `target_compile_options` |
| `/utf-8` | 源码与执行字符集统一 UTF-8 | 源码含中文注释时必须 |
| `/EHsc` | 标准异常模型 | STL 需要 |
| `/GR-` | 关闭 RTTI | 不使用 `dynamic_cast` / `typeid` |
| `/O2 /Oi` | 优化 | 发布配置 |
| `/Gy /Gw` | 函数级与数据段合并 | 配合 `/OPT:ICF` 减体积 |
| `/GL` + `/LTCG` | 全程序优化 | 发布配置 |
| `/Brepro` | 可复现构建 | 便于签名与校验 |
| `/MT` | 静态链接 C 运行库 | 免 VC++ Redist |
| `/SUBSYSTEM:WINDOWS` | 无控制台窗口 | 入口 `wWinMain` |
| `/MANIFEST:EMBED` | manifest 嵌入 exe | 不产生外部文件 |

Debug 配置关闭 `/GL`、`/LTCG`、`/O2`，改用 `/Od /Zi /RTC1`，链接 `/DEBUG`。

## 5. CMake Presets

### 5.1 生成器与编译器的关系

`"generator": "Ninja"` 只指定**构建执行器**，不指定编译器。CMake 会在当前 `PATH` 里找第一个可用的编译器，所以同一个预设可能编出完全不同的产物：

| 执行方式 | 编译器 | 结果 |
|---|---|---|
| `pwsh -File tools/build.ps1 -Preset x64-release` | 脚本先调 `vcvarsall.bat x64`，MSVC 排到 `PATH` 前面 | MSVC ✅ |
| 普通终端 `cmake --preset x64-release` | `PATH` 里若先有 MinGW 的 `g++.exe` | MinGW ❌ |

因此预设里**显式钉死编译器**：

```json
"cacheVariables": {
  "CMAKE_BUILD_TYPE": "Release",
  "CMAKE_CXX_COMPILER": "cl",
  "CMAKE_RC_COMPILER": "rc"
}
```

这样在没有 `vcvars` 的环境里运行会立刻报 `CMAKE_CXX_COMPILER: cl ... not found in the PATH`，而不是静默换成 MinGW。

### 5.2 为什么主用 Ninja 而不是 Visual Studio 生成器

| 维度 | Ninja | Visual Studio 17 2022 |
|---|---|---|
| `compile_commands.json` | 支持，clang-tidy 直接可读 | **不支持**，CI 的静态分析门禁需要额外造编译数据库 |
| 增量构建 | 快，启动开销小 | 慢一些 |
| 产物路径 | `build/<preset>/TinyTheme.exe` | `build/<preset>/Release/TinyTheme.exe`（多一层） |
| 环境要求 | 需要 `vcvars` 提供 `cl` | MSBuild 自己找工具链，无需 `vcvars` |
| IDE 集成 | 一般 | 好 |

结论：**CI 与发布用 Ninja**（单配置、路径简单、支持 compile_commands）；Visual Studio 生成器作为受限环境或 IDE 调试的备选。

### 5.3 预设一览

| 预设 | 生成器 | 用途 |
|---|---|---|
| `x64-debug` | Ninja | 本地调试，需 `vcvars64` |
| `x64-release` | Ninja | CI、发布主路径，需 `vcvars64` |
| `arm64-release` | Ninja | ARM64，需 `vcvarsall arm64` 与 ARM64 工具集 |
| `vs2022-x64-release` | Visual Studio 17 2022 | 无需 `vcvars`；受限环境、IDE；不产出 `compile_commands.json` |

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "x64-release",
      "displayName": "x64 Release (MSVC + Ninja)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/x64-release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_COMPILER": "cl",
        "CMAKE_RC_COMPILER": "rc"
      }
    },
    {
      "name": "vs2022-x64-release",
      "displayName": "x64 Release (Visual Studio 2022)",
      "generator": "Visual Studio 17 2022",
      "architecture": { "value": "x64", "strategy": "set" },
      "binaryDir": "${sourceDir}/build/vs2022-x64-release"
    }
  ],
  "buildPresets": [
    { "name": "x64-release", "configurePreset": "x64-release" },
    {
      "name": "vs2022-x64-release",
      "configurePreset": "vs2022-x64-release",
      "configuration": "Release"
    }
  ],
  "testPresets": [
    {
      "name": "x64-release",
      "configurePreset": "x64-release",
      "output": { "outputOnFailure": true }
    },
    {
      "name": "vs2022-x64-release",
      "configurePreset": "vs2022-x64-release",
      "configuration": "Release",
      "output": { "outputOnFailure": true }
    }
  ]
}
```


## 6. 资源与 manifest

### app.rc

- `app.rc.in` 由 CMake 生成到构建目录，图标使用绝对路径，避免 `rc.exe` 工作目录差异。
- `ICON`：`IDI_LIGHT`、`IDI_DARK` 两套图标，各含 16 / 20 / 24 / 32 四种尺寸；由 `res/*.svg` 生成后入库，流程见 [图标生成](10-icon-pipeline.md)。
- `VERSIONINFO`：版本号由 CMake 生成，禁止手工维护。
- `STRINGTABLE`：v0.1 只出 `zh-CN`；`IDS_TOAST_HOTKEY_FAIL`（203）随 F-02 于 v0.2 加入。

### app.manifest

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity type="win32" name="TinyTheme" version="0.1.0.0"/>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
    </windowsSettings>
  </application>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
</assembly>
```

## 7. 构建命令

```powershell
# 在 x64 Native Tools Command Prompt 中
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release

# 静态分析：读取 build/x64-debug/compile_commands.json
clang-tidy -p build/x64-debug src/app.cpp
# 或对整个项目：run-clang-tidy -p build/x64-debug
```

产物：`build/x64-release/TinyTheme.exe`

## 8. 产物与打包

| 文件 | 说明 |
|---|---|
| `TinyTheme.exe` | 唯一发布物 |
| `README.md` | 使用说明 |
| `LICENSE` | 许可证 |

打包为 `TinyTheme-v0.1.0-x64.zip`，zip 内保持扁平结构，解压即用。

## 9. 发布前核对

| 项 | 方法 | 目标 |
|---|---|---|
| 体积 | `Get-Item TinyTheme.exe` | 记录实测值并与上一版本对比；明显膨胀需说明 |
| 依赖 | `dumpbin /dependents` | 仅系统 DLL |
| 冷启动 | 任务管理器"启动影响"或 `Measure-Command` | < 100 ms |
| 常驻内存 | `Get-Process TinyTheme` 的 `WorkingSet64` | < 10 MB |
| 句柄数 | `Get-Process` 的 `HandleCount` | < 50 |
| 导入表 | `dumpbin /imports` | 无意外 DLL |
| 版本资源 | 文件属性 → 详细信息 | 与 tag 一致 |
| 签名 | `Get-AuthenticodeSignature` | `Valid` |

## 9.1 实测数据

> 记录规则：这里只写**实测值**；目标值见上表，不要把目标值抄进来。每次发布前追加一行。

### v0.1.0（2026-11-01，x64 Release，VS 2022 生成器）

| 项 | 实测 | 备注 |
|---|---|---|
| 体积 | 257,024 字节（251 KB） | `D:\coding\build\TinyTheme\build-vs2022\Release\TinyTheme.exe` |
| 依赖 | USER32 / SHELL32 / ADVAPI32 / ole32 / KERNEL32 | `/MT` 生效，无 VCRUNTIME |
| 导入表 | 无 WS2_32 / WININET / WINHTTP / URLMON | 不发起网络请求 |
| 常驻工作集 | 6.7 MB | 用户实测，满足 < 10 MB |
| 版本资源 | 0.1.0（TinyTheme / TinyTheme 0.1.0） | 与 `CMakeLists.txt` 一致 |
| 签名 | NotSigned | 本地 RC 阶段，公开发布前必须签名 |
| 冷启动 | 待测 | 需在真实桌面用 QPC 日志或屏幕录制 |
| 句柄数 / GDI / 线程 | 待测 | 需运行中采样 |
| 空闲 CPU | 待测 | 需 60 s 采样 |

## 10. 常见构建问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `cl.exe` 找不到 | 未在 VS 开发者命令行 | 用 Native Tools 命令提示符，或 `pwsh -File tools/build.ps1`（内部调用 `vcvarsall.bat`） |
| 中文乱码 | 源文件编码或 `/utf-8` 缺失 | 统一 UTF-8 + `/utf-8` |
| `wWinMain` 链接错误 | 未加 `WIN32` 或子系统不对 | 检查 `add_executable(... WIN32 ...)` |
| 资源编译失败 | `.rc` 引用的图标路径错误 | `app.rc` 由 CMake 生成，图标用绝对路径；检查 `res/*.ico` 是否存在 |
| ARM64 链接失败 | 缺少 ARM64 版库 | 确认 VS 安装了 ARM64 组件；本机未安装时用 CI |
| `ninja` 挂起、configure 卡在 ABI 检测 | 受限沙箱禁止子进程命名管道，ninja 无法回收编译输出 | 该环境用 `cmake -G "NMake Makefiles"` 或 `-G "Visual Studio 17 2022"` 构建；CI 与正常桌面仍用 Ninja |
| 运行时报"无法创建托盘图标"，日志是 `NIM_ADD` 错误 5 | 在低权限工作区/受限沙箱里直接运行，进程无法与资源管理器交互 | 把 exe 复制到普通目录再运行；这是运行环境限制，不是程序缺陷 |
| 双击 exe 出现"Windows 已保护你的电脑" | 未签名构建触发 SmartScreen | 本地测试点"更多信息 → 仍要运行"；对外分发必须先签名 |
