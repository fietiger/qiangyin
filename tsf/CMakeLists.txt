cmake_minimum_required(VERSION 3.20)
project(qiangyin_ime CXX)

# 强音输入法 TSF 外壳：Windows MSVC 构建
if(NOT WIN32)
    message(WARNING "TSF 外壳只能在 Windows 上构建，此处仅做配置检查")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRCS
    src/Guids.cpp
    src/Engine.cpp
    src/KeyHandler.cpp
    src/TextService.cpp
    src/CandidateWindow.cpp
    src/LangBar.cpp
    src/Settings.cpp
    src/SettingsDialog.cpp
    src/AddWordDialog.cpp
    src/AboutDialog.cpp
    src/dllmain.cpp
    src/qiangyin_ime.def
    qiangyin.rc
)

# 版本号：CI 通过 -DQY_VERSION=vX.Y.Z 传入；本地未传时自动取 git describe
if(NOT DEFINED QY_VERSION OR QY_VERSION STREQUAL "")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --always
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE QY_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(NOT QY_VERSION)
        set(QY_VERSION "dev")
    endif()
endif()
message(STATUS "QiangYin IME version: ${QY_VERSION}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qy_version.h"
     "#pragma once\n#define QY_VERSION_WSTR L\"${QY_VERSION}\"\n")

add_library(qiangyin_ime SHARED ${SRCS})

target_include_directories(qiangyin_ime PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}")

# 输出名：qiangyin_ime.dll（不带 lib 前缀）
set_target_properties(qiangyin_ime PROPERTIES
    OUTPUT_NAME "qiangyin_ime"
    PREFIX ""
    SUFFIX ".dll"
)

target_link_libraries(qiangyin_ime PRIVATE ole32 oleaut32 uuid gdi32 advapi32 shell32)

# 整个代码库都用显式 W 后缀 API；定义 UNICODE 让宏（SNDMSG/DefWindowProc 等）
# 也走 W 版本。不定义的话 ComboBox_AddString 会展开成 SendMessageA，
# 把 L"10" 按 ANSI 读到第一个 0 字节截断成 "1"（设置对话框实测的坑）。
target_compile_definitions(qiangyin_ime PRIVATE UNICODE _UNICODE)

# MSVC：关闭不必要的警告，UTF-8 源码
if(MSVC)
    target_compile_options(qiangyin_ime PRIVATE /W3 /utf-8)
endif()
