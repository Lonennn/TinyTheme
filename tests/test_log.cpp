#include "log.h"
#include "util.h"

#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
// 不用 %TEMP%：该环境里 TEMP 是 8.3 短路径，卷上可能未启用短名而无法解析。
// 统一落在测试 exe 同目录的 test-scratch，构建目录本身一定可写。
std::filesystem::path scratch_dir()
{
    static const std::filesystem::path path = []()
    {
        const auto module = util::module_path();
        const auto base = module ? module->parent_path() : std::filesystem::current_path();
        return base / L"test-scratch";
    }();
    std::error_code error;
    std::filesystem::create_directories(path, error);
    return path;
}
} // namespace

TEST_CASE("logger writes a line to the configured file")
{
    const auto file = scratch_dir() / L"log-basic.log";
    std::error_code error;
    std::filesystem::remove(file, error);

    logger::init(file);
    logger::info(L"hello");
    logger::warn(L"careful");
    logger::error(L"boom");

    REQUIRE(std::filesystem::exists(file));
    std::ifstream stream(file);
    std::string contents((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());

    CHECK(contents.find("[info] hello") != std::string::npos);
    CHECK(contents.find("[warn] careful") != std::string::npos);
    CHECK(contents.find("[error] boom") != std::string::npos);
}

TEST_CASE("logger falls back to the module directory when the primary path is not writable")
{
    // 不用 C:\Windows\System32 这类系统目录来构造“写不进去”：托管 CI runner 以管理员身份
    // 运行，那些目录照样能写，用例就退化成在测“跑测试的人是不是管理员”。
    // 父路径是普通文件时，create_directories 与 CreateFile 在任何身份下都必然失败。
    const auto blocker = scratch_dir() / L"not-a-directory";
    {
        std::ofstream file(blocker, std::ios::binary);
        REQUIRE(file.good());
        file << "blocker";
    }
    const auto primary = blocker / L"app.log";

    const auto module = util::module_path();
    REQUIRE(module.has_value());
    const auto fallback = module->parent_path() / L"logs" / L"app.log";
    std::error_code error;
    std::filesystem::remove(fallback, error);

    logger::init(primary);
    logger::info(L"fallback-probe");

    CHECK(std::filesystem::exists(fallback));
}

TEST_CASE("logger rotates when the file grows past the limit")
{
    const auto file = scratch_dir() / L"log-rotate.log";
    const auto history = scratch_dir() / L"log-rotate.1.log";
    std::error_code error;
    std::filesystem::remove(file, error);
    std::filesystem::remove(history, error);

    logger::init(file);

    const std::string padding(60 * 1024, 'x');
    const std::wstring wide(padding.begin(), padding.end());
    for (int index = 0; index < 6; ++index)
    {
        logger::info(wide);
    }

    CHECK(std::filesystem::exists(file));
    CHECK(std::filesystem::exists(history));
    CHECK(std::filesystem::file_size(file) <= 256 * 1024);
}
