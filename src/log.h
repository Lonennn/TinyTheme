#pragma once

#include <filesystem>
#include <string_view>

namespace logger
{

enum class Level
{
    Info,
    Warn,
    Error,
};

void init(const std::filesystem::path& path);
void write(Level level, std::wstring_view message);
void info(std::wstring_view message);
void warn(std::wstring_view message);
void error(std::wstring_view message);

} // namespace logger
