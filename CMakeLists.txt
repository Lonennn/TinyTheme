cmake_minimum_required(VERSION 3.28)

project(TinyTheme VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)
configure_file(res/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)

add_executable(TinyTheme WIN32
    src/main.cpp
    src/app.cpp
    src/tray.cpp
    src/menu.cpp
    src/theme.cpp
    src/config.cpp
    src/toast.cpp
    src/log.cpp
    src/util.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc)

target_include_directories(TinyTheme PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR})

target_compile_definitions(TinyTheme PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX)

if (MSVC)
    target_compile_options(TinyTheme PRIVATE
        /W4 /WX /permissive- /utf-8 /EHsc /GR- /Brepro)

    target_compile_options(TinyTheme PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
        $<$<CONFIG:Release>:/GL>)

    target_compile_options(TinyTheme PRIVATE
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/RTC1>)

    target_link_options(TinyTheme PRIVATE
        /SUBSYSTEM:WINDOWS
        /INCREMENTAL:NO
        /MANIFEST:EMBED
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/res/app.manifest"
        /DYNAMICBASE
        /NXCOMPAT
        /HIGHENTROPYVA)

    target_link_options(TinyTheme PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>)

    set_property(TARGET TinyTheme PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(TinyTheme PRIVATE
    user32
    shell32
    advapi32
    gdi32
    dwmapi
    shlwapi
    ole32)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/app.rc
    PROPERTIES COMPILE_FLAGS
    "/c65001 /I\"${CMAKE_CURRENT_SOURCE_DIR}/src\" /I\"${CMAKE_CURRENT_BINARY_DIR}\"")

enable_testing()
add_subdirectory(tests)
