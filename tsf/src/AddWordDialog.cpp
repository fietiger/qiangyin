// 在线加词对话框实现
#include "AddWordDialog.h"
#include "resids.h"
#include "Utf.h"
#include "QyLog.h"
#include <map>
#include <string.h>

namespace qy {

// ---- 编码自动计算 ----

// 从系统码表反查每个字的全码（该字最长的那条单字编码）
static std::map<wchar_t, std::string> LoadCharCodes(const std::wstring& mbPath,
                                                    const std::wstring& word) {
    std::map<wchar_t, std::string> out;
    HANDLE h = CreateFileW(mbPath.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    std::string data;
    char chunk[8192];
    DWORD rd = 0;
    while (ReadFile(h, chunk, sizeof(chunk), &rd, nullptr) && rd > 0)
        data.append(chunk, rd);
    CloseHandle(h);

    size_t pos = 0;
    while (pos < data.size()) {
        size_t eol = data.find('\n', pos);
        if (eol == std::string::npos) eol = data.size();
        size_t sp = data.find(' ', pos);
        if (sp != std::string::npos && sp < eol && sp > pos) {
            size_t textBegin = sp + 1;
            size_t textLen = eol > textBegin ? eol - textBegin : 0;
            if (textLen > 0 && data[textBegin + textLen - 1] == '\r') --textLen;
            if (textLen == 3) { // 单字条目：文本恰为一个汉字（UTF-8 3 字节）
                std::string code = data.substr(pos, sp - pos);
                bool ok = !code.empty();
                for (char c : code)
                    if (c < 'a' || c > 'z') { ok = false; break; }
                if (ok) {
                    std::wstring ch = Utf8ToWide(data.substr(textBegin, 3));
                    if (ch.size() == 1 && word.find(ch[0]) != std::wstring::npos) {
                        // 同字多编码时的取舍：i 开头是“不知读音”的救急码
                        // （拼音声母无 i，如 了=lm/救急 im），正常声母码优先；
                        // 同优先级取更长（全码），只有救急码时才落到救急码。
                        std::string& best = out[ch[0]];
                        bool bestIsI = !best.empty() && best[0] == 'i';
                        bool codeIsI = code[0] == 'i';
                        if (best.empty() ||
                            (bestIsI && !codeIsI) ||
                            (bestIsI == codeIsI && code.size() > best.size())) {
                            best = code;
                        }
                    }
                }
            }
        }
        pos = eol + 1;
    }
    return out;
}

std::wstring ComputeWordCode(const std::wstring& mbPath, const std::wstring& word) {
    if (word.empty() || mbPath.empty()) return std::wstring();
    auto codes = LoadCharCodes(mbPath, word);
    auto head = [&codes, &word](size_t i, size_t n) -> std::string {
        auto it = codes.find(word[i]);
        if (it == codes.end()) return std::string();
        return it->second.substr(0, n);
    };
    std::string code;
    size_t n = word.size();
    if (n == 1)      code = head(0, 4);                          // 单字：全码
    else if (n == 2) code = head(0, 2) + head(1, 2);             // 二字：每字前两码
    else if (n == 3) code = head(0, 2) + head(1, 1) + head(2, 1); // 三字：首字前两码+后两字首码
    else if (n == 4) code = head(0, 1) + head(1, 1) + head(2, 1) + head(3, 1); // 四字：各字首码
    else             code = head(0, 1) + head(1, 1) + head(2, 1) + head(n - 1, 1); // 多字：前三+末字首码
    return Utf8ToWide(code);
}

// ---- 对话框 ----

namespace {

const UINT kWordLenChanged = WM_APP + 1; // wParam=VK_UP/VK_DOWN

WNDPROC g_oldWordEditProc = nullptr;

// 词条编辑框子类：↑/↓ 交给对话框调词长（原版同款交互）
LRESULT CALLBACK WordEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && (wp == VK_UP || wp == VK_DOWN)) {
        SendMessageW(GetParent(h), kWordLenChanged, wp, 0);
        return 0;
    }
    return CallWindowProcW(g_oldWordEditProc, h, msg, wp, lp);
}

struct AddWordState {
    AddWordContext* ctx;
    size_t wordLen;              // ↑/↓ 调出的词长（recent 尾部 N 字）
    std::wstring* outWord;
    std::wstring* outCode;
};

std::wstring RecentWord(const AddWordContext& ctx, size_t len) {
    const std::wstring& r = ctx.recent;
    size_t n = len < r.size() ? len : r.size();
    return r.substr(r.size() - n);
}

void Refill(HWND h, AddWordState& st, const std::wstring& word) {
    SetDlgItemTextW(h, IDC_AW_WORD, word.c_str());
    SetDlgItemTextW(h, IDC_AW_CODE,
                    ComputeWordCode(st.ctx->mbPath, word).c_str());
}

std::wstring GetClipboardText() {
    std::wstring out;
    if (!OpenClipboard(nullptr)) return out;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h));
        if (p) {
            out = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    // 去首尾空白，只取第一行
    size_t b = out.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return std::wstring();
    size_t e = out.find_first_of(L"\r\n", b);
    return out.substr(b, e == std::wstring::npos ? std::wstring::npos : e - b);
}

INT_PTR CALLBACK AddWordDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    AddWordState* st = reinterpret_cast<AddWordState*>(
        GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (msg) {
    case WM_INITDIALOG: {
        st = reinterpret_cast<AddWordState*>(lp);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        HWND edit = GetDlgItem(h, IDC_AW_WORD);
        g_oldWordEditProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(edit, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(WordEditProc)));
        st->wordLen = st->ctx->recent.size() >= 2 ? 2 : st->ctx->recent.size();
        Refill(h, *st, RecentWord(*st->ctx, st->wordLen));
        return TRUE;
    }
    case kWordLenChanged: {
        const std::wstring& r = st->ctx->recent;
        size_t maxLen = r.size() < 8 ? r.size() : 8;
        if (wp == VK_UP && st->wordLen < maxLen) ++st->wordLen;
        if (wp == VK_DOWN && st->wordLen > 1) --st->wordLen;
        Refill(h, *st, RecentWord(*st->ctx, st->wordLen));
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_AW_CLIP: {
            if (IsDlgButtonChecked(h, IDC_AW_CLIP) == BST_CHECKED) {
                std::wstring clip = GetClipboardText();
                if (!clip.empty())
                    SetDlgItemTextW(h, IDC_AW_WORD, clip.c_str());
                // 按当前词条框内容重算编码
                wchar_t wbuf[128] = {0};
                GetDlgItemTextW(h, IDC_AW_WORD, wbuf, 128);
                SetDlgItemTextW(h, IDC_AW_CODE,
                                ComputeWordCode(st->ctx->mbPath, wbuf).c_str());
            } else {
                // 取消剪贴板：回到最近输入模式
                Refill(h, *st, RecentWord(*st->ctx, st->wordLen));
            }
            return TRUE;
        }
        case IDOK: {
            wchar_t wbuf[128] = {0}, cbuf[32] = {0};
            GetDlgItemTextW(h, IDC_AW_WORD, wbuf, 128);
            GetDlgItemTextW(h, IDC_AW_CODE, cbuf, 32);
            std::wstring code;
            for (wchar_t c : std::wstring(cbuf)) {
                if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
                if (c < L'a' || c > L'z') { code.clear(); break; }
                code += c;
            }
            std::wstring word(wbuf);
            if (word.empty() || code.empty() || code.size() > 10) {
                MessageBoxW(h, L"词条非空；编码为 1~10 个字母（a-z）。",
                            L"在线加词", MB_ICONINFORMATION | MB_OK);
                return TRUE;
            }
            *st->outWord = word;
            *st->outCode = code;
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

bool ShowAddWordDialog(HWND owner, AddWordContext& ctx,
                       std::wstring& outWord, std::wstring& outCode) {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ComputeWordCode), &self);
    if (!self) { QYLOG("addword dlg: no module"); return false; }
    AddWordState st;
    st.ctx = &ctx;
    st.wordLen = 0;
    st.outWord = &outWord;
    st.outCode = &outCode;
    INT_PTR r = DialogBoxParamW(self, MAKEINTRESOURCEW(IDD_ADDWORD), owner,
                                AddWordDlgProc, reinterpret_cast<LPARAM>(&st));
    { wchar_t b[64]; wsprintfW(b, L"addword dlg ret=%d", (int)(INT_PTR)r); qy::log::Write(b); }
    return r == IDOK;
}

} // namespace qy
