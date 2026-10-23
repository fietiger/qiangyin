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

void CandidateWindow::ShowAt(int x, int y, const std::vector<std::wstring>& items) {
    if (!Create()) return;
    items_ = items;
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

void CandidateWindow::Update(const std::vector<std::wstring>& items) {
    if (!hwnd_) return;
    items_ = items;
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
    int w = 60;
    for (const auto& s : items_) {
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, s.c_str(), (int)s.size(), &sz);
        if (sz.cx + 40 > w) w = sz.cx + 40;
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
    int y = 4;
    for (size_t i = 0; i < items_.size(); ++i) {
        wchar_t num[8];
        wsprintfW(num, L"%d. ", (int)i + 1);
        std::wstring line = num + items_[i];
        if (i == 0) {
            // 首选高亮
            RECT hl = {2, y - 2, width_ - 2, y + lineHeight_};
            FillRect(hdc, &hl, reinterpret_cast<HBRUSH>(COLOR_HIGHLIGHT + 1));
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }
        TextOutW(hdc, 8, y, line.c_str(), (int)line.size());
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
