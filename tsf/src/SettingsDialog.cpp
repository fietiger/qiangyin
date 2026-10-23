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
        CheckRadioButton(h, IDC_ENTER_CLEARS, IDC_ENTER_COMMIT,
                         s->enterClears ? IDC_ENTER_CLEARS : IDC_ENTER_COMMIT);
        HWND cb = GetDlgItem(h, IDC_CAND_COMBO);
        ComboBox_AddString(cb, L"5");
        ComboBox_AddString(cb, L"10");
        ComboBox_SelectString(cb, -1, s->candidateCount == 5 ? L"5" : L"10");
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
            s->enterClears   = IsDlgButtonChecked(h, IDC_ENTER_CLEARS) == BST_CHECKED;
            wchar_t buf[8] = {0};
            GetDlgItemTextW(h, IDC_CAND_COMBO, buf, 8);
            s->candidateCount = (wcscmp(buf, L"5") == 0) ? 5 : 10;
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
