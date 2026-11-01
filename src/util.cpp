#include "util.h"

#include <algorithm>
#include <cstdarg>
#include <vector>

namespace util
{

// 一次读写 1 MiB，避免超大块分配。
constexpr std::size_t kChunkSize = static_cast<std::size_t>(1) << 20;

UniqueHandle::~UniqueHandle()
{
    reset();
}

UniqueHandle::UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}

UniqueHandle& UniqueHandle::operator=(UniqueHandle&& other) noexcept
{
    if (this != &other)
    {
        reset(other.release());
    }
    return *this;
}

HANDLE UniqueHandle::release() noexcept
{
    HANDLE handle = handle_;
    handle_ = nullptr;
    return handle;
}

void UniqueHandle::reset(HANDLE handle) noexcept
{
    if (handle_ != nullptr)
    {
        CloseHandle(handle_);
    }
    handle_ = (handle == INVALID_HANDLE_VALUE) ? nullptr : handle;
}

UniqueHkey::~UniqueHkey()
{
    reset();
}

UniqueHkey::UniqueHkey(UniqueHkey&& other) noexcept : key_(other.release()) {}

UniqueHkey& UniqueHkey::operator=(UniqueHkey&& other) noexcept
{
    if (this != &other)
    {
        reset(other.release());
    }
    return *this;
}

HKEY UniqueHkey::release() noexcept
{
    HKEY key = key_;
    key_ = nullptr;
    return key;
}

void UniqueHkey::reset(HKEY key) noexcept
{
    if (key_ != nullptr)
    {
        RegCloseKey(key_);
    }
    key_ = key;
}

UniqueMenu::~UniqueMenu()
{
    reset();
}

UniqueMenu::UniqueMenu(UniqueMenu&& other) noexcept : menu_(other.release()) {}

UniqueMenu& UniqueMenu::operator=(UniqueMenu&& other) noexcept
{
    if (this != &other)
    {
        reset(other.release());
    }
    return *this;
}

HMENU UniqueMenu::release() noexcept
{
    HMENU menu = menu_;
    menu_ = nullptr;
    return menu;
}

void UniqueMenu::reset(HMENU menu) noexcept
{
    if (menu_ != nullptr)
    {
        DestroyMenu(menu_);
    }
    menu_ = menu;
}

UniqueIcon::~UniqueIcon()
{
    reset();
}

UniqueIcon::UniqueIcon(UniqueIcon&& other) noexcept : icon_(other.release()) {}

UniqueIcon& UniqueIcon::operator=(UniqueIcon&& other) noexcept
{
    if (this != &other)
    {
        reset(other.release());
    }
    return *this;
}

HICON UniqueIcon::release() noexcept
{
    HICON icon = icon_;
    icon_ = nullptr;
    return icon;
}

void UniqueIcon::reset(HICON icon) noexcept
{
    if (icon_ != nullptr)
    {
        DestroyIcon(icon_);
    }
    icon_ = icon;
}

std::wstring win32_error_message(DWORD error)
{
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    if (length == 0 || message == nullptr)
    {
        return L"error " + std::to_wstring(error);
    }

    std::wstring result(message, length);
    LocalFree(message);
    while (!result.empty() &&
           (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' '))
    {
        result.pop_back();
    }
    return result;
}

std::string to_utf8(std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size,
                        nullptr, nullptr);
    return result;
}

std::optional<std::wstring> to_wide(std::string_view text)
{
    if (text.empty())
    {
        return std::wstring{};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
    {
        return std::nullopt;
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                        result.data(), size);
    return result;
}

std::wstring format_string(const wchar_t* format, ...)
{
    va_list args = nullptr;
    va_start(args, format);
    const int size = _vscwprintf(format, args);
    va_end(args);
    if (size <= 0)
    {
        return {};
    }

    std::vector<wchar_t> buffer(static_cast<std::size_t>(size) + 1, L'\0');
    va_start(args, format);
    _vsnwprintf_s(buffer.data(), buffer.size(), _TRUNCATE, format, args);
    va_end(args);
    return {buffer.data(), static_cast<std::size_t>(size)};
}

std::wstring load_string(HINSTANCE instance, UINT id)
{
    wchar_t buffer[512] = {};
    const int length = LoadStringW(instance, id, buffer, static_cast<int>(std::size(buffer)));
    if (length <= 0)
    {
        return {};
    }
    return {buffer, static_cast<std::size_t>(length)};
}

std::optional<std::filesystem::path> module_path()
{
    std::vector<wchar_t> buffer(512);
    for (;;)
    {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return std::nullopt;
        }
        if (length < buffer.size() - 1)
        {
            return std::filesystem::path(std::wstring(buffer.data(), length));
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::optional<std::filesystem::path> known_folder(REFKNOWNFOLDERID folder_id)
{
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT, nullptr, &raw)))
    {
        return std::nullopt;
    }
    std::filesystem::path path(raw);
    CoTaskMemFree(raw);
    return path;
}

std::wstring quote_path(const std::filesystem::path& path)
{
    return L"\"" + path.wstring() + L"\"";
}

bool read_file_bytes(const std::filesystem::path& path, std::vector<std::uint8_t>& data)
{
    const UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file)
    {
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart < 0 || size.QuadPart > 1024 * 1024)
    {
        return false;
    }

    data.resize(static_cast<std::size_t>(size.QuadPart));
    std::size_t offset = 0;
    while (offset < data.size())
    {
        const DWORD chunk =
            static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, kChunkSize));
        DWORD read = 0;
        if (!ReadFile(file.get(), data.data() + offset, chunk, &read, nullptr) || read == 0)
        {
            return false;
        }
        offset += read;
    }
    return true;
}

bool write_file_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& data)
{
    const UniqueHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file)
    {
        return false;
    }

    std::size_t offset = 0;
    while (offset < data.size())
    {
        const DWORD chunk =
            static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, kChunkSize));
        DWORD written = 0;
        if (!WriteFile(file.get(), data.data() + offset, chunk, &written, nullptr) ||
            written != chunk)
        {
            return false;
        }
        offset += written;
    }

    return FlushFileBuffers(file.get()) != FALSE;
}

} // namespace util
