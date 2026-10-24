// 候选窗口实现：极简自绘 popup
#include "CandidateWindow.h"

namespace qy {

namespace {
const wchar_t* kClassName = L"QiangYinCandidateWnd";
}

CandidateWindow::CandidateWindow() = default;

CandidateWindow::~CandidateWindow() { Destroy(); }

bool CandidateWindow::Create() {
    if (hwnd_) return true;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    if (!GetClassInfoW(wc.hInstance, kClassName, &wc))
        RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kClassName, L"", WS_POPUP | WS_BORDER,
        0, 0, 120, 60, nullptr, nullptr, wc.hInstance, this);
    if (!hwnd_) return false;

    font_ = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    return true;
}

void CandidateWindow::Destroy() {
    if (font_) { DeleteObject(font_); font_ = nullptr; }
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
}

void CandidateWindow::ShowAt(int x, int y, const std::vector<CandidateItem>& items,
                             int page, int pageCount) {
    if (!Create()) return;
    items_ = items;
    page_ = page;
    pageCount_ = pageCount;
    RecalcSize();
    // 防止超出屏幕右/下边缘
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int h = lineHeight_ * (int)items_.size() + 8;
    if (x + width_ > sw) x = sw - width_;
    if (y + h > sh) y = y - h - 24;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width_, h,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void CandidateWindow::Update(const std::vector<CandidateItem>& items,
                             int page, int pageCount) {
    if (!hwnd_) return;
    items_ = items;
    page_ = page;
    pageCount_ = pageCount;
    RecalcSize();
    RECT rc; GetWindowRect(hwnd_, &rc);
    SetWindowPos(hwnd_, nullptr, 0, 0, width_,
                 lineHeight_ * (int)items_.size() + 8,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void CandidateWindow::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
    items_.clear();
}

bool CandidateWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void CandidateWindow::RecalcSize() {
    HDC hdc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(hdc, font_);
    SIZE szNum = {}, szPage = {};
    GetTextExtentPoint32W(hdc, L"9. ", 3, &szNum);
    numW_ = szNum.cx;
    wchar_t page[16] = {0};
    wsprintfW(page, L"%d/%d", page_, pageCount_);
    GetTextExtentPoint32W(hdc, page, (int)wcslen(page), &szPage);
    pageW_ = szPage.cx + 10; // 右侧页码列（含间隙）
    int w = 60;
    for (const auto& s : items_) {
        SIZE sz = {}, szHint = {};
        GetTextExtentPoint32W(hdc, s.text.c_str(), (int)s.text.size(), &sz);
        if (!s.hint.empty())
            GetTextExtentPoint32W(hdc, s.hint.c_str(), (int)s.hint.size(), &szHint);
        // 8 左边距 + 序号列 + 文本 + (6 间隙 + 提示) + 页码列 + 12 右边距
        int need = 8 + numW_ + sz.cx + (szHint.cx ? 6 + szHint.cx : 0) + pageW_ + 12;
        if (need > w) w = need;
        lineHeight_ = sz.cy + 8;
    }
    SelectObject(hdc, old);
    ReleaseDC(hwnd_, hdc);
    width_ = w;
}

void CandidateWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    HGDIOBJ old = SelectObject(hdc, font_);
    SetBkMode(hdc, TRANSPARENT);
    RECT rc; GetClientRect(hwnd_, &rc);
    FillRect(hdc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    // 右上角页码 "n/m"（首选高亮行上，灰色弱化）
    if (pageCount_ > 1) {
        wchar_t page[16] = {0};
        wsprintfW(page, L"%d/%d", page_, pageCount_);
        int len = (int)wcslen(page);
        SIZE szPage = {};
        GetTextExtentPoint32W(hdc, page, len, &szPage);
        SetTextColor(hdc, RGB(0x90, 0x90, 0x90));
        TextOutW(hdc, width_ - 8 - szPage.cx, 4, page, len);
    }
    int y = 4;
    for (size_t i = 0; i < items_.size(); ++i) {
        wchar_t num[8];
        wsprintfW(num, L"%d. ", (int)i + 1);
        bool first = (i == 0);
        if (first) {
            // 首选高亮
            RECT hl = {2, y - 2, width_ - 2, y + lineHeight_};
            FillRect(hdc, &hl, reinterpret_cast<HBRUSH>(COLOR_HIGHLIGHT + 1));
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }
        TextOutW(hdc, 8, y, num, (int)wcslen(num));
        TextOutW(hdc, 8 + numW_, y, items_[i].text.c_str(), (int)items_[i].text.size());
        // 编码提示：补全该候选还差的编码，灰色弱化
        if (!items_[i].hint.empty()) {
            SIZE szText = {};
            GetTextExtentPoint32W(hdc, items_[i].text.c_str(),
                                  (int)items_[i].text.size(), &szText);
            SetTextColor(hdc, first ? RGB(0xC5, 0xDA, 0xF5) : RGB(0x90, 0x90, 0x90));
            TextOutW(hdc, 8 + numW_ + szText.cx + 6, y,
                     items_[i].hint.c_str(), (int)items_[i].hint.size());
        }
        y += lineHeight_;
    }
    SelectObject(hdc, old);
    EndPaint(hwnd_, &ps);
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CandidateWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<CandidateWindow*>(
            reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    switch (msg) {
    case WM_PAINT:
        if (self) self->OnPaint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace qy
