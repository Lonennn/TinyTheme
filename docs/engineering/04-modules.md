# 模块设计

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 1. 模块清单

| 模块 | 职责 | 依赖 | 预估行数 |
|---|---|---|---|
| `main` | 入口、单实例、消息循环、窗口过程 | 全部 | 150 |
| `app` | 状态机、模式切换、事件编排 | tray/menu/theme/scheduler/config | 300 |
| `tray` | 托盘图标创建、更新、销毁、事件回调 | 无 | 150 |
| `menu` | 构建菜单、勾选状态、命令分发 | config/theme | 150 |
| `theme` | 注册表读写、广播、状态读取 | 无 | 120 |
| `scheduler` | 计算下一次切换点、定时器管理 | solar/config | 150 |
| `solar` | 日出日落计算 | 无 | 120 |
| `config` | INI 读写、默认值、路径解析 | 无 | 180 |
| `hotkey`（v0.2） | 注册 / 注销全局热键、字符串解析 | 无 | 100 |
| `toast` | 托盘气泡提示 | tray | 60 |
| `log` | 日志写入、轮转 | 无 | 100 |
| `util` | RAII 句柄、字符串转换、错误码 | 无 | 80 |

合计约 1660 行，不含测试与资源；v0.1 不含 `hotkey`（100 行），约 1560 行。

## 2. 核心数据结构

```cpp
// theme.h
struct ThemeSnapshot {
    bool apps_light = true;
    bool system_light = true;
    bool high_contrast = false;
};

enum class ThemeMode { Light, Dark, Auto };

struct AppState {
    ThemeMode mode = ThemeMode::Auto;
    bool override_active = false;   // 自动模式下的临时覆盖
    bool override_light = true;
    ThemeSnapshot theme{};
    bool suspended = false;         // 高对比度导致暂停
};
```

```cpp
// config.h
struct TimeOfDay {
    int hour = 0;
    int minute = 0;
};

struct Config {
    ThemeMode mode = ThemeMode::Auto;
    bool include_system_theme = true;
    bool autostart = true;
    std::wstring hotkey = L"Ctrl+Alt+D";  // v0.2 起生效
    bool use_solar = false;
    TimeOfDay light_at{7, 30};
    TimeOfDay dark_at{19, 0};
    double latitude = 0.0;
    double longitude = 0.0;
    int solar_offset_minutes = 0;
    bool show_toast = true;
    std::wstring language = L"auto";
    bool first_run = true;
};
```

## 3. 模块接口

### theme

```cpp
namespace theme {
ThemeSnapshot read();
bool apply(bool light, bool include_system);
bool is_high_contrast();
void broadcast_change();
}
```

- `read`：读取两个键与高对比度状态，任何键缺失按浅色处理。
- `apply`：写入并广播，成功返回 true；不修改内存状态，由 `App` 负责。

### config

```cpp
namespace config {
std::filesystem::path file_path();
Config load();
bool save(const Config&);
bool is_portable();
}
```

- `load`：文件不存在时返回默认值；解析失败时备份并返回默认值。
- `save`：先写临时文件再原子替换，避免写入中断导致配置损坏。

### scheduler

```cpp
struct SchedulePoint {
    std::chrono::system_clock::time_point when;
    bool light;
};

namespace scheduler {
std::optional<SchedulePoint> next_point(const Config& cfg,
                                        std::chrono::system_clock::time_point now);
std::optional<bool> target_at(const Config& cfg,
                              std::chrono::system_clock::time_point now);
}
```

- `next_point`：返回下一次切换的时刻与目标；自动模式未配置时间时返回 `nullopt`。
- `target_at`：判断"此刻应该是什么主题"，用于启动时校准与休眠补偿。

### solar

```cpp
struct SolarTimes {
    std::optional<std::chrono::sys_seconds> sunrise;
    std::optional<std::chrono::sys_seconds> sunset;
};

SolarTimes compute_solar_times(double latitude, double longitude,
                               std::chrono::year_month_day date,
                               std::chrono::minutes utc_offset);
```

极昼 / 极夜时对应字段为 `nullopt`，由调度器回退到固定时间。

### hotkey（v0.2）

```cpp
struct HotkeySpec {
    UINT modifiers = 0;   // MOD_CONTROL | MOD_ALT | ...
    UINT vk = 0;          // 'D', VK_F5, ...
};

std::optional<HotkeySpec> parse_hotkey(std::wstring_view text);
std::wstring format_hotkey(const HotkeySpec& spec);
```

支持 `Ctrl` / `Alt` / `Shift` / `Win` 组合，主键支持 `A-Z`、`0-9`、`F1-F24`。

### tray

```cpp
namespace tray {
bool create(HWND owner, HINSTANCE instance);
void set_theme(bool light);      // 切换图标资源
void set_tooltip(std::wstring_view text);
void show_balloon(std::wstring_view title, std::wstring_view text);
void destroy();
}
```

### toast

```cpp
namespace toast {
void once(std::wstring_view key, std::wstring_view title, std::wstring_view text);
}
```

按 key 去重，24 小时内相同 key 只提示一次。

## 4. App 状态机实现

`App` 持有 `AppState` 与 `Config`，所有外部事件先进入 `App` 的私有方法，再由它调用功能模块。

```cpp
class App {
public:
    explicit App(HINSTANCE instance);
    int run(int show_command);

    void on_tray_event(UINT event);
    void on_hotkey();  // v0.2
    void on_command(UINT command_id);
    void on_setting_change();
    void on_power_broadcast(WPARAM event);
    void on_timer(UINT_PTR timer_id);
    void on_second_instance();

private:
    void toggle();
    void set_mode(ThemeMode mode);
    void apply_theme(bool light);
    void apply_scheduled(bool light);
    void refresh_ui();
    void arm_schedule();
    void enter_override(bool light);
    void clear_override();

    HINSTANCE instance_;
    HWND window_ = nullptr;
    Config config_;
    AppState state_;
    HotkeySpec hotkey_{};  // v0.2
};
```

关键规则：

- `toggle()` 永远先 `theme::read()` 再取反，不依赖 `state_`。
- `apply_theme()` 成功后更新 `state_.theme` 并调用 `refresh_ui()`。
- `set_mode()` 切换模式时清除 Override，并重排定时器。
- `arm_schedule()` 先 `KillTimer` 再 `SetTimer`，保证只有一个定时器。

## 5. 事件流示例

### 热键按下（v0.2）

```
WM_HOTKEY
  → App::on_hotkey()
  → App::toggle()
  → theme::read()                    // 拿到当前真实值
  → theme::apply(!apps_light, cfg.include_system_theme)
  → App 更新 state_.theme
  → 若 mode == Auto：enter_override(!apps_light)
  → App::refresh_ui()                // 图标 + 菜单
```

### 菜单选择"自动"

```
WM_COMMAND(ID_MODE_AUTO)
  → App::set_mode(ThemeMode::Auto)
  → clear_override()
  → scheduler::target_at(cfg, now)
  → 若与当前主题不一致：theme::apply()
  → App::arm_schedule()
  → App::refresh_ui()
```

### 定时器到期

```
WM_TIMER(id=1)
  → App::on_timer()
  → 读取当前时刻，scheduler::target_at() 校验
  → 若需要切换：App::apply_scheduled(target_light)
  → App::arm_schedule()
```

## 6. 命令 ID

| 常量 | 值 | 含义 |
|---|---|---|
| `ID_MODE_LIGHT` | 1001 | 浅色 |
| `ID_MODE_DARK` | 1002 | 深色 |
| `ID_MODE_AUTO` | 1003 | 自动 |
| `ID_AUTOSTART` | 1004 | 开机启动 |
| `ID_OPEN_CONFIG` | 1005 | 打开配置文件 |
| `ID_ABOUT` | 1006 | 关于 |
| `ID_EXIT` | 1007 | 退出 |

命令 ID 集中定义在 `menu.h`，避免散落。

## 7. 资源 ID

| 常量 | 值 | 含义 |
|---|---|---|
| `IDI_LIGHT` | 101 | 浅色图标 |
| `IDI_DARK` | 102 | 深色图标 |
| `IDS_APP_NAME` | 201 | 应用名 |
| `IDS_TOAST_FIRST_RUN` | 202 | 首次运行提示 |
| `IDS_TOAST_HOTKEY_FAIL`（v0.2） | 203 | 热键冲突提示 |
| `IDS_TOAST_APPLY_FAIL` | 204 | 切换失败提示 |
| `IDS_TOAST_HIGH_CONTRAST` | 205 | 高对比度提示 |
