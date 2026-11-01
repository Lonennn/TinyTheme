# 变更记录

本文件记录每个版本的变更。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循语义化版本。

## [0.1.0] - 2026-11-01

### 新增

- 托盘图标左键单击切换浅色 / 深色主题，太阳表示浅色、月亮表示深色。
- 右键菜单：状态摘要（应用 / 系统，含混合状态）、浅色、深色、开机启动、退出。
- 深色菜单：通过 `uxtheme` 的 `SetPreferredAppMode` 跟随系统主题；不可用时静默回退系统默认菜单。
- 开机启动：写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，勾选状态以注册表实际值为准。
- 配置持久化：`%APPDATA%\TinyTheme\config.ini`（UTF-8），保存采用临时文件 + 原子替换；损坏文件自动备份重建。
- 便携模式：exe 同目录存在 `portable.ini` 时，配置写到 exe 同目录。
- 单实例：命名互斥体保护，第二次启动弹出已有实例菜单。
- 日志：`%LOCALAPPDATA%\TinyTheme\logs\app.log`，256 KB 轮转、保留 3 个历史文件；目录不可写时回退到 exe 同目录。
- 首次运行引导气泡；切换失败提示一次（每次运行最多一次）。
- 托盘图标按 `SM_CXSMICON` 在 16 / 20 / 24 / 32 四档中选最接近的一档。

### 已知限制

- 无自动调度（v0.2）、无全局热键（v0.2）、无高对比度保护（v0.2）。
- 外部修改系统主题后托盘图标不立即刷新（v0.2 的 F-11 补齐）。
- 菜单不含"关于 / 打开配置文件"（v0.2）。
- 仅简体中文界面（v0.3 的多语言）。
- 构建未签名，公开分发前必须签名。

[0.1.0]: https://github.com/OWNER/TinyTheme/releases/tag/v0.1.0
