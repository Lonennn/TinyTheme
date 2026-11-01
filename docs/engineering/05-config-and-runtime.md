# 配置与运行时

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 1. 配置

### 位置

| 模式 | 路径 |
|---|---|
| 标准 | `%APPDATA%\TinyTheme\config.ini` |
| 便携 | exe 同目录下存在 `portable.ini` 时，配置写 exe 同目录的 `config.ini` |

便携模式的判定只看 `portable.ini` 是否存在，不读取其内容。

### 格式

标准 INI，**UTF-8 with BOM** 写出；读取兼容 UTF-8 BOM 与无 BOM。v0.1 不再支持 UTF-16，也不使用 `GetPrivateProfileStringW`（该 API 对 UTF-8 不可靠），而是用自研的小型 INI 解析器统一处理编码、校验与注释。

```ini
[general]
mode=dark
include_system_theme=1
autostart=1
; F-02 于 v0.2 引入，v0.1 不读取该键
hotkey=Ctrl+Alt+D
language=auto
show_toast=1

[schedule]
light_at=07:30
dark_at=19:00
use_solar=0
latitude=31.2304
longitude=121.4737
solar_offset_minutes=0

[meta]
first_run=0
```

### 键定义

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `mode` | enum | `light` | v0.1 仅 `light` / `dark`；当前主题的持久化镜像，启动以注册表为准 |
| `include_system_theme` | bool | `1` | 是否同时切换系统主题 |
| `autostart` | bool | `1` | 是否写入开机启动项 |
| `hotkey` | string | `Ctrl+Alt+D` | 全局热键，空字符串表示禁用；v0.2 起读取 |
| `language` | string | `auto` | `auto` / `zh-CN` / `en-US` |
| `show_toast` | bool | `1` | 是否允许气泡提示 |
| `light_at` | `HH:mm` | `07:30` | 固定时间：切浅色 |
| `dark_at` | `HH:mm` | `19:00` | 固定时间：切深色 |
| `use_solar` | bool | `0` | 是否使用日出日落 |
| `latitude` | double | 空 | 纬度，-90 ~ 90 |
| `longitude` | double | 空 | 经度，-180 ~ 180 |
| `solar_offset_minutes` | int | `0` | 日出日落偏移，-120 ~ 120 |
| `first_run` | bool | `1` | 是否首次运行 |

### 读写规则

- 读取：UTF-8 解码（BOM 可选），逐键读取并校验范围；非法值回退默认并记录日志，不整体失败。
- 写入：先写 `config.ini.tmp`，`FlushFileBuffers` 后 `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` 原子替换；文件带 UTF-8 BOM。
- 损坏处理：不可解码、缺失 `[general]` 或结构性损坏时，将原文件重命名为 `config.ini.bak.<时间戳>`，重建默认配置。
- 启动时清理遗留 `config.ini.tmp`，不把它当配置。
- 便携目录不可写：回退到 `%APPDATA%\TinyTheme\config.ini` 并记录 `warn`。
- 不保存派生状态（如"当前是深色"）：`mode` 只是当前主题的镜像，主题以注册表为准。

## 2. 日志

### 位置

`%LOCALAPPDATA%\TinyTheme\logs\app.log`

### 规则

| 项 | 值 |
|---|---|
| 级别 | `info` / `warn` / `error` |
| 格式 | `2026-11-01T09:12:33.482+08:00 [info] Toggle: light -> dark` |
| 轮转 | 单文件 256 KB，保留 3 个历史文件 |
| 刷新 | 每条立即 flush，崩溃时不丢最后几行 |
| 默认级别 | `info`；配置项 `log_level=debug` 可开启详细日志 |

### 记录内容

- 启动、退出、版本号。
- 模式变更、主题写入结果、广播耗时。
- 热键注册结果（v0.2）。
- 调度点计算与触发。
- 所有失败的系统调用及 `GetLastError`。

不记录用户路径以外的隐私信息，不记录窗口标题。

## 3. 单实例

```cpp
HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\TinyTheme.Instance");
if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND existing = FindWindowW(L"TinyTheme.MainWindow", nullptr);
    if (existing) PostMessageW(existing, WM_APP + 1, 0, 0);
    return 0;
}
```

- 使用 `Local\` 前缀，限制在当前会话，避免多用户环境互相干扰。
- 第二个实例不创建托盘图标，不注册热键（v0.2）。
- 若第一个实例无响应，`FindWindowW` 失败时直接退出，不弹错误。

## 4. 开机自启

- 位置：`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- 值名：`TinyTheme`
- 值数据：`"C:\path\to\TinyTheme.exe"`（路径含空格时必须带引号）
- 菜单勾选状态每次打开菜单时从注册表实时读取，不信任配置。
- 便携模式下首次运行不自动开启，仅在用户手动勾选时写入。
- 取消勾选时删除该值，不写空字符串。

## 5. 调度器（v0.2）

> v0.1 不包含本节实现：没有自动模式、没有定时器、没有目标时刻计算。

### 定时器策略

- 每次只设置一个一次性定时器：`SetTimer(window_, 1, delay_ms, nullptr)`。
- `delay_ms` 由 `scheduler::next_point` 计算，取值范围 `[1000, 86_400_000]`。
- 触发后重新计算并重排，不依赖固定周期轮询。
- 系统时间被修改时，通过 `WM_TIMECHANGE` 重新计算。

### 休眠与唤醒

- 收到 `WM_POWERBROADCAST`（`PBT_APMRESUMEAUTOMATIC` / `PBT_APMRESUMESUSPEND`）后：
  1. `scheduler::target_at(cfg, now)` 计算当前应有主题。
  2. 与现状不一致则立即应用。
  3. 重排下一次定时器。
- 补偿只执行一次，即使错过了多个切换点。

### 时钟变更

- `WM_TIMECHANGE`：重排定时器，不立即切换。

## 6. 崩溃与异常

- 不安装 `MiniDump`，不引入 `dbghelp`。
- `wWinMain` 外层包裹 `try/catch(...)`，捕获后写日志并返回 1。
- 关键资源使用 RAII（`wil` 风格的轻量封装或自写 `UniqueHandle`），避免泄漏。
- 连续崩溃保护：启动时写入 `last_start` 时间戳，若 1 分钟内启动超过 3 次，本次启动不注册开机自启、不显示气泡，仅记录日志。

## 7. 资源占用基线

| 项 | 目标 |
|---|---|
| 句柄数 | < 50 |
| GDI 对象 | < 20 |
| 线程数 | 1 |
| 工作集 | < 10 MB |
| 私有字节 | < 5 MB |

发布前用 `Get-Process` 与任务管理器核对，记录到 `06-build.md`。
