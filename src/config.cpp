#include "config.h"

#include "log.h"
#include "util.h"

#include <shlobj.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace config
{
namespace
{

constexpr wchar_t kAppDirName[] = L"TinyTheme";
constexpr wchar_t kConfigFileName[] = L"config.ini";
constexpr wchar_t kPortableMarker[] = L"portable.ini";

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"TinyTheme";

std::wstring trim(std::wstring_view text)
{
    const auto begin = text.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring_view::npos)
    {
        return {};
    }
    const auto end = text.find_last_not_of(L" \t\r\n");
    return std::wstring(text.substr(begin, end - begin + 1));
}

bool starts_with_icase(std::wstring_view text, std::wstring_view prefix)
{
    if (text.size() < prefix.size())
    {
        return false;
    }
    // 显式传长度，避免依赖 data() 的 NUL 结尾。
    return CompareStringOrdinal(text.data(), static_cast<int>(prefix.size()), prefix.data(),
                                static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL;
}

bool parse_bool(std::wstring_view text, bool& value)
{
    const std::wstring trimmed = trim(text);
    if (trimmed == L"1" || _wcsicmp(trimmed.c_str(), L"true") == 0 ||
        _wcsicmp(trimmed.c_str(), L"yes") == 0)
    {
        value = true;
        return true;
    }
    if (trimmed == L"0" || _wcsicmp(trimmed.c_str(), L"false") == 0 ||
        _wcsicmp(trimmed.c_str(), L"no") == 0)
    {
        value = false;
        return true;
    }
    return false;
}

std::optional<Mode> parse_mode(std::wstring_view text)
{
    const std::wstring trimmed = trim(text);
    if (_wcsicmp(trimmed.c_str(), L"light") == 0)
    {
        return Mode::Light;
    }
    if (_wcsicmp(trimmed.c_str(), L"dark") == 0)
    {
        return Mode::Dark;
    }
    return std::nullopt;
}

std::wstring mode_to_string(Mode mode)
{
    return mode == Mode::Dark ? L"dark" : L"light";
}

std::optional<std::filesystem::path> appdata_dir()
{
    const auto known = util::known_folder(FOLDERID_RoamingAppData);
    if (!known)
    {
        logger::error(L"无法获取 %APPDATA% 路径");
        return std::nullopt;
    }
    return *known / kAppDirName;
}

} // namespace

ParseResult parse(std::wstring_view text)
{
    ParseResult result;
    std::wstring section;

    std::size_t position = 0;
    while (position <= text.size())
    {
        const std::size_t line_end = text.find(L'\n', position);
        const std::wstring_view raw_line =
            text.substr(position, line_end == std::wstring_view::npos ? std::wstring_view::npos
                                                                      : line_end - position);
        position = (line_end == std::wstring_view::npos) ? text.size() + 1 : line_end + 1;

        const std::wstring line = trim(raw_line);
        if (line.empty() || line.front() == L';' || line.front() == L'#')
        {
            continue;
        }

        if (line.front() == L'[')
        {
            const auto close = line.find(L']');
            if (close == std::wstring::npos)
            {
                result.had_invalid_value = true;
                continue;
            }
            section = trim(std::wstring_view(line).substr(1, close - 1));
            std::transform(section.begin(), section.end(), section.begin(), towlower);
            continue;
        }

        const auto equals = line.find(L'=');
        if (equals == std::wstring::npos)
        {
            result.had_invalid_value = true;
            continue;
        }

        const std::wstring key = trim(std::wstring_view(line).substr(0, equals));
        const std::wstring value = trim(std::wstring_view(line).substr(equals + 1));

        if (section == L"general")
        {
            if (_wcsicmp(key.c_str(), L"mode") == 0)
            {
                if (const auto mode = parse_mode(value))
                {
                    result.config.mode = *mode;
                }
                else
                {
                    result.had_invalid_value = true;
                }
            }
            else if (_wcsicmp(key.c_str(), L"include_system_theme") == 0)
            {
                bool parsed = true;
                if (parse_bool(value, parsed))
                {
                    result.config.include_system_theme = parsed;
                }
                else
                {
                    result.had_invalid_value = true;
                }
            }
            else if (_wcsicmp(key.c_str(), L"autostart") == 0)
            {
                bool parsed = true;
                if (parse_bool(value, parsed))
                {
                    result.config.autostart = parsed;
                }
                else
                {
                    result.had_invalid_value = true;
                }
            }
            else if (_wcsicmp(key.c_str(), L"show_toast") == 0)
            {
                bool parsed = true;
                if (parse_bool(value, parsed))
                {
                    result.config.show_toast = parsed;
                }
                else
                {
                    result.had_invalid_value = true;
                }
            }
            else if (_wcsicmp(key.c_str(), L"language") == 0)
            {
                result.config.language = value;
            }
        }
        else if (section == L"meta")
        {
            if (_wcsicmp(key.c_str(), L"first_run") == 0)
            {
                bool parsed = true;
                if (parse_bool(value, parsed))
                {
                    result.config.first_run = parsed;
                }
                else
                {
                    result.had_invalid_value = true;
                }
            }
        }
    }

    return result;
}

std::wstring serialize(const Config& config)
{
    std::wstring text;
    text += L"; TinyTheme 配置文件（UTF-8）。修改后重启程序生效。\r\n";
    text += L"\r\n";
    text += L"[general]\r\n";
    text += L"mode=" + mode_to_string(config.mode) + L"\r\n";
    text += L"include_system_theme=" + std::wstring(config.include_system_theme ? L"1" : L"0") +
            L"\r\n";
    text += L"autostart=" + std::wstring(config.autostart ? L"1" : L"0") + L"\r\n";
    text += L"show_toast=" + std::wstring(config.show_toast ? L"1" : L"0") + L"\r\n";
    text += L"language=" + config.language + L"\r\n";
    text += L"\r\n";
    text += L"[meta]\r\n";
    text += L"first_run=" + std::wstring(config.first_run ? L"1" : L"0") + L"\r\n";
    return text;
}

std::optional<std::wstring> decode(std::string_view bytes)
{
    if (bytes.empty())
    {
        return std::wstring{};
    }

    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    if (bytes.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
    {
        return util::to_wide(bytes.substr(3));
    }
    if (bytes.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE)
    {
        const std::wstring_view wide(reinterpret_cast<const wchar_t*>(data + 2),
                                     (bytes.size() - 2) / sizeof(wchar_t));
        return std::wstring(wide);
    }
    return util::to_wide(bytes);
}

std::string encode(std::wstring_view text)
{
    std::string result;
    result.push_back(static_cast<char>(0xEF));
    result.push_back(static_cast<char>(0xBB));
    result.push_back(static_cast<char>(0xBF));
    result += util::to_utf8(text);
    return result;
}

bool is_portable()
{
    const auto module = util::module_path();
    if (!module)
    {
        return false;
    }
    std::error_code error;
    return std::filesystem::exists(module->parent_path() / kPortableMarker, error);
}

std::filesystem::path directory()
{
    if (is_portable())
    {
        if (const auto module = util::module_path())
        {
            return module->parent_path();
        }
    }
    if (const auto roaming = appdata_dir())
    {
        return *roaming;
    }
    return std::filesystem::current_path();
}

std::filesystem::path file_path()
{
    return directory() / kConfigFileName;
}

Config load()
{
    const auto path = file_path();
    std::error_code error;
    if (!std::filesystem::exists(path, error))
    {
        logger::info(L"配置文件不存在，使用默认值");
        return Config{};
    }

    std::vector<std::uint8_t> bytes;
    if (!util::read_file_bytes(path, bytes))
    {
        logger::error(L"读取配置文件失败，使用默认值");
        return Config{};
    }

    const std::string_view raw(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    const auto decoded = decode(raw);
    if (!decoded)
    {
        logger::error(L"配置文件编码无效，备份后重建默认值");
        std::filesystem::rename(path, path.wstring() + L".bak", error);
        return Config{};
    }

    const ParseResult parsed = parse(*decoded);
    if (parsed.had_invalid_value)
    {
        logger::warn(L"配置文件包含非法值，已回退默认");
    }
    return parsed.config;
}

bool save(const Config& config)
{
    const auto path = file_path();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        logger::error(util::format_string(L"创建配置目录失败：%s", path.parent_path().c_str()));
        return false;
    }

    const std::string text = encode(serialize(config));
    const auto temporary = path.wstring() + L".tmp";

    std::vector<std::uint8_t> bytes(text.begin(), text.end());
    if (!util::write_file_bytes(temporary, bytes))
    {
        logger::error(util::format_string(L"写入临时配置失败：%s", temporary.c_str()));
        return false;
    }

    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const DWORD move_error = GetLastError();
        logger::error(util::format_string(L"替换配置失败：%lu %s", move_error,
                                          util::win32_error_message(move_error).c_str()));
        DeleteFileW(temporary.c_str());
        return false;
    }

    return true;
}

bool is_autostart_enabled()
{
    DWORD size = 0;
    const LSTATUS probe = RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValueName, RRF_RT_REG_SZ,
                                       nullptr, nullptr, &size);
    return probe == ERROR_SUCCESS;
}

bool set_autostart(bool enabled)
{
    util::UniqueHkey key;
    HKEY raw = nullptr;
    const LSTATUS open_result = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &raw);
    if (open_result != ERROR_SUCCESS)
    {
        logger::error(util::format_string(L"打开 Run 键失败：%lu %s", open_result,
                                          util::win32_error_message(open_result).c_str()));
        return false;
    }
    key.reset(raw);

    if (!enabled)
    {
        const LSTATUS delete_result = RegDeleteValueW(key.get(), kRunValueName);
        if (delete_result != ERROR_SUCCESS && delete_result != ERROR_FILE_NOT_FOUND)
        {
            logger::error(util::format_string(L"删除自启值失败：%lu %s", delete_result,
                                              util::win32_error_message(delete_result).c_str()));
            return false;
        }
        return true;
    }

    const auto module = util::module_path();
    if (!module)
    {
        logger::error(L"无法获取 exe 路径，无法写自启");
        return false;
    }

    const std::wstring quoted = util::quote_path(*module);
    const LSTATUS write_result = RegSetValueExW(
        key.get(), kRunValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(quoted.c_str()),
        static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
    if (write_result != ERROR_SUCCESS)
    {
        logger::error(util::format_string(L"写入自启值失败：%lu %s", write_result,
                                          util::win32_error_message(write_result).c_str()));
        return false;
    }
    return true;
}

} // namespace config
