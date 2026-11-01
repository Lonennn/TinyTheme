# 技术选型

> 状态：草案 v0.1 ｜ 更新：2026-11-01

本文只记录最终采用的技术，不讨论备选方案。

## 1. 总览

| 层次 | 选型 | 版本 |
|---|---|---|
| 语言 | C++ | C++23 |
| 编译器 | MSVC（Visual Studio Build Tools） | v143，19.40+ |
| SDK | Windows SDK | 10.0.26100 |
| UI | 原生 Win32 API | — |
| 构建系统 | CMake + Ninja | CMake 3.28+，Ninja 1.11+ |
| 单元测试 | doctest | 2.5.3（本地 vendor，单头文件） |
| 格式化 | clang-format | 18+ |
| 静态分析 | clang-tidy | 18+ |
| CI | GitHub Actions | windows-2025 |
| 签名 | Azure Trusted Signing / SignPath | — |
| 分发 | GitHub Releases + winget + Scoop | — |
| 运行时依赖 | 仅 Windows 系统 DLL | — |

## 2. 语言：C++23

- 标准由 `CMAKE_CXX_STANDARD 23` 指定；MSVC 尚无 `/std:c++23` 开关，实际落为 `/std:c++latest`，并开启 `/permissive-`。
- 采用 C++23 的 `std::expected`、`std::to_underlying`、`std::string::contains`，以及 C++20 的 `std::optional`、`std::string_view`、`std::span`、`concepts`（少量）、指定初始化。
- 不使用 C++ 模块（MSVC 支持仍在演进，构建复杂度不划算）。
- 不引入异常作为控制流；错误通过返回值、`std::expected` 与 `std::optional` 表达，仅在 STL 内部允许异常。

## 3. 标准库使用策略

标准库可用，不做极端裁剪。建议范围：

| 组件 | 使用建议 |
|---|---|
| `<string>` / `<string_view>` | 自由使用 |
| `<vector>` / `<array>` / `<span>` | 自由使用 |
| `<optional>` / `<variant>` | 自由使用 |
| `<expected>` | 自由使用，模块边界返回"失败带原因"结果的首选 |
| `<chrono>` | 自由使用，用于调度计算 |
| `<filesystem>` | 可用，用于配置与日志路径 |
| `<algorithm>` / `<ranges>` | 自由使用 |
| `<format>` | 可用，但不用于高频路径 |
| `<iostream>` / `<fstream>` | 不推荐，改用 Win32 文件 API 或 `std::filesystem` + 原生句柄 |
| `<regex>` | 不推荐，配置解析用简单字符串处理即可 |

说明：以上"不推荐"是体积与启动时间的偏好，不是禁令；如果某处用标准库明显更清晰，优先可读性。

## 4. UI：原生 Win32

- 托盘：`Shell_NotifyIconW`，使用 `NOTIFYICON_VERSION_4`。
- 菜单：`CreatePopupMenu` + `TrackPopupMenuEx`。
- 热键（v0.2）：`RegisterHotKey`。
- 消息：隐藏顶层窗口 + `GetMessage` 循环。
- DPI：manifest 声明 `PerMonitorV2`。
- 深色菜单：通过 `uxtheme.dll` 的 `SetPreferredAppMode` / `FlushMenuThemes` 启用，失败时静默回退到系统默认菜单。
- 不使用任何 UI 框架，所有界面元素由系统提供。

## 5. 构建系统：CMake + Ninja + MSVC

- `CMakeLists.txt` 单文件，不拆子目录（项目规模不需要）。
- `CMakePresets.json` 提供 `x64-debug`、`x64-release`、`arm64-release` 三个预设。
- 生成器用 Ninja，构建目录 `build/<preset>`。
- 依赖仅系统库，不使用包管理器。
- 资源通过 `.rc` 编译进 exe，manifest 通过 `/MANIFEST:EMBED` 嵌入。

## 6. 测试：doctest

- 单头文件，编译进独立的 `tests` 目标，不进入发布物；头文件 vendor 在 `tests/third_party/doctest/`。
- 覆盖纯逻辑：v0.1 为配置解析、模式映射；日出日落、调度、热键解析自 v0.2 纳入。
- 通过 `ctest` 驱动，CI 中作为合并门禁。

## 7. 工具链

| 工具 | 用途 | 配置 |
|---|---|---|
| clang-format | 统一格式 | `.clang-format`，基于 Microsoft 风格，4 空格缩进 |
| clang-tidy | 静态检查 | `.clang-tidy`，`bugprone-*` / `clang-analyzer-*` / `cppcoreguidelines-*` / `misc-*` / `modernize-*` / `performance-*` / `readability-*` 子集，含命名规则 |
| CMake Presets | 固化构建配置 | `CMakePresets.json` |
| GitHub Actions | 构建、测试、打包 | `.github/workflows/ci.yml` |
| dumpbin / llvm-size | 体积核对 | 发布前手工执行 |

## 8. 目标平台

| 项 | 值 |
|---|---|
| 最低系统 | Windows 10 1809（build 17763） |
| 主要目标 | Windows 11 22H2 及以上 |
| 架构 | x64、ARM64 |
| 权限 | 普通用户 |
| 运行时 | 无额外依赖，静态链接 C 运行库 |

## 9. 关键技术约束

- 只写 `HKCU`，不写 `HKLM`。
- 不加载驱动、不注入进程、不修改系统文件。
- 不发起网络请求。
- 单进程单线程，无 COM 依赖（v0.3 引入壁纸接口时再评估）。
- 发布物为单个 exe，配置与日志写在用户目录。

## 10. 版本与依赖管理

- 版本号单一来源：`CMakeLists.txt` 的 `project(VERSION ...)`，通过 `configure_file` 生成 `version.h` 与 `.rc` 的版本资源。
- 不引入第三方库，因此没有依赖升级负担；GitHub Actions 的 action 版本由 Dependabot 维护。
- 编译器与 SDK 版本在 `CMakePresets.json` 与 CI 中固定，避免"本地能编、CI 不能编"。
