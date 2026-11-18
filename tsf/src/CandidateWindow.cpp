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

// 计算与 WPF 设备无关单位一致的有效渲染 DPI：
// - DPI_UNAWARE 宿主（由 DWM 整窗位图放大）：返回 96，避免双重放大；
// - SYSTEM_DPI_AWARE 宿主（跨屏由 DWM 缩放）：返回窗口所属系统 DPI；
// - PER_MONITOR_DPI_AWARE 宿主（Chrome/VSCode/Win11 记事本等，DWM 不缩放）：
//   返回目标点 (x,y) 所在显示器的真实有效 DPI，跨不同缩放比副屏时物理字号也恒定。
int GetEffectiveDpiForPoint(HWND hwnd, int x, int y) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 && hwnd) {
        using GetWndCtxFn = HANDLE(WINAPI*)(HWND);
        using GetAwarenessFn = int(WINAPI*)(HANDLE); // 0=UNAWARE, 1=SYSTEM, 2=PER_MONITOR
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

        auto fnCtx = reinterpret_cast<GetWndCtxFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "GetWindowDpiAwarenessContext")));
        auto fnAware = reinterpret_cast<GetAwarenessFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "GetAwarenessFromDpiAwarenessContext")));
        auto fnWndDpi = reinterpret_cast<GetDpiForWindowFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForWindow")));

        if (fnCtx && fnAware) {
            int awareness = fnAware(fnCtx(hwnd));
            if (awareness == 0) return 96; // UNAWARE：交由 DWM 等比放大
            if (awareness == 1 && fnWndDpi) {
                UINT d = fnWndDpi(hwnd);
                if (d >= 96) return static_cast<int>(d);
            }
        }
    }

    // Per-Monitor 感知或旧版系统兜底：Shcore!GetDpiForMonitor(MDT_EFFECTIVE_DPI=0)
    // 该 API 本身也会按当前进程 DPI 感知模式返回对应有效值
    POINT probe = {x, y};
    HMONITOR hmon = MonitorFromPoint(probe, MONITOR_DEFAULTTONEAREST);
    if (hmon) {
        static HMODULE s_shcore = LoadLibraryW(L"Shcore.dll");
        if (s_shcore) {
            using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
            static auto fnMonDpi = reinterpret_cast<GetDpiForMonitorFn>(
                reinterpret_cast<void*>(GetProcAddress(s_shcore, "GetDpiForMonitor")));
            if (fnMonDpi) {
                UINT dx = 96, dy = 96;
                if (SUCCEEDED(fnMonDpi(hmon, 0 /*MDT_EFFECTIVE_DPI*/, &dx, &dy)) && dy >= 96)
                    return static_cast<int>(dy);
            }
        }
    }

    HDC hdc = GetDC(nullptr);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) ReleaseDC(nullptr, hdc);
    return dpi >= 96 ? dpi : 96;
}
}

void CandidateWindow::UpdateTheme() {
    dark_ = IsSystemDarkTheme();
}

CandidateWindow::CandidateWindow() = default;

CandidateWindow::~CandidateWindow() { Destroy(); }

void CandidateWindow::EnsureFont(int dpi) {
    if (dpi < 96) dpi = 96;
    if (font_ && appliedFontPt_ == fontSizePt_ && dpi_ == dpi) return;
    dpi_ = dpi;
    appliedFontPt_ = fontSizePt_;
    if (font_) { DeleteObject(font_); font_ = nullptr; }
    // 负高度 = 真实字符字面高度（WPF / 标准点制换算：px = pt * dpi / 72）
    int pxHeight = -MulDiv(fontSizePt_, dpi_, 72);
    font_ = CreateFontW(pxHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
}

bool CandidateWindow::Create() {
    if (hwnd_) return true;
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSW tmp = {};
    if (!GetClassInfoW(hInst, kClassName, &tmp)) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = kClassName;
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
        RegisterClassW(&wc);
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kClassName, L"", WS_POPUP, // 边框在 OnPaint 自绘（颜色随系统深浅色）
        0, 0, 120, 60, nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;

    EnsureFont(96);
    return true;
}

void CandidateWindow::Destroy() {
    if (font_) { DeleteObject(font_); font_ = nullptr; }
    appliedFontPt_ = 0;
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
    EnsureFont(GetEffectiveDpiForPoint(hwnd_, x, y));
    RECT wa = GetMonitorWorkArea(x, y);
    RecalcSize((wa.right - wa.left) - Scale(16));
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
        wchar_t b[180];
        wsprintfW(b, L"cand first show: input=%d,%d win=%d,%d size=%dx%d dpi=%d pt=%d above=%d horiz=%d gap=%d delta=%d,%d",
                  x, y, fx, fy, width_, h_, dpi_, fontSizePt_, above_ ? 1 : 0, horizontal_ ? 1 : 0, gap, fx - x, fy - y);
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
    EnsureFont(GetEffectiveDpiForPoint(hwnd_, x, y));
    RECT wa = GetMonitorWorkArea(x, y);
    RecalcSize((wa.right - wa.left) - Scale(16));
    int fx = 0, fy = 0;
    ApplyLayout(x, y, wa, fx, fy);
    SetWindowPos(hwnd_, nullptr, fx, fy, width_, h_,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    wchar_t b[128];
    wsprintfW(b, L"cand window relayout pos=%d,%d size=%dx%d n=%u dpi=%d above=%d horiz=%d",
              fx, fy, width_, h_, (unsigned)items_.size(), dpi_, above_ ? 1 : 0, horizontal_ ? 1 : 0);
    qy::log::Write(b);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

// 计算最终窗口位置。默认在输入点 (x, y) 下方；屏幕下方放不下时翻到上方，
// 上方模式锚定下沿（y - h_ - upGap）：候选数减少、框体缩小时整体下移，
// 保证贴近输入点的那条边始终不动。
void CandidateWindow::ApplyLayout(int x, int y, const RECT& wa, int& outX, int& outY) {
    const int waL = wa.left, waR = wa.right, waT = wa.top, waB = wa.bottom;
    const int upGap = Scale(24);

    int nx = x, ny = y;
    if (nx + width_ > waR) nx = waR - width_;
    if (nx < waL) nx = waL;
    const bool fitsBelow = (y + h_ <= waB);
    if (above_) {
        ny = y - h_ - upGap; // 下沿锚定在输入点上方（随 DPI 等比缩放）
        if (ny < waT) {
            if (fitsBelow) { above_ = false; ny = y; } // 上方也放不下则回下方
            else ny = waT;
        }
    } else if (!fitsBelow) {
        above_ = true;
        ny = y - h_ - upGap;
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

    const int pad4  = Scale(4);
    const int pad6  = Scale(6);
    const int pad8  = Scale(8);
    const int pad10 = Scale(10);
    const int pad12 = Scale(12);
    const int minW  = Scale(60);

    if (!horizontal_) {
        hSlots_.clear();
        SIZE szNum = {};
        const wchar_t* numSample = items_.size() >= 10 ? L"10. " : L"9. ";
        GetTextExtentPoint32W(hdc, numSample, (int)wcslen(numSample), &szNum);
        numW_ = szNum.cx;
        pageW_ = (pageCount_ > 1) ? (szPage.cx + pad10) : 0; // 右侧页码列（含间隙）
        int w = minW;
        for (const auto& s : items_) {
            SIZE sz = {}, szHint = {};
            GetTextExtentPoint32W(hdc, s.text.c_str(), (int)s.text.size(), &sz);
            if (!s.hint.empty())
                GetTextExtentPoint32W(hdc, s.hint.c_str(), (int)s.hint.size(), &szHint);
            int need = pad8 + numW_ + sz.cx + (szHint.cx ? pad6 + szHint.cx : 0) + pageW_ + pad12;
            if (need > w) w = need;
            lineHeight_ = sz.cy + pad8;
        }
        width_ = w;
        h_ = lineHeight_ * (int)items_.size() + pad4 * 2;
    } else {
        // 横排模式：各候选项从左到右排列，超出当前显示器工作区宽度时自动折行
        SIZE szRef = {};
        GetTextExtentPoint32W(hdc, L"国", 1, &szRef);
        lineHeight_ = (szRef.cy > 0 ? szRef.cy : Scale(18)) + pad8;
        pageW_ = (pageCount_ > 1) ? (szPage.cx + Scale(14)) : 0;

        const int kPadX = pad10;
        const int kPadY = pad4;
        // 项间距随字号与 DPI 自然伸展，大字号下也不显得拥挤
        int kItemGap = Scale(14);
        if (lineHeight_ / 2 > kItemGap) kItemGap = lineHeight_ / 2;
        const int wrapLimit = (maxBarWidth > Scale(240)) ? maxBarWidth : Scale(1200);

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

            int itemW = szNum.cx + szText.cx + (szHint.cx ? pad4 + szHint.cx : 0);
            int reserveRight = onFirstRow ? pageW_ : 0;
            if (curX > kPadX && (curX + itemW + reserveRight + kPadX > wrapLimit)) {
                curX = kPadX;
                curY += lineHeight_ + Scale(2);
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
        width_ = totalW > minW ? totalW : minW;
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

    const int pad2  = Scale(2);
    const int pad4  = Scale(4);
    const int pad5  = Scale(5);
    const int pad6  = Scale(6);
    const int pad8  = Scale(8);
    const int pad10 = Scale(10);

    if (!horizontal_) {
        // 右上角页码 "n/m"（首选高亮行上，灰色弱化）
        if (pageCount_ > 1) {
            wchar_t page[16] = {0};
            wsprintfW(page, L"%d/%d", page_, pageCount_);
            int len = (int)wcslen(page);
            SIZE szPage = {};
            GetTextExtentPoint32W(hdc, page, len, &szPage);
            SetTextColor(hdc, colHint);
            TextOutW(hdc, width_ - pad8 - szPage.cx, pad4, page, len);
        }
        int y = pad4;
        for (size_t i = 0; i < items_.size(); ++i) {
            wchar_t num[8];
            wsprintfW(num, L"%d. ", (int)i + 1);
            bool hl = ((int)i == sel_); // 高亮行 = ↑/↓ 移动的选中项（默认第 1 行）
            if (hl) {
                RECT hlr = { pad2, y - pad2, width_ - pad2, y + lineHeight_ };
                HBRUSH hbrHl = CreateSolidBrush(colHlBg);
                FillRect(hdc, &hlr, hbrHl);
                DeleteObject(hbrHl);
                SetTextColor(hdc, colHlText);
            } else {
                SetTextColor(hdc, colText);
            }
            TextOutW(hdc, pad8, y, num, (int)wcslen(num));
            TextOutW(hdc, pad8 + numW_, y, items_[i].text.c_str(), (int)items_[i].text.size());
            if (!items_[i].hint.empty()) {
                SIZE szText = {};
                GetTextExtentPoint32W(hdc, items_[i].text.c_str(),
                                      (int)items_[i].text.size(), &szText);
                SetTextColor(hdc, hl ? colHintHl : colHint);
                TextOutW(hdc, pad8 + numW_ + szText.cx + pad6, y,
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
            int textY = sl.y + pad4;
            if (hl) {
                RECT hlr = { sl.x - pad5, sl.y, sl.x + sl.w + pad5, sl.y + lineHeight_ };
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
                TextOutW(hdc, sl.x + sl.numW + sl.textW + pad4, textY,
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
            TextOutW(hdc, width_ - pad10 - szPage.cx, pad8, page, len);
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

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

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
    case WM_DPICHANGED:
        if (self) {
            self->EnsureFont(HIWORD(wp));
            if (lp) {
                const RECT* prc = reinterpret_cast<const RECT*>(lp);
                RECT wa = GetMonitorWorkArea(prc->left, prc->top);
                self->RecalcSize((wa.right - wa.left) - self->Scale(16));
                SetWindowPos(hwnd, nullptr, prc->left, prc->top,
                             self->width_, self->h_, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace qy
