# TinyTheme

Windows 11 浅色 / 深色主题一键切换工具。单文件、无运行时依赖、免安装。

> 当前状态：**v0.1.0 本地候选版（RC）**。功能可用，尚未签名、尚未发布到远程仓库。

## 解决什么问题

Windows 11 切换深浅色要翻三层设置，而且没有快捷键：

```
设置 → 个性化 → 颜色 → 选择默认应用模式 → 浅色 / 深色
```

TinyTheme 把这件事变成**点一下托盘图标**。

## 功能（v0.1）

| 能力 | 说明 |
|---|---|
| 托盘左键切换 | 太阳 = 当前浅色，月亮 = 当前深色；点一下立即切换 |
| 右键菜单 | 状态摘要、浅色 / 深色单选、开机启动、退出 |
| 开机自启 | 写入 `HKCU\...\Run`，勾选状态以注册表实际值为准 |
| 配置持久化 | `%APPDATA%\TinyTheme\config.ini`（UTF-8），便携模式写到 exe 同目录 |
| 单实例 | 再次启动只弹出已有实例的菜单，不会出现第二个图标 |
| 深色菜单 | 跟随系统主题；`uxtheme` 不可用时静默回退系统默认菜单 |

## 系统要求

- Windows 10 1809（build 17763）及以上；主要验证目标为 Windows 11 22H2 以上
- x64（ARM64 需自行构建）
- 普通用户权限，不弹 UAC

## 使用

1. 解压 `TinyTheme-v0.1.0-x64.zip` 到任意目录（路径可含空格与中文）。
2. 双击 `TinyTheme.exe`，通知区域出现太阳或月亮图标。
3. **左键**点图标切换主题；**右键**打开菜单。
4. 退出：右键 → 退出。删除 exe 即视为卸载。

首次运行会写入默认配置并注册开机启动（可在菜单里取消）。

### 便携模式

在 exe 同目录放一个空的 `portable.ini`，配置改写到 exe 同目录的 `config.ini`；
便携模式下**不会**自动注册开机启动，需要手动勾选。

### 配置文件

```ini
[general]
mode=dark            ; light / dark，当前主题的镜像，启动以注册表为准
include_system_theme=1
autostart=1
show_toast=1
language=auto

[meta]
first_run=0
```

改完重启生效。配置损坏会被重命名为 `config.ini.bak.<时间戳>` 并重建默认值。

### 日志

`%LOCALAPPDATA%\TinyTheme\logs\app.log`，256 KB 轮转、保留 3 个历史文件。
日志目录不可写时退回 exe 同目录的 `logs\`。

## 已知限制（v0.1）

- **没有自动切换**：v0.1 只做手动浅 / 深两态，定时与日出日落留待 v0.2。
- **没有全局热键**，手动切换只有托盘左键（v0.2）。
- **没有高对比度保护**：高对比度用户不应依赖 v0.1（v0.2 补齐）。
- 外部修改系统主题后，托盘图标不会立刻刷新，重启或下次切换后恢复（v0.2 补齐）。
- 菜单没有"关于 / 打开配置文件"（v0.2）。
- 仅简体中文界面（多语言 v0.3）。
- 部分旧 Win32 / UWP 应用不响应 `WM_SETTINGCHANGE`，切换后颜色不变；本工具不会重启 `explorer.exe`。
- 构建未签名，首次运行可能被 SmartScreen 拦截：选"更多信息 → 仍要运行"。
- 开启自启后直接删除 exe，`HKCU\...\Run\TinyTheme` 会残留；卸载前请先在菜单里取消自启。

## 从源码构建

需要 Visual Studio 2022（"使用 C++ 的桌面开发"）+ CMake 3.28+ + Ninja（或 NMake / VS 生成器）。

```powershell
# 默认 Ninja 预设
pwsh -File tools/build.ps1 -Preset x64-release -Test

# 没有 Ninja 或受限环境
pwsh -File tools/build.ps1 -Preset x64-release -Generator vs2022 -Test
pwsh -File tools/build.ps1 -Preset x64-release -Generator nmake -Test
```

产物：`build/x64-release/TinyTheme.exe`（VS 生成器为 `build/x64-release-vs2022/Release/TinyTheme.exe`）。

也可以直接用 CMake。注意预设把编译器钉死为 MSVC（`cl`），必须在 `vcvars64` 环境下运行：

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

不想管 `vcvars` 时用 Visual Studio 生成器预设（不需要 `vcvars`，但不产出 `compile_commands.json`）：

```powershell
cmake --preset vs2022-x64-release
cmake --build --preset vs2022-x64-release
ctest --preset vs2022-x64-release
```

### 打包

```powershell
pwsh -File tools/package.ps1 -Version 0.1.0
```

生成 `dist/TinyTheme-v0.1.0-x64.zip` 与 `dist/SHA256SUMS.txt`。

## 文档

- [文档索引](docs/README.md)
- 产品：[概览](docs/product/01-overview.md) · [用户与场景](docs/product/02-users-and-scenarios.md) · [功能规格](docs/product/03-feature-spec.md) · [交互设计](docs/product/04-interaction-design.md) · [路线图](docs/product/05-roadmap.md) · [图标设计](docs/product/06-icon-design.md)
- 技术：[架构](docs/engineering/01-architecture.md) · [技术选型](docs/engineering/02-tech-stack.md) · [主题切换](docs/engineering/03-theme-switching.md) · [模块设计](docs/engineering/04-modules.md) · [配置与运行时](docs/engineering/05-config-and-runtime.md) · [构建](docs/engineering/06-build.md) · [测试](docs/engineering/07-testing.md) · [交付](docs/engineering/08-delivery.md) · [规范](docs/engineering/09-conventions.md) · [图标生成](docs/engineering/10-icon-pipeline.md)
- 决策记录：[0001](docs/decisions/0001-cpp-win32-stack.md) · [0002](docs/decisions/0002-zero-dependency.md) · [0003](docs/decisions/0003-tray-first.md)

## 许可证

[MIT](LICENSE)

## 快速了解

| 项 | 内容 |
|---|---|
| 解决的问题 | Win11 切换深浅色要翻三层设置，没有快捷键 |
| 核心交互 | 托盘图标左键单击切换（P0）；全局热键 `Ctrl+Alt+D`（P1，v0.2） |
| 发布物 | 单个 exe（约 251 KB），免安装，普通用户权限 |
| 常驻内存 | 实测约 6.7 MB 工作集 |
| 支持系统 | Windows 10 1809+ / Windows 11 |
