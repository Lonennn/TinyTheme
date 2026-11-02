# 技术选型

> 状态：v0.1 已实现 ｜ 更新：2026-11-01

本文只记录最终采用的技术与取舍理由，不讨论备选方案。版本号以 `CMakeLists.txt`、`CMakePresets.json`、`.github/workflows/ci.yml` 为准。

## 1. 总览

| 层次 | 选型 | 版本 |
|---|---|---|
| 语言 | C++ | C++23 |
| 编译器 | MSVC（Visual Studio Build Tools） | v143，19.40+（CI 跟随镜像最新） |
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

标准库可用，不做极端裁剪。只有三条偏好：

- 文件读写优先 Win32 API 或 `std::filesystem` + 原生句柄，不用 `<iostream>` / `<fstream>`（体积与启动开销）。
- 配置解析用简单字符串处理，不引 `<regex>`。
- `<format>` 可用，但不放在高频路径上。

## 4. UI：原生 Win32

- 托盘：`Shell_NotifyIconW`，`NOTIFYICON_VERSION_4`。
- 菜单：`CreatePopupMenu` + `TrackPopupMenuEx`。
- 热键（v0.2）：`RegisterHotKey`。
- 消息：隐藏顶层窗口 + `GetMessage` 循环。
- DPI：manifest 声明 `PerMonitorV2`。
- 深色菜单：`uxtheme` 的 `SetPreferredAppMode` / `FlushMenuThemes`，失败时静默回退。
- 不使用任何 UI 框架，所有界面元素由系统提供。

## 5. 构建系统：CMake + Ninja + MSVC

- `CMakeLists.txt` 单文件，不拆子目录（项目规模不需要）。
- 生成器默认 Ninja，构建目录 `build/<preset>`。
- 依赖仅系统库，不使用包管理器。

预设清单、为什么选 Ninja、生成器与编译器的关系，见 [构建 §4](06-build.md#4-预设与生成器)。

## 6. 测试：doctest

- 单头文件，只编译进 `tests` 目标，不进入发布物；vendor 在 `tests/third_party/doctest/`。
- 通过 `ctest` 驱动，CI 中作为合并门禁。
- 覆盖范围见 [测试 §2](07-testing.md#2-覆盖范围)。

## 7. 工具链

| 工具 | 用途 | 配置 |
|---|---|---|
| clang-format | 统一格式 | `.clang-format` |
| clang-tidy | 静态检查 | `.clang-tidy`（命名规则、Win32 例外的说明都在里面） |
| CMake Presets | 固化构建配置 | `CMakePresets.json` |
| GitHub Actions | 构建、测试、打包 | `.github/workflows/ci.yml` |
| dumpbin | 依赖与导入核对 | 发布前手工执行 |

## 8. 目标平台

| 项 | 值 |
|---|---|
| 最低系统 | Windows 10 1809（build 17763） |
| 主要目标 | Windows 11 22H2 及以上 |
| 架构 | x64、ARM64 |
| 权限 | 普通用户 |
| 运行时 | 无额外依赖，静态链接 C 运行库 |

## 9. 关键技术约束

禁止事项（不写 HKLM、不注入、不重启 Explorer、不联网等）统一列在 [编码规范 §10](09-conventions.md#10-禁止事项)，这里不重复。

本项目特有的两条：

- 单进程单线程，无 COM 依赖（v0.3 引入壁纸接口时再评估）。
- 未文档化 API（`uxtheme` 的 `SetPreferredAppMode`）只能作为增强，必须带 fallback。

## 10. 版本管理

- 版本号单一来源：`CMakeLists.txt` 的 `project(VERSION ...)`，其余地方一律不手写版本。
- 不引入第三方库，因此没有依赖升级负担；GitHub Actions 的 action 版本由 Dependabot 维护。
- 编译器与 SDK 版本在预设与 CI 中固定，避免"本地能编、CI 不能编"。
