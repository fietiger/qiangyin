#pragma once
// 极简文件日志：写到 %TEMP%\qiangyin_ime.log，用于远程诊断
// 发布时可保留（开销极小），或定义 QY_NO_LOG 关闭
#include <windows.h>
#include <string>

namespace qy {
namespace log {

inline std::wstring LogPath() {
    wchar_t tmp[MAX_PATH] = {0};
    DWORD n = GetTempPathW(MAX_PATH, tmp);
    std::wstring p = (n > 0 && n < MAX_PATH) ? tmp : L"C:\\Windows\\Temp\\";
    if (!p.empty() && p.back() != L'\\') p += L'\\';
    return p + L"qiangyin_ime.log";
}

inline void Write(const wchar_t* msg) {
    // 追加写入，失败就静默跳过（绝不能因为日志崩了输入法）
    HANDLE h = CreateFileW(LogPath().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t line[512];
    int len = wsprintfW(line, L"[%02d:%02d:%02d.%03d pid=%lu] %s\r\n",
                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                        GetCurrentProcessId(), msg);
    DWORD written = 0;
    if (len > 0) WriteFile(h, line, (DWORD)len * sizeof(wchar_t), &written, nullptr);
    CloseHandle(h);
}

inline void WriteA(const char* msg) {
    wchar_t w[256] = {0};
    MultiByteToWideChar(CP_UTF8, 0, msg, -1, w, 256);
    Write(w);
}

} // namespace log
} // namespace qy

#define QYLOG(msg) qy::log::Write(L"" msg)
