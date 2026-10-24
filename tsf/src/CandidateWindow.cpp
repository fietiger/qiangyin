// 候选窗口实现：极简自绘 popup
#include "CandidateWindow.h"

namespace qy {

namespace {
const wchar_t* kClassName = L"QiangYinCandidateWnd";

// 系统“应用深色模式”：HKCU\...\Themes\Personalize\AppsUseLightTheme（0=深色）
bool IsSystemDarkTheme() {
    DWORD v = 1, size = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &size) == ERROR_SUCCESS)
        return v == 0;
    return false;
}
}

void CandidateWindow::UpdateTheme() {
    dark_ = IsSystemDarkTheme();
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
        kClassName, L"", WS_POPUP, // 边框在 OnPaint 自绘（颜色随系统深浅色）
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
    UpdateTheme();
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
    UpdateTheme();
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

    // 跟随系统深浅色（Win11 Dark 配色）；浅色沿用系统经典色
    const COLORREF colBg      = dark_ ? RGB(0x2B, 0x2B, 0x2B) : GetSysColor(COLOR_WINDOW);
    const COLORREF colText    = dark_ ? RGB(0xEC, 0xEC, 0xEC) : GetSysColor(COLOR_WINDOWTEXT);
    const COLORREF colHlBg    = dark_ ? RGB(0x0F, 0x6C, 0xBD) : GetSysColor(COLOR_HIGHLIGHT);
    const COLORREF colHlText  = dark_ ? RGB(0xFF, 0xFF, 0xFF) : GetSysColor(COLOR_HIGHLIGHTTEXT);
    const COLORREF colHint    = dark_ ? RGB(0x8F, 0x8F, 0x8F) : RGB(0x90, 0x90, 0x90);
    const COLORREF colHintHl  = dark_ ? RGB(0xB7, 0xD3, 0xF2) : RGB(0xC5, 0xDA, 0xF5);
    const COLORREF colBorder  = dark_ ? RGB(0x4A, 0x4A, 0x4A) : RGB(0xC9, 0xC9, 0xC9);

    HBRUSH hbrBg = CreateSolidBrush(colBg);
    FillRect(hdc, &rc, hbrBg);
    DeleteObject(hbrBg);
    // 右上角页码 "n/m"（首选高亮行上，灰色弱化）
    if (pageCount_ > 1) {
        wchar_t page[16] = {0};
        wsprintfW(page, L"%d/%d", page_, pageCount_);
        int len = (int)wcslen(page);
        SIZE szPage = {};
        GetTextExtentPoint32W(hdc, page, len, &szPage);
        SetTextColor(hdc, colHint);
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
            HBRUSH hbrHl = CreateSolidBrush(colHlBg);
            FillRect(hdc, &hl, hbrHl);
            DeleteObject(hbrHl);
            SetTextColor(hdc, colHlText);
        } else {
            SetTextColor(hdc, colText);
        }
        TextOutW(hdc, 8, y, num, (int)wcslen(num));
        TextOutW(hdc, 8 + numW_, y, items_[i].text.c_str(), (int)items_[i].text.size());
        // 编码提示：补全该候选还差的编码，灰色弱化
        if (!items_[i].hint.empty()) {
            SIZE szText = {};
            GetTextExtentPoint32W(hdc, items_[i].text.c_str(),
                                  (int)items_[i].text.size(), &szText);
            SetTextColor(hdc, first ? colHintHl : colHint);
            TextOutW(hdc, 8 + numW_ + szText.cx + 6, y,
                     items_[i].hint.c_str(), (int)items_[i].hint.size());
        }
        y += lineHeight_;
    }
    // 自绘边框（替代 WS_BORDER，颜色随主题）
    HPEN hpen = CreatePen(PS_SOLID, 1, colBorder);
    HGDIOBJ oldPen = SelectObject(hdc, hpen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, width_, lineHeight_ * (int)items_.size() + 8);
    SelectObject(hdc, oldPen);
    DeleteObject(hpen);
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
