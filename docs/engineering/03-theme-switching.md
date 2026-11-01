# 主题切换机制

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 1. 原理

Windows 的浅色 / 深色由当前用户注册表中的两个 DWORD 决定，修改后向所有顶层窗口广播 `WM_SETTINGCHANGE`，携带 `ImmersiveColorSet`，系统组件与应用程序据此刷新配色。

没有公开的 `SetSystemTheme` API，注册表 + 广播是系统设置页本身采用的方式，也是各类主题工具通用做法。

## 2. 注册表位置

```
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize
```

| 值名 | 类型 | 含义 | 浅色 | 深色 |
|---|---|---|---|---|
| `AppsUseLightTheme` | REG_DWORD | 应用主题 | 1 | 0 |
| `SystemUsesLightTheme` | REG_DWORD | 系统主题（任务栏、开始、通知中心） | 1 | 0 |

只读写这两个值，不修改同键下的其他值（如 `EnableTransparency`），也不修改 `CurrentTheme` 或 `.theme` 文件。

## 3. 读取

```cpp
constexpr wchar_t kPersonalize[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

std::optional<DWORD> read_dword(const wchar_t* name) {
    DWORD value = 0, size = sizeof(value), type = 0;
    LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, kPersonalize, name,
                              RRF_RT_REG_DWORD, &type, &value, &size);
    if (rc != ERROR_SUCCESS) return std::nullopt;
    return value;
}
```

约定：

- 值不存在或读取失败时视为浅色（系统默认）。
- 每次切换前都重新读取，不使用缓存值决定"取反"目标，避免用户在系统设置里改过之后状态错位。

## 4. 写入

```cpp
bool apply_theme(bool light, bool include_system) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kPersonalize, 0,
                      KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    const DWORD value = light ? 1u : 0u;
    LSTATUS rc = RegSetKeyValueW(key, nullptr, L"AppsUseLightTheme",
                                 REG_DWORD, &value, sizeof(value));
    if (rc == ERROR_SUCCESS && include_system) {
        rc = RegSetKeyValueW(key, nullptr, L"SystemUsesLightTheme",
                             REG_DWORD, &value, sizeof(value));
    }
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) return false;

    broadcast_immersive_color_set();
    return true;
}
```

要点：

- `RegSetKeyValueW` 在值不存在时会创建，避免额外的存在性判断。
- 先写应用主题，再写系统主题；任一失败即返回失败，不广播。
- 写入后立刻广播，不延迟、不合并。

## 5. 广播

```cpp
void broadcast_immersive_color_set() {
    DWORD_PTR result = 0;
    SendMessageTimeoutW(
        HWND_BROADCAST, WM_SETTINGCHANGE, 0,
        reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
        SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, 5000, &result);
}
```

要点：

- 用 `SendMessageTimeoutW` 而非 `SendNotifyMessage`：需要等待窗口处理完毕，且能对无响应的窗口设置超时。
- `SMTO_ABORTIFHUNG` 防止某个挂起的程序拖住整个切换流程。
- 广播本身通常几毫秒完成；设置了 5 秒上限属于极端兜底。

## 6. 时序

```
用户点击
  │
  ├─ 读取 AppsUseLightTheme ──► 当前 = 浅色
  ├─ 写入 AppsUseLightTheme = 0
  ├─ 写入 SystemUsesLightTheme = 0
  ├─ 广播 WM_SETTINGCHANGE("ImmersiveColorSet")
  │     ├─ Explorer：任务栏、开始菜单、通知中心刷新
  │     ├─ Chromium 系：窗口配色刷新
  │     └─ 其他应用：各自处理
  ├─ 刷新托盘图标
  └─ 更新菜单状态
```

## 7. 状态判定

| 应用主题 | 系统主题 | 展示 |
|---|---|---|
| 深色 | 深色 | 深色 |
| 浅色 | 浅色 | 浅色 |
| 深色 | 浅色 | 深色（应用）/ 浅色（系统） |
| 浅色 | 深色 | 浅色（应用）/ 深色（系统） |

混合状态下，切换以**应用主题**为基准取反，并同时写入两者（除非配置为仅应用主题）。

## 8. 边界与已知问题

| 现象 | 原因 | 处理 |
|---|---|---|
| 部分旧 Win32 程序切换后颜色不变 | 程序只在启动时读取主题 | 文档说明；不重启进程 |
| 任务栏偶发残留旧颜色 | Explorer 刷新时机 | 广播后 80 ms 再广播一次（仅在首次失败时启用） |
| UWP / WinUI 应用未刷新 | 应用自身缓存 | 文档说明 |
| 高对比度模式下切换无效 | 系统优先使用对比度主题 | 检测到即禁用并提示 |
| 远程桌面会话 | 主题按会话隔离 | 正常支持，无需特殊处理 |
| 用户手动在设置中改主题 | 状态被外部修改 | 监听广播，刷新内部状态 |

明确不做：

- 不重启 `explorer.exe`。
- 不调用未文档化的 `SetSystemTheme` 类接口。
- 不修改 `.theme` 文件或 `CurrentTheme`。

## 9. 失败处理

| 失败点 | 处理 |
|---|---|
| 打开注册表失败 | 返回失败，记 `GetLastError`，气泡提示一次 |
| 写值失败 | 回滚已写的另一个值（尽力而为），不广播 |
| 广播超时 | 视为成功（注册表已改），仅记录日志 |
| 写入成功但读回不一致 | 记录日志，以注册表实际值为准刷新 UI |

## 10. 性能与安全

- 单次切换的注册表操作 + 广播通常 < 30 ms。
- 不申请管理员权限；即使用户以管理员身份运行，程序也只操作当前用户的 `HKCU`。
- 不创建服务、不写计划任务、不修改系统策略。
