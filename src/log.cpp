#include "log.h"

#include "util.h"

#include <windows.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace logger
{
namespace
{

constexpr std::uintmax_t kMaxLogSize = 256 * 1024;
constexpr int kMaxHistoryFiles = 3;

std::filesystem::path g_path;
std::filesystem::path g_fallback_path;
bool g_fallback_attempted = false;

// 首选路径不可写时（便携目录只读、沙箱限制等），退到 exe 同目录的 logs；
// 仍然失败则禁用日志。日志失败永远不能影响主流程。
bool switch_to_fallback()
{
    if (g_fallback_attempted)
    {
        return false;
    }
    g_fallback_attempted = true;

    if (const auto module = util::module_path())
    {
        g_fallback_path = module->parent_path() / L"logs" / L"app.log";
        g_path = g_fallback_path;
        return true;
    }
    return false;
}

const wchar_t* level_name(Level level)
{
    switch (level)
    {
    case Level::Info:
        return L"info";
    case Level::Warn:
        return L"warn";
    case Level::Error:
        return L"error";
    }
    return L"info";
}

std::wstring timestamp()
{
    SYSTEMTIME local{};
    GetLocalTime(&local);

    TIME_ZONE_INFORMATION zone{};
    const DWORD zone_result = GetTimeZoneInformation(&zone);
    int offset_minutes = -static_cast<int>(zone.Bias);
    if (zone_result == TIME_ZONE_ID_DAYLIGHT)
    {
        offset_minutes -= static_cast<int>(zone.DaylightBias);
    }
    else if (zone_result == TIME_ZONE_ID_STANDARD)
    {
        offset_minutes -= static_cast<int>(zone.StandardBias);
    }

    const wchar_t sign = offset_minutes < 0 ? L'-' : L'+';
    const int absolute = offset_minutes < 0 ? -offset_minutes : offset_minutes;

    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02u.%03u%c%02d:%02d", local.wYear, local.wMonth,
               local.wDay, local.wHour, local.wMinute, local.wSecond, local.wMilliseconds, sign,
               absolute / 60, absolute % 60);
    return buffer;
}

std::filesystem::path history_path(int index)
{
    return g_path.parent_path() /
           (g_path.stem().wstring() + L"." + std::to_wstring(index) + g_path.extension().wstring());
}

void rotate_if_needed()
{
    std::error_code error;
    const auto size = std::filesystem::file_size(g_path, error);
    if (error || size <= kMaxLogSize)
    {
        return;
    }

    std::filesystem::remove(history_path(kMaxHistoryFiles), error);
    for (int index = kMaxHistoryFiles - 1; index >= 1; --index)
    {
        std::filesystem::remove(history_path(index + 1), error);
        error.clear();
        std::filesystem::rename(history_path(index), history_path(index + 1), error);
        error.clear();
    }
    std::filesystem::remove(history_path(1), error);
    error.clear();
    std::filesystem::rename(g_path, history_path(1), error);
}

void append_line(const std::wstring& line)
{
    if (g_path.empty())
    {
        return;
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        std::error_code error;
        std::filesystem::create_directories(g_path.parent_path(), error);

        rotate_if_needed();

        util::UniqueHandle file(CreateFileW(g_path.c_str(), FILE_APPEND_DATA,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (file)
        {
            const std::string utf8 = util::to_utf8(line) + "\r\n";
            DWORD written = 0;
            WriteFile(file.get(), utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
            FlushFileBuffers(file.get());
            return;
        }

        if (!switch_to_fallback())
        {
            return;
        }
    }
}

} // namespace

void init(const std::filesystem::path& path)
{
    g_path = path;
}

void write(Level level, std::wstring_view message)
{
    if (g_path.empty())
    {
        return;
    }

    std::wstring line = timestamp();
    line += L" [";
    line += level_name(level);
    line += L"] ";
    line.append(message);
    append_line(line);
}

void info(std::wstring_view message)
{
    write(Level::Info, message);
}

void warn(std::wstring_view message)
{
    write(Level::Warn, message);
}

void error(std::wstring_view message)
{
    write(Level::Error, message);
}

} // namespace logger
