#pragma once

#include <windows.h>

#include "config.h"
#include "theme.h"

namespace menu
{
namespace ids
{

constexpr UINT kModeLight = 1001;
constexpr UINT kModeDark = 1002;
constexpr UINT kModeAuto = 1003; // v0.2 保留
constexpr UINT kAutostart = 1004;
constexpr UINT kOpenConfig = 1005; // v0.2 保留
constexpr UINT kAbout = 1006;      // v0.2 保留
constexpr UINT kExit = 1007;

} // namespace ids

// 弹出右键菜单，返回选中的命令 ID；未选择返回 0。
UINT show(HWND owner, HINSTANCE instance, const config::Config& config,
          const theme::ThemeSnapshot& snapshot, bool autostart_enabled);

} // namespace menu
