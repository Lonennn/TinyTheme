# 架构设计

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 1. 设计原则

1. **单进程、单线程**：所有逻辑跑在 UI 线程的消息循环里，不引入线程与同步原语。
2. **事件驱动**：除一次性调度定时器外，不做任何轮询。
3. **状态单一来源**：注册表是主题状态的唯一事实来源，内存状态只是缓存。
4. **失败降级**：任何一步失败都不能导致进程退出，最坏情况是"这次没切成功"。
5. **无侵入**：不注入、不 hook、不重启 Explorer、不写 HKLM。

## 2. 进程与线程模型

```
TinyTheme.exe
└── 主线程
    ├── 隐藏窗口 + 消息循环
    ├── 托盘图标（Shell_NotifyIcon）
    ├── 全局热键（RegisterHotKey，v0.2）
    └── 一次性调度定时器（SetTimer）
```

没有工作线程，没有线程池，没有 COM 单元（v0.3 引入壁纸接口时再评估）。

## 3. 窗口与消息模型

程序启动后创建一个不可见的顶层窗口 `TinyTheme.MainWindow`，作为所有系统事件的接收端。

| 消息 | 来源 | 处理 |
|---|---|---|
| `WM_APP + 1` | 第二个实例 | 弹出菜单，不切换主题 |
| `WM_APP + 100` | 托盘回调 | 分发 `NIN_SELECT` / `WM_CONTEXTMENU` / `NIN_KEYSELECT` |
| `WM_HOTKEY`（v0.2） | `RegisterHotKey` | 执行切换 |
| `WM_TIMER`（id=1） | 调度器 | 执行计划切换并重排下一次 |
| `WM_SETTINGCHANGE` | 系统或自身广播 | 主题 / 高对比度变化，刷新图标与菜单 |
| `WM_POWERBROADCAST` | 系统 | 唤醒后重算计划切换点，必要时补偿 |
| `WM_ENDSESSION` | 系统 | 保存配置 |
| `WM_DISPLAYCHANGE` | 系统 | DPI 变化时重载托盘图标 |
| `WM_CLOSE` | 系统 | 忽略，只允许通过菜单退出 |

## 4. 模块关系

```
                    ┌──────────────┐
                    │   App        │  状态机与编排
                    └──┬───┬───┬───┘
          ┌────────────┘   │   └────────────┐
          ▼                ▼                ▼
    ┌──────────┐    ┌───────────┐    ┌──────────┐
    │  Tray    │    │ ThemeEngine│    │ Scheduler│
    └────┬─────┘    └─────┬─────┘    └────┬─────┘
         │                │               │
         ▼                ▼               ▼
    ┌──────────┐    ┌───────────┐    ┌──────────┐
    │  Menu    │    │ Registry  │    │  Solar   │
    └──────────┘    │ + Broadcast│   └──────────┘
                    └───────────┘
    ┌──────────┐    ┌───────────┐    ┌──────────┐
    │  Config  │    │   Hotkey  │    │   Log    │
    └──────────┘    └───────────┘    └──────────┘
```

依赖方向：`App` 依赖各功能模块，功能模块之间不互相依赖，只通过 `App` 协调。

`Hotkey` 模块自 v0.2 引入；v0.1 不注册全局热键，也不处理 `WM_HOTKEY`。

## 5. 核心数据流

### 手动切换

```
用户点击（v0.2 起也包含热键）
   → App::toggle()
   → ThemeEngine::read()          读取注册表当前值
   → ThemeEngine::apply(!current) 写注册表
   → broadcast_immersive_color_set() 广播
   → Tray::refresh()              切换图标
   → App 更新内存状态
```

### 自动切换

```
Scheduler 定时器到期
   → App::on_schedule_boundary()
   → 计算目标主题（固定时间 / 日出日落）
   → ThemeEngine::apply(target)
   → Scheduler::arm_next()         重排下一次
```

### 外部变更（用户在系统设置里改了主题）

```
WM_SETTINGCHANGE("ImmersiveColorSet")
   → App::on_setting_change()
   → ThemeEngine::read()
   → 更新内存状态并刷新托盘图标
   → 若是自动模式，重新校准定时器
```

## 6. 启动与退出

### 启动

1. 检查互斥体；已存在则发送 `WM_APP + 1` 给已有实例并退出。
2. 加载配置（失败则使用默认值）。
3. 创建隐藏窗口，注册消息处理。
4. 创建托盘图标（先显示默认图标）。
5. 注册全局热键（v0.2；失败则记录并提示，降级为仅托盘切换）。
6. 读取当前主题，刷新图标与菜单状态。
7. 若模式为自动，计算当前应有主题；与现状不一致则应用，然后排定下一次切换。
8. 首次运行时弹出引导气泡。

### 退出

1. 保存配置。
2. 注销热键（v0.2）。
3. 删除托盘图标。
4. 销毁窗口，释放互斥体。

## 7. 错误处理策略

| 层级 | 策略 |
|---|---|
| 系统调用失败 | 记录 `GetLastError`，返回 `bool` / `std::optional`，不抛异常 |
| 可恢复错误 | 提示一次，继续运行（如热键冲突，v0.2） |
| 不可恢复错误 | 记录日志，退出进程，下次启动自动恢复（如窗口创建失败） |
| 未捕获异常 | `wWinMain` 最外层兜底，写日志后退出，不弹错误框 |

## 8. 可扩展点

| 扩展点 | 用途 | 版本 |
|---|---|---|
| `IThemeAction` 接口 | 编排动作（壁纸、强调色、外部命令） | v0.3 |
| `Scheduler` 策略 | 固定时间 / 日出日落 / 未来的地理围栏 | v0.2 |
| 字符串资源表 | 多语言 | v0.3 |
| `ThemeEngine` 目标参数 | 仅应用主题 / 应用 + 系统 | v0.3 |

## 9. 目录结构

```
TinyTheme/
├── CMakeLists.txt
├── CMakePresets.json
├── .clang-format
├── .clang-tidy
├── src/
│   ├── main.cpp          入口、消息循环、单实例
│   ├── app.h/.cpp        状态机与编排
│   ├── tray.h/.cpp       托盘图标
│   ├── menu.h/.cpp       右键菜单
│   ├── theme.h/.cpp      注册表读写与广播
│   ├── scheduler.h/.cpp  计划切换
│   ├── solar.h/.cpp      日出日落计算
│   ├── config.h/.cpp     INI 读写
│   ├── hotkey.h/.cpp     全局热键（v0.2）
│   ├── toast.h/.cpp      气泡提示
│   ├── log.h/.cpp        日志
│   └── util.h            RAII 与字符串工具
├── res/
│   ├── app.rc
│   ├── app.manifest
│   ├── icon-light.svg    图标源文件
│   ├── icon-dark.svg
│   ├── icon-light.ico    由 SVG 生成，入库
│   └── icon-dark.ico
├── tools/
│   ├── render-icons.ps1  SVG → 四档 PNG
│   └── pack-icon.ps1     PNG → 多尺寸 ICO
├── tests/
│   ├── CMakeLists.txt
│   ├── test_solar.cpp
│   ├── test_config.cpp
│   └── test_scheduler.cpp
└── docs/
```
