// 候选窗口实现：极简自绘 popup
#include "CandidateWindow.h"
#include "QyLog.h"

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

// 取输入点所在显示器的工作区（兼容多屏及 RDP/云桌面动态分辨率）
RECT GetMonitorWorkArea(int x, int y) {
    RECT wa = {0, 0, GetSystemMetrics(SM_CXSCREEN) - 1, GetSystemMetrics(SM_CYSCREEN) - 1};
    POINT probe = {x, y};
    HMONITOR hmon = MonitorFromPoint(probe, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (hmon && GetMonitorInfoW(hmon, &mi)) wa = mi.rcWork;
    return wa;
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
                             int page, int pageCount, int sel) {
    if (!Create()) return;
    items_ = items;
    page_ = page;
    pageCount_ = pageCount;
    sel_ = sel;
    UpdateTheme();
    RECT wa = GetMonitorWorkArea(x, y);
    RecalcSize((wa.right - wa.left) - 16);
    above_ = false; // 每次重新显示都重新判定放上方/下方
    int fx = 0, fy = 0;
    ApplyLayout(x, y, wa, fx, fy);
    SetWindowPos(hwnd_, HWND_TOPMOST, fx, fy, width_, h_,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    // 首次出现时记录候选框与输入光标的位置关系，便于诊断定位问题：
    //   input  = 输入光标点（组字区左下角，屏幕坐标）
    //   win    = 候选窗左上角最终位置
    //   gap    = 贴近光标那条边到输入点的距离（下方模式=上沿-y；上方模式=y-下沿）
    //   delta  = 窗口左上角相对输入点的偏移
    {
        const int gap = above_ ? (y - (fy + h_)) : (fy - y);
        wchar_t b[160];
        wsprintfW(b, L"cand first show: input=%d,%d win=%d,%d size=%dx%d above=%d horiz=%d gap=%d delta=%d,%d",
                  x, y, fx, fy, width_, h_, above_ ? 1 : 0, horizontal_ ? 1 : 0, gap, fx - x, fy - y);
        qy::log::Write(b);
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void CandidateWindow::Update(int x, int y, const std::vector<CandidateItem>& items,
                             int page, int pageCount, int sel) {
    if (!hwnd_) return;
    items_ = items;
    page_ = page;
    pageCount_ = pageCount;
    sel_ = sel;
    UpdateTheme();
    RECT wa = GetMonitorWorkArea(x, y);
    RecalcSize((wa.right - wa.left) - 16);
    int fx = 0, fy = 0;
    ApplyLayout(x, y, wa, fx, fy);
    SetWindowPos(hwnd_, nullptr, fx, fy, width_, h_,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    wchar_t b[112];
    wsprintfW(b, L"cand window relayout pos=%d,%d size=%dx%d n=%u above=%d horiz=%d",
              fx, fy, width_, h_, (unsigned)items_.size(), above_ ? 1 : 0, horizontal_ ? 1 : 0);
    qy::log::Write(b);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

// 计算最终窗口位置。默认在输入点 (x, y) 下方；屏幕下方放不下时翻到上方，
// 上方模式锚定下沿（y - h_ - 24）：候选数减少、框体缩小时整体下移，
// 保证贴近输入点的那条边始终不动。
void CandidateWindow::ApplyLayout(int x, int y, const RECT& wa, int& outX, int& outY) {
    const int waL = wa.left, waR = wa.right, waT = wa.top, waB = wa.bottom;

    int nx = x, ny = y;
    if (nx + width_ > waR) nx = waR - width_;
    if (nx < waL) nx = waL;
    const bool fitsBelow = (y + h_ <= waB);
    if (above_) {
        ny = y - h_ - 24; // 下沿锚定在输入点上方 24px
        if (ny < waT) {
            if (fitsBelow) { above_ = false; ny = y; } // 上方也放不下则回下方
            else ny = waT;
        }
    } else if (!fitsBelow) {
        above_ = true;
        ny = y - h_ - 24;
        if (ny < waT) ny = waT;
    }
    outX = nx;
    outY = ny;
}

void CandidateWindow::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
    items_.clear();
    hSlots_.clear();
    above_ = false; // 下次显示重新判定布局模式
}

bool CandidateWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void CandidateWindow::RecalcSize(int maxBarWidth) {
    HDC hdc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(hdc, font_);

    SIZE szPage = {};
    if (pageCount_ > 1) {
        wchar_t page[16] = {0};
        wsprintfW(page, L"%d/%d", page_, pageCount_);
        GetTextExtentPoint32W(hdc, page, (int)wcslen(page), &szPage);
    }

    if (!horizontal_) {
        hSlots_.clear();
        SIZE szNum = {};
        const wchar_t* numSample = items_.size() >= 10 ? L"10. " : L"9. ";
        GetTextExtentPoint32W(hdc, numSample, (int)wcslen(numSample), &szNum);
        numW_ = szNum.cx;
        pageW_ = (pageCount_ > 1) ? (szPage.cx + 10) : 0; // 右侧页码列（含间隙）
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
        width_ = w;
        h_ = lineHeight_ * (int)items_.size() + 8;
    } else {
        // 横排模式：各候选项从左到右排列，超出当前显示器工作区宽度时自动折行
        SIZE szRef = {};
        GetTextExtentPoint32W(hdc, L"国", 1, &szRef);
        lineHeight_ = (szRef.cy > 0 ? szRef.cy : 18) + 8;
        pageW_ = (pageCount_ > 1) ? (szPage.cx + 14) : 0;

        const int kPadX = 10;
        const int kPadY = 4;
        const int kItemGap = 14;
        const int wrapLimit = (maxBarWidth > 240) ? maxBarWidth : 1200;

        hSlots_.resize(items_.size());
        int curX = kPadX;
        int curY = kPadY;
        int maxRowRight = kPadX;
        int firstRowRight = kPadX;
        bool onFirstRow = true;

        for (size_t i = 0; i < items_.size(); ++i) {
            wchar_t num[8] = {0};
            wsprintfW(num, L"%d. ", (int)i + 1);
            SIZE szNum = {}, szText = {}, szHint = {};
            GetTextExtentPoint32W(hdc, num, (int)wcslen(num), &szNum);
            GetTextExtentPoint32W(hdc, items_[i].text.c_str(), (int)items_[i].text.size(), &szText);
            if (!items_[i].hint.empty())
                GetTextExtentPoint32W(hdc, items_[i].hint.c_str(), (int)items_[i].hint.size(), &szHint);

            int itemW = szNum.cx + szText.cx + (szHint.cx ? 4 + szHint.cx : 0);
            int reserveRight = onFirstRow ? pageW_ : 0;
            if (curX > kPadX && (curX + itemW + reserveRight + kPadX > wrapLimit)) {
                curX = kPadX;
                curY += lineHeight_ + 2;
                onFirstRow = false;
            }

            hSlots_[i] = { curX, curY, itemW, szNum.cx, szText.cx };
            int rightEdge = curX + itemW;
            if (rightEdge > maxRowRight) maxRowRight = rightEdge;
            if (onFirstRow && rightEdge > firstRowRight) firstRowRight = rightEdge;
            curX += itemW + kItemGap;
        }

        int totalW = maxRowRight + kPadX;
        int firstRowTotalW = firstRowRight + pageW_ + kPadX;
        if (firstRowTotalW > totalW) totalW = firstRowTotalW;
        width_ = totalW > 60 ? totalW : 60;
        h_ = (curY - kPadY) + lineHeight_ + kPadY * 2;
    }

    SelectObject(hdc, old);
    ReleaseDC(hwnd_, hdc);
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

    if (!horizontal_) {
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
            bool hl = ((int)i == sel_); // 高亮行 = ↑/↓ 移动的选中项（默认第 1 行）
            if (hl) {
                RECT hlr = {2, y - 2, width_ - 2, y + lineHeight_};
                HBRUSH hbrHl = CreateSolidBrush(colHlBg);
                FillRect(hdc, &hlr, hbrHl);
                DeleteObject(hbrHl);
                SetTextColor(hdc, colHlText);
            } else {
                SetTextColor(hdc, colText);
            }
            TextOutW(hdc, 8, y, num, (int)wcslen(num));
            TextOutW(hdc, 8 + numW_, y, items_[i].text.c_str(), (int)items_[i].text.size());
            if (!items_[i].hint.empty()) {
                SIZE szText = {};
                GetTextExtentPoint32W(hdc, items_[i].text.c_str(),
                                      (int)items_[i].text.size(), &szText);
                SetTextColor(hdc, hl ? colHintHl : colHint);
                TextOutW(hdc, 8 + numW_ + szText.cx + 6, y,
                         items_[i].hint.c_str(), (int)items_[i].hint.size());
            }
            y += lineHeight_;
        }
    } else {
        // 横排绘制：每个候选项独立高亮块，右侧显示页码
        for (size_t i = 0; i < items_.size() && i < hSlots_.size(); ++i) {
            const HorizSlot& sl = hSlots_[i];
            wchar_t num[8];
            wsprintfW(num, L"%d. ", (int)i + 1);
            bool hl = ((int)i == sel_);
            int textY = sl.y + 4;
            if (hl) {
                RECT hlr = { sl.x - 5, sl.y, sl.x + sl.w + 5, sl.y + lineHeight_ };
                HBRUSH hbrHl = CreateSolidBrush(colHlBg);
                FillRect(hdc, &hlr, hbrHl);
                DeleteObject(hbrHl);
                SetTextColor(hdc, colHlText);
            } else {
                SetTextColor(hdc, colText);
            }
            TextOutW(hdc, sl.x, textY, num, (int)wcslen(num));
            TextOutW(hdc, sl.x + sl.numW, textY,
                     items_[i].text.c_str(), (int)items_[i].text.size());
            if (!items_[i].hint.empty()) {
                SetTextColor(hdc, hl ? colHintHl : colHint);
                TextOutW(hdc, sl.x + sl.numW + sl.textW + 4, textY,
                         items_[i].hint.c_str(), (int)items_[i].hint.size());
            }
        }
        if (pageCount_ > 1) {
            wchar_t page[16] = {0};
            wsprintfW(page, L"%d/%d", page_, pageCount_);
            int len = (int)wcslen(page);
            SIZE szPage = {};
            GetTextExtentPoint32W(hdc, page, len, &szPage);
            SetTextColor(hdc, colHint);
            TextOutW(hdc, width_ - 10 - szPage.cx, 8, page, len);
        }
    }

    // 自绘边框（替代 WS_BORDER，颜色随主题）
    HPEN hpen = CreatePen(PS_SOLID, 1, colBorder);
    HGDIOBJ oldPen = SelectObject(hdc, hpen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, width_, h_);
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
