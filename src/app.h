#pragma once

#include <windows.h>

#include "config.h"
#include "theme.h"

class App
{
  public:
    explicit App(HINSTANCE instance);
    ~App();

    int run(int show_command);

    void on_tray_event(UINT event, LPARAM position);
    void on_command(UINT command);
    void on_setting_change();
    void on_second_instance();
    void on_taskbar_created();
    void on_display_change();

  private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    bool create_window();
    void shutdown();

    void toggle();
    void set_mode(config::Mode mode);
    void apply_theme(bool light);
    void refresh_ui();
    void show_menu();
    void show_toast(UINT text_id);
    void reload_autostart();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    config::Config config_{};
    theme::ThemeSnapshot snapshot_{};
    bool autostart_enabled_ = false;
    bool apply_fail_toasted_ = false;
    ULONGLONG last_toggle_tick_ = 0;
};
