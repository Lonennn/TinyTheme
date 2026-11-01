#include "app.h"
#include "config.h"
#include "log.h"
#include "util.h"

#include <windows.h>

#include <filesystem>

namespace
{

// 日志路径按以下顺序回退：LOCALAPPDATA -> exe 同目录 -> 临时目录。
// 全部失败时禁用日志，但不阻止程序启动。
std::filesystem::path resolve_log_path()
{
    if (const auto local = util::known_folder(FOLDERID_LocalAppData))
    {
        return *local / L"TinyTheme" / L"logs" / L"app.log";
    }
    if (const auto module = util::module_path())
    {
        return module->parent_path() / L"logs" / L"app.log";
    }
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetTempPathW(MAX_PATH, buffer);
    if (length > 0)
    {
        return std::filesystem::path(buffer) / L"TinyTheme" / L"logs" / L"app.log";
    }
    return {};
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    (void) previous;
    (void) command_line;

    // GetLastError 在成功后不会被清零，必须显式重置后再判断"已存在"。
    SetLastError(ERROR_SUCCESS);
    util::UniqueHandle mutex(CreateMutexW(nullptr, FALSE, L"Local\\TinyTheme.Instance"));
    const DWORD mutex_error = GetLastError();
    if (mutex && mutex_error == ERROR_ALREADY_EXISTS)
    {
        HWND existing = FindWindowW(L"TinyTheme.MainWindow", nullptr);
        if (existing != nullptr)
        {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessageW(existing, WM_APP + 1, 0, 0);
        }
        return 0;
    }

    logger::init(resolve_log_path());

    App app(instance);
    return app.run(show_command);
}
