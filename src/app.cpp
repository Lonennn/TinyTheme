#include "app.h"

#include "log.h"
#include "menu.h"
#include "resource.h"
#include "tray.h"
#include "util.h"

#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <string_view>

namespace
{

constexpr wchar_t kWindowClass[] = L"TinyTheme.MainWindow";
constexpr UINT kSecondInstanceMessage = WM_APP + 1;
constexpr UINT kTrayCallbackMessage = WM_APP + 100;
constexpr ULONGLONG kToggleDebounceMs = 200;

UINT g_taskbar_created_message = 0;

} // namespace

App::App(HINSTANCE instance) : instance_(instance) {}

App::~App()
{
    shutdown();
}

bool App::create_window()
{
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &App::window_proc;
    window_class.hInstance = instance_;
    window_class.lpszClassName = kWindowClass;

    if (RegisterClassExW(&window_class) == 0)
    {
        const DWORD error = GetLastError();
        logger::error(util::format_string(L"RegisterClassExW 失败：%lu %s", error,
                                          util::win32_error_message(error).c_str()));
        return false;
    }

    window_ = CreateWindowExW(0, kWindowClass, L"TinyTheme", WS_OVERLAPPED, 0, 0, 0, 0, nullptr,
                              nullptr, instance_, this);
    if (window_ == nullptr)
    {
        const DWORD error = GetLastError();
        logger::error(util::format_string(L"CreateWindowExW 失败：%lu %s", error,
                                          util::win32_error_message(error).c_str()));
        return false;
    }

    return true;
}

void App::shutdown()
{
    if (window_ == nullptr)
    {
        return;
    }

    tray::destroy();
    DestroyWindow(window_);
    window_ = nullptr;
}

int App::run(int show_command)
{
    (void) show_command;

    logger::info(L"TinyTheme 启动");

    g_taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");

    config_ = config::load();
    snapshot_ = theme::read();
    // v0.1：启动不写主题，以注册表实际值为准同步状态。
    config_.mode = snapshot_.apps_light ? config::Mode::Light : config::Mode::Dark;
    reload_autostart();

    if (!create_window())
    {
        // 没有窗口就没有任何可见反馈，必须让用户知道发生了什么。
        MessageBoxW(nullptr, util::load_string(instance_, IDS_ERR_STARTUP).c_str(),
                    util::load_string(instance_, IDS_APP_NAME).c_str(), MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!tray::create(window_, instance_))
    {
        // 典型原因：explorer.exe 未运行，通知区域不存在。
        logger::error(L"创建托盘图标失败，退出");
        MessageBoxW(nullptr, util::load_string(instance_, IDS_ERR_TRAY).c_str(),
                    util::load_string(instance_, IDS_APP_NAME).c_str(), MB_ICONERROR | MB_OK);
        return 1;
    }

    (void) config::save(config_);

    if (config_.first_run)
    {
        show_toast(IDS_TOAST_FIRST_RUN);
        config_.first_run = false;
        (void) config::save(config_);
    }

    refresh_ui();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    logger::info(L"TinyTheme 退出");
    return 0;
}

void App::reload_autostart()
{
    autostart_enabled_ = config::is_autostart_enabled();
}

void App::refresh_ui()
{
    const std::wstring status = theme::status_text(snapshot_);
    const std::wstring tooltip = util::format_string(
        util::load_string(instance_, IDS_TOOLTIP_FORMAT).c_str(), status.c_str());
    tray::set_tooltip(tooltip);
    tray::set_theme(snapshot_.system_light);
}

void App::show_toast(UINT text_id)
{
    if (!config_.show_toast)
    {
        return;
    }
    tray::show_balloon(instance_, IDS_APP_NAME, text_id);
}

void App::toggle()
{
    const ULONGLONG now = GetTickCount64();
    if (now - last_toggle_tick_ < kToggleDebounceMs)
    {
        logger::info(L"忽略 200 ms 内的重复切换");
        return;
    }
    last_toggle_tick_ = now;

    const theme::ThemeSnapshot current = theme::read();
    const bool target_light = !current.apps_light;
    apply_theme(target_light);
}

void App::apply_theme(bool light)
{
    if (!theme::apply(light, config_.include_system_theme))
    {
        logger::error(L"主题写入失败");
        if (!apply_fail_toasted_)
        {
            show_toast(IDS_TOAST_APPLY_FAIL);
            apply_fail_toasted_ = true;
        }
        snapshot_ = theme::read();
        refresh_ui();
        return;
    }

    snapshot_ = theme::read();
    config_.mode = snapshot_.apps_light ? config::Mode::Light : config::Mode::Dark;
    (void) config::save(config_);
    refresh_ui();
}

void App::set_mode(config::Mode mode)
{
    const bool light = mode == config::Mode::Light;
    apply_theme(light);
}

void App::show_menu()
{
    reload_autostart();
    const UINT command = menu::show(window_, instance_, config_, snapshot_, autostart_enabled_);
    if (command != 0)
    {
        on_command(command);
    }
}

void App::on_command(UINT command)
{
    switch (command)
    {
    case menu::ids::kModeLight:
        set_mode(config::Mode::Light);
        break;
    case menu::ids::kModeDark:
        set_mode(config::Mode::Dark);
        break;
    case menu::ids::kAutostart:
        if (config::set_autostart(!autostart_enabled_))
        {
            config_.autostart = !autostart_enabled_;
            reload_autostart();
            config_.autostart = autostart_enabled_;
            (void) config::save(config_);
        }
        break;
    case menu::ids::kExit:
        tray::destroy();
        DestroyWindow(window_);
        window_ = nullptr;
        PostQuitMessage(0);
        break;
    default:
        logger::warn(util::format_string(L"忽略未处理命令 %u", command));
        break;
    }
}

void App::on_tray_event(UINT event, LPARAM position)
{
    (void) position;
    switch (event)
    {
    case WM_CONTEXTMENU:
        show_menu();
        break;
    case NIN_SELECT:
    case NIN_KEYSELECT:
        toggle();
        break;
    default:
        break;
    }
}

void App::on_setting_change()
{
    snapshot_ = theme::read();
    config_.mode = snapshot_.apps_light ? config::Mode::Light : config::Mode::Dark;
    refresh_ui();
}

void App::on_second_instance()
{
    show_menu();
}

void App::on_taskbar_created()
{
    tray::destroy();
    tray::create(window_, instance_);
    refresh_ui();
}

void App::on_display_change()
{
    snapshot_ = theme::read();
    tray::set_theme(snapshot_.system_light);
    refresh_ui();
}

LRESULT CALLBACK App::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    App* self = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr)
        {
            self->window_ = window;
        }
    }
    else
    {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }

    switch (message)
    {
    case kSecondInstanceMessage:
        self->on_second_instance();
        return 0;
    case kTrayCallbackMessage:
        self->on_tray_event(LOWORD(lparam), static_cast<LPARAM>(HIWORD(lparam)));
        return 0;
    case WM_SETTINGCHANGE:
    {
        const auto* topic = reinterpret_cast<const wchar_t*>(lparam);
        if (topic != nullptr && wcscmp(topic, L"ImmersiveColorSet") == 0)
        {
            self->on_setting_change();
        }
        return 0;
    }
    case WM_CLOSE:
        return 0;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (wparam != 0)
        {
            (void) config::save(self->config_);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        // DPI 变化后系统托盘的图标尺寸会变（16/20/24/32），按新尺寸重新加载。
        self->on_display_change();
        return 0;
    default:
        break;
    }

    if (g_taskbar_created_message != 0 && message == g_taskbar_created_message)
    {
        self->on_taskbar_created();
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}
