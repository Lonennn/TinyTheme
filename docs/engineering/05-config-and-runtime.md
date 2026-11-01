# 配置与运行时

> 状态：v0.1 已实现 ｜ 更新：2026-11-01

键名、默认值与序列化格式以 `src/config.cpp` 为准；用户视角的配置说明见 [使用指南 §3](../usage.md#3-配置文件)。本文只记录设计取舍。

## 1. 配置文件

### 位置

| 模式 | 路径 |
|---|---|
| 标准 | `%APPDATA%\TinyTheme\config.ini` |
| 便携 | exe 同目录存在 `portable.ini` 时，配置写 exe 同目录 |

便携判定只看 `portable.ini` **是否存在**，不读其内容。

### 编码

写出 **UTF-8 with BOM**，读取兼容 BOM 与无 BOM。

不使用 `GetPrivateProfileStringW`：该 API 对 UTF-8 支持不可靠，也做不了原子替换与逐键校验。因此自研一个小型 INI 解析器（约百余行），换来三件事：编码可预期、错误可定位到键、解析函数可单测。

### 读写规则

- 逐键读取并校验范围；非法值回退默认并记 `warn`，**不整体失败**。
- 保存走 `config.ini.tmp` → `FlushFileBuffers` → `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`；启动时清理遗留的 `.tmp`。
- 不可解码或结构性损坏：原文件改名为 `config.ini.bak.<时间戳>`，重建默认值。
- 便携目录不可写：回退到 `%APPDATA%` 并记 `warn`。
- 不保存派生状态。"当前是深色"由注册表回答，`mode` 只是镜像。

### v0.2 预留的键

`hotkey`、`[schedule]` 整节在 v0.1 中**不读取**，只是为了让配置格式在 v0.2 不用改结构。解析器遇到未知键忽略并记录。

## 2. 日志

### 位置与规则

`%LOCALAPPDATA%\TinyTheme\logs\app.log`，单文件 256 KB 轮转、保留 3 份、每条立即 flush（崩溃时不丢最后几行）。

格式：`2026-11-01T09:12:33.482+08:00 [info] Toggle: light -> dark`。

**路径回退**：`%LOCALAPPDATA%` 不可写时退到 exe 同目录的 `logs\`；仍失败则本次运行禁用日志。日志失败永远不影响主流程——这条是硬约束，别让日志把程序带崩。

### 记录内容

启动与退出、配置加载结果、主题写入结果与失败原因、托盘创建失败、单实例唤起。**不记录**窗口标题、用户名等隐私信息。

## 3. 单实例

- 用 `Local\` 前缀的命名互斥体，限制在当前会话，避免多用户环境互相干扰。
- 第二个实例 `FindWindow` 找到已有窗口后投递 `WM_APP + 1` 并退出：不创建托盘、不弹对话框。
- 找不到窗口（第一个实例还没建好窗口或已无响应）就静默退出，不报错。
- 判定前必须显式 `SetLastError(ERROR_SUCCESS)`：`CreateMutexW` 成功时不会清零错误码，直接读会误判为"已存在"。

## 4. 开机自启

- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，值名 `TinyTheme`，数据是**带引号**的 exe 完整路径。
- 菜单勾选状态每次打开菜单时从注册表实时读取，不信任配置里的 `autostart`。
- 便携模式不自动开启，只在用户手动勾选时写入。
- 取消勾选删除该值，不写空字符串。

## 5. 崩溃与异常

- 不安装 MiniDump，不引入 `dbghelp`。
- `wWinMain` 外层 `try/catch(...)`，捕获后写日志并返回 1。
- 关键资源全部用 `util` 里的 RAII 包装，业务代码不出现裸 `CloseHandle` / `RegCloseKey`。

## 6. 资源占用

目标值与实测值统一记录在 [构建 §8](06-build.md#8-发布前核对)，避免两处各写一份数字。
