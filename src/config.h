#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace config
{

enum class Mode
{
    Light,
    Dark,
};

struct Config
{
    Mode mode = Mode::Light;
    bool include_system_theme = true;
    bool autostart = true;
    bool show_toast = true;
    bool first_run = true;
    std::wstring language = L"auto";
};

// 纯解析入口：测试用。text 为标准 INI 文本，返回值只带解析结果与告警。
struct ParseResult
{
    Config config;
    bool had_invalid_value = false;
};

[[nodiscard]] ParseResult parse(std::wstring_view text);

// 序列化为带注释头的 INI 文本（UTF-8 BOM 在写文件时追加）。
[[nodiscard]] std::wstring serialize(const Config& config);

// 把文本编码转换为 UTF-16；识别 UTF-8 BOM 与无 BOM UTF-8。
[[nodiscard]] std::optional<std::wstring> decode(std::string_view bytes);

// UTF-16 -> 带 BOM 的 UTF-8。
[[nodiscard]] std::string encode(std::wstring_view text);

[[nodiscard]] std::filesystem::path directory();
[[nodiscard]] std::filesystem::path file_path();
[[nodiscard]] bool is_portable();

// 读取配置。文件不存在、损坏或不可读时返回默认值；损坏文件会被备份。
[[nodiscard]] Config load();

// 原子保存：写临时文件后替换。失败返回 false，调用方决定是否提示。
[[nodiscard]] bool save(const Config& config);

[[nodiscard]] bool is_autostart_enabled();
[[nodiscard]] bool set_autostart(bool enabled);

} // namespace config
