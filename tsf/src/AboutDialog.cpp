#include "AboutDialog.h"
#include "Engine.h"
#include "resids.h"
#include <shellapi.h>

#if __has_include("qy_version.h")
#include "qy_version.h"
#endif

#ifndef QY_VERSION_WSTR
#define QY_VERSION_WSTR L"dev"
#endif

namespace qy {

namespace {

struct AboutState {
    size_t dictCount = 0;
    HFONT titleFont = nullptr;
};

const wchar_t* ArchLabel() {
#if defined(_M_ARM64) || defined(__aarch64__)
    return L"ARM64";
#elif defined(_WIN64) || defined(__x86_64__)
    return L"64 位 (x64)";
#else
    return L"32 位 (x86)";
#endif
}

INT_PTR CALLBACK AboutDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    AboutState* st = reinterpret_cast<AboutState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (msg) {
    case WM_INITDIALOG: {
        st = reinterpret_cast<AboutState*>(lp);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

        // 标题加粗放大一号
        HFONT baseFont = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0));
        LOGFONTW lf = {};
        if (baseFont && GetObjectW(baseFont, sizeof(lf), &lf) == sizeof(lf)) {
            lf.lfHeight = (lf.lfHeight < 0) ? (lf.lfHeight * 5 / 4) : 16;
            lf.lfWeight = FW_BOLD;
            st->titleFont = CreateFontIndirectW(&lf);
            if (st->titleFont) {
                SendDlgItemMessageW(h, IDC_ABOUT_TITLE, WM_SETFONT,
                                    reinterpret_cast<WPARAM>(st->titleFont), TRUE);
            }
        }

        wchar_t verLine[160] = {0};
        wsprintfW(verLine, L"版本：%s  ·  %s  ·  %S",
                  QY_VERSION_WSTR, ArchLabel(), __DATE__);
        SetDlgItemTextW(h, IDC_ABOUT_VERSION, verLine);

        std::wstring userPath = Engine::UserTablePath();
        if (userPath.empty()) userPath = L"%APPDATA%\\QiangYin\\usercode.txt";

        wchar_t engLine[512] = {0};
        if (st && st->dictCount > 0) {
            wsprintfW(engLine, L"词库状态：已加载 %lu 条词条\n用户词库：%s",
                      static_cast<unsigned long>(st->dictCount), userPath.c_str());
        } else {
            wsprintfW(engLine, L"词库状态：就绪（首次输入时自动载入）\n用户词库：%s",
                      userPath.c_str());
        }
        SetDlgItemTextW(h, IDC_ABOUT_ENGINE, engLine);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_ABOUT_REPO:
            ShellExecuteW(h, L"open", L"https://github.com/fietiger/qiangyin",
                          nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        case IDOK:
        case IDCANCEL:
            EndDialog(h, LOWORD(wp));
            return TRUE;
        }
        break;
    case WM_DESTROY:
        if (st && st->titleFont) {
            DeleteObject(st->titleFont);
            st->titleFont = nullptr;
        }
        break;
    }
    return FALSE;
}

} // namespace

void ShowAboutDialog(HWND owner, size_t dictCount) {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&AboutDlgProc), &self);
    if (!self) return;
    AboutState st;
    st.dictCount = dictCount;
    DialogBoxParamW(self, MAKEINTRESOURCEW(IDD_ABOUT), owner,
                    AboutDlgProc, reinterpret_cast<LPARAM>(&st));
}

} // namespace qy
