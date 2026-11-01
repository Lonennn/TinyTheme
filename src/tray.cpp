#include "tray.h"

#include "log.h"
#include "resource.h"
#include "util.h"

#include <shellapi.h>
#include <windows.h>

namespace tray
{
namespace
{

constexpr UINT kTrayCallbackMessage = WM_APP + 100;
constexpr UINT kTrayIconId = 1;
constexpr int kAddAttempts = 3;
constexpr DWORD kAddRetryDelayMs = 200;

NOTIFYICONDATAW g_icon{};
bool g_created = false;
HINSTANCE g_instance = nullptr;
util::UniqueIcon g_current;

HICON load_icon(HINSTANCE instance, bool system_light)
{
    const UINT id = system_light ? IDI_LIGHT : IDI_DARK;
    const int size = GetSystemMetrics(SM_CXSMICON);

    // 用 LoadImageW 并显式给尺寸，才能在 16/20/24/32 里挑到最接近的一档。
    HICON icon = static_cast<HICON>(
        LoadImageW(instance, MAKEINTRESOURCEW(id), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    if (icon == nullptr)
    {
        logger::warn(util::format_string(L"LoadImageW(%u, %d) 失败，回退默认图标", id, size));
        icon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    }
    return icon;
}

} // namespace

bool create(HWND owner, HINSTANCE instance)
{
    g_instance = instance;

    g_icon = {};
    g_icon.cbSize = sizeof(g_icon);
    g_icon.hWnd = owner;
    g_icon.uID = kTrayIconId;
    g_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    g_icon.uCallbackMessage = kTrayCallbackMessage;
    g_icon.hIcon = load_icon(instance, true);
    wcscpy_s(g_icon.szTip, L"TinyTheme");

    // 资源管理器可能刚启动或正在重启，NIM_ADD 失败时短暂重试几次。
    for (int attempt = 0; attempt < kAddAttempts; ++attempt)
    {
        SetLastError(ERROR_SUCCESS);
        if (Shell_NotifyIconW(NIM_ADD, &g_icon))
        {
            g_icon.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &g_icon);
            g_created = true;
            return true;
        }

        const DWORD error = GetLastError();
        logger::warn(util::format_string(L"Shell_NotifyIconW(NIM_ADD) 失败（第 %d 次）：%lu %s",
                                         attempt + 1, error,
                                         util::win32_error_message(error).c_str()));
        if (attempt + 1 < kAddAttempts)
        {
            Sleep(kAddRetryDelayMs);
        }
    }

    return false;
}

void destroy()
{
    if (g_created)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_icon);
        g_created = false;
    }
    g_current.reset();
}

void set_theme(bool system_light)
{
    if (!g_created)
    {
        return;
    }

    HICON icon = load_icon(g_instance, system_light);
    if (icon == nullptr)
    {
        return;
    }

    g_icon.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    g_icon.hIcon = icon;
    Shell_NotifyIconW(NIM_MODIFY, &g_icon);
    g_current.reset(icon);
}

void set_tooltip(std::wstring_view text)
{
    if (!g_created)
    {
        return;
    }

    const std::wstring copy(text);
    wcsncpy_s(g_icon.szTip, copy.c_str(), _TRUNCATE);
    g_icon.uFlags = NIF_TIP | NIF_SHOWTIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_icon);
}

void show_balloon(HINSTANCE instance, UINT title_id, UINT text_id)
{
    if (!g_created)
    {
        return;
    }

    const std::wstring title = util::load_string(instance, title_id);
    const std::wstring text = util::load_string(instance, text_id);

    g_icon.uFlags = NIF_INFO;
    g_icon.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(g_icon.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(g_icon.szInfo, text.c_str(), _TRUNCATE);
    g_icon.uTimeout = 5000;

    if (!Shell_NotifyIconW(NIM_MODIFY, &g_icon))
    {
        logger::warn(L"NIM_MODIFY(NIF_INFO) 失败，气泡未显示");
    }
}

} // namespace tray
