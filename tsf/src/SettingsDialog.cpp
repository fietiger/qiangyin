#include "SettingsDialog.h"
#include "AboutDialog.h"
#include "resids.h"
#include <windowsx.h> // ComboBox_xxx / Button 宏
#include <string.h>   // wcscmp

namespace qy {

namespace {

struct SettingsDlgState {
    Settings* s = nullptr;
    size_t dictCount = 0;
};

INT_PTR CALLBACK SettingsDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        SettingsDlgState* st = reinterpret_cast<SettingsDlgState*>(lp);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        Settings* s = st->s;
        CheckDlgButton(h, IDC_SHOW_HINT,      s->showHint ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_AUTO_COMMIT,    s->autoCommitFull ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_CHINESE_PUNCT,  s->chinesePunct ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_ALWAYS_CHINESE, s->alwaysChinese ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_FULL_SHAPE,     s->fullShape ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_ENG_HALFWIDTH,  s->engAutoHalfWidth ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_ENG_ENPUNCT,    s->engAutoEnPunct ? BST_CHECKED : BST_UNCHECKED);
        // Enter 行为三选一（组内 WS_GROUP 在 IDC_ENTER_CLEARS）
        CheckRadioButton(h, IDC_ENTER_CLEARS, IDC_ENTER_CODE,
                         s->enterMode == 2 ? IDC_ENTER_CODE :
                         s->enterMode == 1 ? IDC_ENTER_COMMIT : IDC_ENTER_CLEARS);
        // 切换中/英时三选一
        CheckRadioButton(h, IDC_SW_CLEARS, IDC_SW_CODE,
                         s->switchMode == 2 ? IDC_SW_CODE :
                         s->switchMode == 1 ? IDC_SW_COMMIT : IDC_SW_CLEARS);
        // 候选窗排列方向二选一
        CheckRadioButton(h, IDC_CAND_VERTICAL, IDC_CAND_HORIZONTAL,
                         s->horizontal ? IDC_CAND_HORIZONTAL : IDC_CAND_VERTICAL);
        HWND cb = GetDlgItem(h, IDC_CAND_COMBO);
        for (int n = 5; n <= 10; ++n) {
            wchar_t numStr[4] = {0};
            wsprintfW(numStr, L"%d", n);
            ComboBox_AddString(cb, numStr);
        }
        wchar_t cur[4] = {0};
        wsprintfW(cur, L"%d", s->candidateCount);
        ComboBox_SelectString(cb, -1, cur);
        return TRUE; // 焦点给默认按钮
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_OPEN_ABOUT: {
            SettingsDlgState* st = reinterpret_cast<SettingsDlgState*>(
                GetWindowLongPtrW(h, GWLP_USERDATA));
            ShowAboutDialog(h, st ? st->dictCount : 0);
            return TRUE;
        }
        case IDOK: {
            SettingsDlgState* st = reinterpret_cast<SettingsDlgState*>(
                GetWindowLongPtrW(h, GWLP_USERDATA));
            Settings* s = st ? st->s : nullptr;
            if (!s) { EndDialog(h, IDCANCEL); return TRUE; }
            s->showHint      = IsDlgButtonChecked(h, IDC_SHOW_HINT) == BST_CHECKED;
            s->autoCommitFull= IsDlgButtonChecked(h, IDC_AUTO_COMMIT) == BST_CHECKED;
            s->chinesePunct  = IsDlgButtonChecked(h, IDC_CHINESE_PUNCT) == BST_CHECKED;
            s->alwaysChinese = IsDlgButtonChecked(h, IDC_ALWAYS_CHINESE) == BST_CHECKED;
            s->fullShape     = IsDlgButtonChecked(h, IDC_FULL_SHAPE) == BST_CHECKED;
            s->engAutoHalfWidth = IsDlgButtonChecked(h, IDC_ENG_HALFWIDTH) == BST_CHECKED;
            s->engAutoEnPunct   = IsDlgButtonChecked(h, IDC_ENG_ENPUNCT) == BST_CHECKED;
            // 切换中/英时三选一
            if (IsDlgButtonChecked(h, IDC_SW_CODE) == BST_CHECKED)            s->switchMode = 2;
            else if (IsDlgButtonChecked(h, IDC_SW_COMMIT) == BST_CHECKED)     s->switchMode = 1;
            else                                                               s->switchMode = 0;
            if (IsDlgButtonChecked(h, IDC_ENTER_CODE) == BST_CHECKED)          s->enterMode = 2;
            else if (IsDlgButtonChecked(h, IDC_ENTER_COMMIT) == BST_CHECKED)   s->enterMode = 1;
            else                                                               s->enterMode = 0;
            s->horizontal = IsDlgButtonChecked(h, IDC_CAND_HORIZONTAL) == BST_CHECKED;
            wchar_t buf[8] = {0};
            GetDlgItemTextW(h, IDC_CAND_COMBO, buf, 8);
            int n = _wtoi(buf);
            if (n < 5) n = 5;
            if (n > 10) n = 10;
            s->candidateCount = n;
            s->Save();
            EndDialog(h, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(h, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

} // namespace

bool ShowSettingsDialog(HWND owner, Settings& s, size_t dictCount) {
    // 从本函数地址反查宿主 DLL，拿资源模板
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&SettingsDlgProc), &self);
    if (!self) return false;
    SettingsDlgState st{ &s, dictCount };
    return DialogBoxParamW(self, MAKEINTRESOURCEW(IDD_SETTINGS), owner,
                           SettingsDlgProc, reinterpret_cast<LPARAM>(&st)) == IDOK;
}

} // namespace qy
