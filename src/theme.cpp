#include "theme.h"

#include "log.h"
#include "resource.h"
#include "util.h"

#include <windows.h>

#include <string>

namespace theme
{
namespace
{

constexpr wchar_t kPersonalize[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kAppsUseLightTheme[] = L"AppsUseLightTheme";
constexpr wchar_t kSystemUsesLightTheme[] = L"SystemUsesLightTheme";

std::optional<DWORD> read_dword(const wchar_t* name)
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS result =
        RegGetValueW(HKEY_CURRENT_USER, kPersonalize, name, RRF_RT_REG_DWORD, &type, &value, &size);
    if (result != ERROR_SUCCESS)
    {
        logger::warn(util::format_string(L"RegGetValueW(%s) failed: %lu %s", name, result,
                                         util::win32_error_message(result).c_str()));
        return std::nullopt;
    }
    return value;
}

bool write_dword(HKEY key, const wchar_t* name, DWORD value)
{
    const LSTATUS result = RegSetKeyValueW(key, nullptr, name, REG_DWORD, &value, sizeof(value));
    if (result != ERROR_SUCCESS)
    {
        logger::error(util::format_string(L"RegSetKeyValueW(%s) failed: %lu %s", name, result,
                                          util::win32_error_message(result).c_str()));
        return false;
    }
    return true;
}

} // namespace

ThemeSnapshot read()
{
    ThemeSnapshot snapshot;
    if (const auto value = read_dword(kAppsUseLightTheme))
    {
        snapshot.apps_light = *value != 0;
    }
    if (const auto value = read_dword(kSystemUsesLightTheme))
    {
        snapshot.system_light = *value != 0;
    }
    return snapshot;
}

bool apply(bool light, bool include_system)
{
    const DWORD value = light ? 1u : 0u;

    util::UniqueHkey key;
    HKEY raw = nullptr;
    const LSTATUS open_result =
        RegOpenKeyExW(HKEY_CURRENT_USER, kPersonalize, 0, KEY_SET_VALUE, &raw);
    if (open_result != ERROR_SUCCESS)
    {
        logger::error(util::format_string(L"RegOpenKeyExW(Personalize) failed: %lu %s", open_result,
                                          util::win32_error_message(open_result).c_str()));
        return false;
    }
    key.reset(raw);

    const ThemeSnapshot before = read();

    if (!write_dword(key.get(), kAppsUseLightTheme, value))
    {
        return false;
    }

    if (include_system && !write_dword(key.get(), kSystemUsesLightTheme, value))
    {
        // 尽力回滚应用主题，避免半切换。
        write_dword(key.get(), kAppsUseLightTheme, before.apps_light ? 1u : 0u);
        logger::warn(L"已回滚 AppsUseLightTheme，未广播");
        return false;
    }

    broadcast_change();

    const ThemeSnapshot after = read();
    if (after.apps_light != light || (include_system && after.system_light != light))
    {
        logger::warn(util::format_string(L"写入后读回不一致：apps=%d system=%d expected=%d",
                                         after.apps_light ? 1 : 0, after.system_light ? 1 : 0,
                                         light ? 1 : 0));
    }

    return true;
}

void broadcast_change()
{
    DWORD_PTR result = 0;
    const ULONG_PTR timeout = 5000;
    const LRESULT sent = SendMessageTimeoutW(
        HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
        SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, static_cast<UINT>(timeout), &result);
    if (sent == 0)
    {
        const DWORD error = GetLastError();
        logger::warn(util::format_string(L"广播 WM_SETTINGCHANGE 超时或失败：%lu %s", error,
                                         util::win32_error_message(error).c_str()));
    }
}

std::wstring status_text(const ThemeSnapshot& snapshot)
{
    if (snapshot.apps_light == snapshot.system_light)
    {
        return util::load_string(nullptr, snapshot.apps_light ? IDS_STATUS_LIGHT : IDS_STATUS_DARK);
    }
    return util::load_string(nullptr, snapshot.apps_light ? IDS_STATUS_MIXED_LIGHT_APPS
                                                          : IDS_STATUS_MIXED_DARK_APPS);
}

} // namespace theme
