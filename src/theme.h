#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace theme
{

struct ThemeSnapshot
{
    bool apps_light = true;
    bool system_light = true;
};

// 读取当前用户的应用主题与系统主题。值缺失或读取失败时按浅色处理。
[[nodiscard]] ThemeSnapshot read();

// 写入主题并广播；include_system 为 false 时只写应用主题。
// 任一写入失败会尽力回滚已写的值，并且不广播。
[[nodiscard]] bool apply(bool light, bool include_system);

// 向所有顶层窗口广播 ImmersiveColorSet。广播超时视为成功（注册表已改）。
void broadcast_change();

// 状态摘要：用于菜单与工具提示。
[[nodiscard]] std::wstring status_text(const ThemeSnapshot& snapshot);

} // namespace theme
