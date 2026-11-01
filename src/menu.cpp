#include "menu.h"

#include "log.h"
#include "resource.h"
#include "util.h"

#include <windows.h>

namespace
{

// uxtheme 的 SetPreferredAppMode / FlushMenuThemes 没有公开导出名，只能按序号取。
// 取不到就静默回退到系统默认菜单（规范 §10：未文档化 API 不能作为唯一实现路径）。
enum class PreferredAppMode : int
{
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3,
    Max = 4,
};

using SetPreferredAppModeFn = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FlushMenuThemesFn = void(WINAPI*)();

SetPreferredAppModeFn g_set_preferred_app_mode = nullptr;
FlushMenuThemesFn g_flush_menu_themes = nullptr;
bool g_theme_api_checked = false;

void load_theme_api()
{
    if (g_theme_api_checked)
    {
        return;
    }
    g_theme_api_checked = true;

    const HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (uxtheme == nullptr)
    {
        logger::warn(L"加载 uxtheme.dll 失败，菜单使用系统默认主题");
        return;
    }

    g_set_preferred_app_mode =
        reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
    g_flush_menu_themes =
        reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));

    if (g_set_preferred_app_mode == nullptr)
    {
        logger::info(L"uxtheme!SetPreferredAppMode 不可用，菜单使用系统默认主题");
    }
}

void apply_menu_theme(bool dark)
{
    load_theme_api();
    if (g_set_preferred_app_mode == nullptr)
    {
        return;
    }

    g_set_preferred_app_mode(dark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
    if (g_flush_menu_themes != nullptr)
    {
        g_flush_menu_themes();
    }
}

} // namespace

namespace menu
{

UINT show(HWND owner, HINSTANCE instance, const config::Config& config,
          const theme::ThemeSnapshot& snapshot, bool autostart_enabled)
{
    // 深色菜单跟随系统主题（任务栏颜色），与托盘图标保持一致。
    apply_menu_theme(!snapshot.system_light);

    util::UniqueMenu popup(CreatePopupMenu());
    if (!popup)
    {
        return 0;
    }

    const std::wstring status = theme::status_text(snapshot);
    AppendMenuW(popup.get(), MF_STRING | MF_GRAYED, 0, status.c_str());
    AppendMenuW(popup.get(), MF_SEPARATOR, 0, nullptr);

    AppendMenuW(popup.get(), MF_STRING, ids::kModeLight,
                util::load_string(instance, IDS_MENU_LIGHT).c_str());
    AppendMenuW(popup.get(), MF_STRING, ids::kModeDark,
                util::load_string(instance, IDS_MENU_DARK).c_str());

    CheckMenuRadioItem(popup.get(), ids::kModeLight, ids::kModeDark,
                       config.mode == config::Mode::Dark ? ids::kModeDark : ids::kModeLight,
                       MF_BYCOMMAND);

    AppendMenuW(popup.get(), MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup.get(), MF_STRING | (autostart_enabled ? MF_CHECKED : MF_UNCHECKED),
                ids::kAutostart, util::load_string(instance, IDS_MENU_AUTOSTART).c_str());

    AppendMenuW(popup.get(), MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup.get(), MF_STRING, ids::kExit,
                util::load_string(instance, IDS_MENU_EXIT).c_str());

    POINT cursor{};
    GetCursorPos(&cursor);

    SetForegroundWindow(owner);
    const UINT command = TrackPopupMenuEx(popup.get(), TPM_RETURNCMD | TPM_NONOTIFY, cursor.x,
                                          cursor.y, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    return command;
}

} // namespace menu
