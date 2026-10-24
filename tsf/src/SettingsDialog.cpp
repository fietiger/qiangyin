#include "SettingsDialog.h"
#include "resids.h"
#include <windowsx.h> // ComboBox_xxx / Button 宏
#include <string.h>   // wcscmp

namespace qy {

namespace {

INT_PTR CALLBACK SettingsDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        Settings* s = reinterpret_cast<Settings*>(lp);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        CheckDlgButton(h, IDC_SHOW_HINT,      s->showHint ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_AUTO_COMMIT,    s->autoCommitFull ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_CHINESE_PUNCT,  s->chinesePunct ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_ALWAYS_CHINESE, s->alwaysChinese ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_FULL_SHAPE,     s->fullShape ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_ENG_HALFWIDTH,  s->engAutoHalfWidth ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_ENG_ENPUNCT,    s->engAutoEnPunct ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_SWITCH_CLEAR,   s->switchClears ? BST_CHECKED : BST_UNCHECKED);
        // Enter 行为三选一（组内 WS_GROUP 在 IDC_ENTER_CLEARS）
        CheckRadioButton(h, IDC_ENTER_CLEARS, IDC_ENTER_CODE,
                         s->enterMode == 2 ? IDC_ENTER_CODE :
                         s->enterMode == 1 ? IDC_ENTER_COMMIT : IDC_ENTER_CLEARS);
        HWND cb = GetDlgItem(h, IDC_CAND_COMBO);
        for (int n = 5; n <= 10; ++n) {
            wchar_t s[4] = {0};
            wsprintfW(s, L"%d", n);
            ComboBox_AddString(cb, s);
        }
        wchar_t cur[4] = {0};
        wsprintfW(cur, L"%d", s->candidateCount);
        ComboBox_SelectString(cb, -1, cur);
        return TRUE; // 焦点给默认按钮
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK: {
            Settings* s = reinterpret_cast<Settings*>(
                GetWindowLongPtrW(h, GWLP_USERDATA));
            if (!s) { EndDialog(h, IDCANCEL); return TRUE; }
            s->showHint      = IsDlgButtonChecked(h, IDC_SHOW_HINT) == BST_CHECKED;
            s->autoCommitFull= IsDlgButtonChecked(h, IDC_AUTO_COMMIT) == BST_CHECKED;
            s->chinesePunct  = IsDlgButtonChecked(h, IDC_CHINESE_PUNCT) == BST_CHECKED;
            s->alwaysChinese = IsDlgButtonChecked(h, IDC_ALWAYS_CHINESE) == BST_CHECKED;
            s->fullShape     = IsDlgButtonChecked(h, IDC_FULL_SHAPE) == BST_CHECKED;
            s->engAutoHalfWidth = IsDlgButtonChecked(h, IDC_ENG_HALFWIDTH) == BST_CHECKED;
            s->engAutoEnPunct   = IsDlgButtonChecked(h, IDC_ENG_ENPUNCT) == BST_CHECKED;
            s->switchClears     = IsDlgButtonChecked(h, IDC_SWITCH_CLEAR) == BST_CHECKED;
            if (IsDlgButtonChecked(h, IDC_ENTER_CODE) == BST_CHECKED)          s->enterMode = 2;
            else if (IsDlgButtonChecked(h, IDC_ENTER_COMMIT) == BST_CHECKED)   s->enterMode = 1;
            else                                                               s->enterMode = 0;
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

bool ShowSettingsDialog(HWND owner, Settings& s) {
    // 从本函数地址反查宿主 DLL，拿资源模板
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&SettingsDlgProc), &self);
    if (!self) return false;
    return DialogBoxParamW(self, MAKEINTRESOURCEW(IDD_SETTINGS), owner,
                           SettingsDlgProc, reinterpret_cast<LPARAM>(&s)) == IDOK;
}

} // namespace qy
