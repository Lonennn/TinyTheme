#pragma once

#include <shlobj.h>
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace util
{

class UniqueHandle
{
  public:
    UniqueHandle() noexcept = default;
    // CreateFileW 等 API 失败时返回 INVALID_HANDLE_VALUE 而不是 nullptr，
    // 必须在这里归一化，否则 operator bool 会把失败当成成功。
    explicit UniqueHandle(HANDLE handle) noexcept
        : handle_(handle == INVALID_HANDLE_VALUE ? nullptr : handle)
    {
    }
    ~UniqueHandle();

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept;
    UniqueHandle& operator=(UniqueHandle&& other) noexcept;

    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return handle_ != nullptr;
    }

    HANDLE release() noexcept;
    void reset(HANDLE handle = nullptr) noexcept;

  private:
    HANDLE handle_ = nullptr;
};

class UniqueHkey
{
  public:
    UniqueHkey() noexcept = default;
    explicit UniqueHkey(HKEY key) noexcept : key_(key) {}
    ~UniqueHkey();

    UniqueHkey(const UniqueHkey&) = delete;
    UniqueHkey& operator=(const UniqueHkey&) = delete;

    UniqueHkey(UniqueHkey&& other) noexcept;
    UniqueHkey& operator=(UniqueHkey&& other) noexcept;

    [[nodiscard]] HKEY get() const noexcept
    {
        return key_;
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return key_ != nullptr;
    }

    HKEY release() noexcept;
    void reset(HKEY key = nullptr) noexcept;

  private:
    HKEY key_ = nullptr;
};

class UniqueMenu
{
  public:
    UniqueMenu() noexcept = default;
    explicit UniqueMenu(HMENU menu) noexcept : menu_(menu) {}
    ~UniqueMenu();

    UniqueMenu(const UniqueMenu&) = delete;
    UniqueMenu& operator=(const UniqueMenu&) = delete;

    UniqueMenu(UniqueMenu&& other) noexcept;
    UniqueMenu& operator=(UniqueMenu&& other) noexcept;

    [[nodiscard]] HMENU get() const noexcept
    {
        return menu_;
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return menu_ != nullptr;
    }

    HMENU release() noexcept;
    void reset(HMENU menu = nullptr) noexcept;

  private:
    HMENU menu_ = nullptr;
};

class UniqueIcon
{
  public:
    UniqueIcon() noexcept = default;
    explicit UniqueIcon(HICON icon) noexcept : icon_(icon) {}
    ~UniqueIcon();

    UniqueIcon(const UniqueIcon&) = delete;
    UniqueIcon& operator=(const UniqueIcon&) = delete;

    UniqueIcon(UniqueIcon&& other) noexcept;
    UniqueIcon& operator=(UniqueIcon&& other) noexcept;

    [[nodiscard]] HICON get() const noexcept
    {
        return icon_;
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return icon_ != nullptr;
    }

    HICON release() noexcept;
    void reset(HICON icon = nullptr) noexcept;

  private:
    HICON icon_ = nullptr;
};

[[nodiscard]] std::wstring win32_error_message(DWORD error);
[[nodiscard]] std::string to_utf8(std::wstring_view text);
[[nodiscard]] std::optional<std::wstring> to_wide(std::string_view text);
[[nodiscard]] std::wstring format_string(const wchar_t* format, ...);
[[nodiscard]] std::wstring load_string(HINSTANCE instance, UINT id);

[[nodiscard]] std::optional<std::filesystem::path> module_path();
[[nodiscard]] std::optional<std::filesystem::path> known_folder(REFKNOWNFOLDERID folder_id);
[[nodiscard]] std::wstring quote_path(const std::filesystem::path& path);

[[nodiscard]] bool read_file_bytes(const std::filesystem::path& path,
                                   std::vector<std::uint8_t>& data);
[[nodiscard]] bool write_file_bytes(const std::filesystem::path& path,
                                    const std::vector<std::uint8_t>& data);

} // namespace util
