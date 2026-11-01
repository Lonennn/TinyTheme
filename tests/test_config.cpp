#include "config.h"

#include <doctest.h>

#include <string>

using config::Mode;

TEST_CASE("empty file yields defaults")
{
    const auto result = config::parse(L"");
    CHECK(result.config.mode == Mode::Light);
    CHECK(result.config.include_system_theme);
    CHECK(result.config.autostart);
    CHECK(result.config.show_toast);
    CHECK(result.config.first_run);
    CHECK_FALSE(result.had_invalid_value);
}

TEST_CASE("invalid mode falls back to light")
{
    const auto result = config::parse(L"[general]\nmode=purple\n");
    CHECK(result.config.mode == Mode::Light);
    CHECK(result.had_invalid_value);
}

TEST_CASE("auto mode is not accepted in v0.1")
{
    const auto result = config::parse(L"[general]\nmode=auto\n");
    CHECK(result.config.mode == Mode::Light);
    CHECK(result.had_invalid_value);
}

TEST_CASE("valid keys are parsed")
{
    const auto result = config::parse(L"; comment\n"
                                      L"[general]\n"
                                      L"mode=dark\n"
                                      L"include_system_theme=0\n"
                                      L"autostart=0\n"
                                      L"show_toast=0\n"
                                      L"\n"
                                      L"[meta]\n"
                                      L"first_run=0\n");
    CHECK(result.config.mode == Mode::Dark);
    CHECK_FALSE(result.config.include_system_theme);
    CHECK_FALSE(result.config.autostart);
    CHECK_FALSE(result.config.show_toast);
    CHECK_FALSE(result.config.first_run);
    CHECK_FALSE(result.had_invalid_value);
}

TEST_CASE("round trip through serialize")
{
    config::Config original;
    original.mode = Mode::Dark;
    original.include_system_theme = false;
    original.autostart = false;
    original.show_toast = true;
    original.first_run = false;

    const std::wstring text = config::serialize(original);
    const auto parsed = config::parse(text);
    CHECK(parsed.config.mode == Mode::Dark);
    CHECK_FALSE(parsed.config.include_system_theme);
    CHECK_FALSE(parsed.config.autostart);
    CHECK(parsed.config.show_toast);
    CHECK_FALSE(parsed.config.first_run);
}

TEST_CASE("utf8 bom is decoded")
{
    std::string bytes;
    bytes.push_back(static_cast<char>(0xEF));
    bytes.push_back(static_cast<char>(0xBB));
    bytes.push_back(static_cast<char>(0xBF));
    bytes += "[general]\nmode=dark\n";

    const auto decoded = config::decode(bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->find(L"mode=dark") != std::wstring::npos);
}

TEST_CASE("plain utf8 without bom is decoded")
{
    const auto decoded = config::decode("[general]\nmode=dark\n");
    REQUIRE(decoded.has_value());
    CHECK(decoded->find(L"mode=dark") != std::wstring::npos);
}
