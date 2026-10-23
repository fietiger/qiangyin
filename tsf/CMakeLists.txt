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
    src/dllmain.cpp
    src/qiangyin_ime.def
    qiangyin.rc
)

add_library(qiangyin_ime SHARED ${SRCS})

target_include_directories(qiangyin_ime PRIVATE src)

# 输出名：qiangyin_ime.dll（不带 lib 前缀）
set_target_properties(qiangyin_ime PROPERTIES
    OUTPUT_NAME "qiangyin_ime"
    PREFIX ""
    SUFFIX ".dll"
)

target_link_libraries(qiangyin_ime PRIVATE ole32 oleaut32 uuid gdi32 advapi32)

# MSVC：关闭不必要的警告，UTF-8 源码
if(MSVC)
    target_compile_options(qiangyin_ime PRIVATE /W3 /utf-8)
endif()
