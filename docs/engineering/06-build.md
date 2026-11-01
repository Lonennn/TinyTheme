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
| `tinytheme_tests` | 控制台可执行文件 | 单元测试，仅本地与 CI 构建 |

## 3. CMake 配置

```cmake
cmake_minimum_required(VERSION 3.28)
project(TinyTheme VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 供 clang-tidy 读取

add_executable(TinyTheme WIN32
    src/main.cpp
    src/app.cpp
    src/tray.cpp
    src/menu.cpp
    src/theme.cpp
    src/scheduler.cpp
    src/solar.cpp
    src/config.cpp
    src/hotkey.cpp  # v0.2
    src/toast.cpp
    src/log.cpp
    res/app.rc)

target_compile_definitions(TinyTheme PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

target_compile_options(TinyTheme PRIVATE
    /W4 /WX /permissive- /utf-8 /EHsc /GR-
    /O2 /Oi /Gy /Gw /GL /Brepro)

target_link_options(TinyTheme PRIVATE
    /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF
    /INCREMENTAL:NO /MANIFEST:EMBED
    /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)

set_property(TARGET TinyTheme PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(TinyTheme PRIVATE
    user32 shell32 advapi32 gdi32 dwmapi shlwapi)

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

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "x64-release",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/x64-release",
      "architecture": { "value": "x64", "strategy": "external" },
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },
    {
      "name": "x64-debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/x64-debug",
      "architecture": { "value": "x64", "strategy": "external" },
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "arm64-release",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/arm64-release",
      "architecture": { "value": "arm64", "strategy": "external" },
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    }
  ],
  "buildPresets": [
    { "name": "x64-release", "configurePreset": "x64-release" },
    { "name": "x64-debug", "configurePreset": "x64-debug" },
    { "name": "arm64-release", "configurePreset": "arm64-release" }
  ],
  "testPresets": [
    {
      "name": "x64-release",
      "configurePreset": "x64-release",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

## 6. 资源与 manifest

### app.rc

- `ICON`：`IDI_LIGHT`、`IDI_DARK` 两套图标，各含 16 / 20 / 24 / 32 四种尺寸；由 `res/*.svg` 生成后入库，流程见 [图标生成](10-icon-pipeline.md)。
- `VERSIONINFO`：版本号由 CMake 生成，禁止手工维护。
- `STRINGTABLE`：`zh-CN` 与 `en-US` 两份；`IDS_TOAST_HOTKEY_FAIL`（203）随 F-02 于 v0.2 加入。

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

## 10. 常见构建问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `cl.exe` 找不到 | 未在 VS 开发者命令行 | 用 Native Tools 命令提示符 |
| 中文乱码 | 源文件编码或 `/utf-8` 缺失 | 统一 UTF-8 + `/utf-8` |
| `wWinMain` 链接错误 | 未加 `WIN32` 或子系统不对 | 检查 `add_executable(... WIN32 ...)` |
| 资源编译失败 | `.rc` 引用的图标路径错误 | 检查相对路径与文件名 |
| ARM64 链接失败 | 缺少 ARM64 版库 | 确认 VS 安装了 ARM64 组件 |
