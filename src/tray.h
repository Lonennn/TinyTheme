#pragma once

#include <windows.h>

#include <string_view>

namespace tray
{

bool create(HWND owner, HINSTANCE instance);
void destroy();

void set_theme(bool system_light);
void set_tooltip(std::wstring_view text);

void show_balloon(HINSTANCE instance, UINT title_id, UINT text_id);

} // namespace tray
