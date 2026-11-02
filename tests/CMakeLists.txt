add_executable(tinytheme_tests main.cpp test_config.cpp test_log.cpp)

target_sources(tinytheme_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/util.cpp)

target_include_directories(tinytheme_tests SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest)

target_include_directories(tinytheme_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_compile_definitions(tinytheme_tests PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX)

if (MSVC)
    target_compile_options(tinytheme_tests PRIVATE
        /W4 /WX /permissive- /utf-8 /EHsc /GR- /sdl)

    target_link_options(tinytheme_tests PRIVATE /WX)

    target_compile_options(tinytheme_tests PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/RTC1>)

    set_property(TARGET tinytheme_tests PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(tinytheme_tests PRIVATE
    advapi32
    ole32
    shell32
    user32)

add_test(NAME tinytheme_tests COMMAND tinytheme_tests)
