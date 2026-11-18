// 任务栏 中/英 状态指示按钮实现
#include <new> // std::nothrow
#include <vector>
#include <objbase.h> // 先于 oleauto（oleauto.h 依赖 IID 已定义）
#include <oleauto.h> // SysAllocString
#include "LangBar.h"
#include "Guids.h"
#include "QyLog.h"

// 连接点错误码（SDK 里在 olectl.h，为免拖入 OLE 控件头直接取标准值）
#ifndef CONNECT_E_NOCONNECTION
#define CONNECT_E_NOCONNECTION ((HRESULT)0x80040200L)
#endif
#ifndef CONNECT_E_ADVISELIMIT
#define CONNECT_E_ADVISELIMIT  ((HRESULT)0x80040201L)
#endif
#ifndef CONNECT_E_CANNOTCONNECT
#define CONNECT_E_CANNOTCONNECT ((HRESULT)0x80040202L)
#endif

namespace qy {

// 运行时用 GDI 把一个汉字渲染成带 alpha 的小图标。
// 先画白字黑底取 R 通道当覆盖率，再把像素改写成“纯色 + 直通 alpha”。
static HICON RenderGlyphIcon(LPCWSTR glyph, COLORREF rgb) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = -cy; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdc = CreateCompatibleDC(hdcScreen);
    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hdc || !hbmp) {
        if (hbmp) DeleteObject(hbmp);
        if (hdc) DeleteDC(hdc);
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }
    HGDIOBJ oldBmp = SelectObject(hdc, hbmp);

    // 用 GB2312_CHARSET：有雅黑 UI 时仍选雅黑 UI；英文/精简版 Server 缺该字体时
    // GDI 会按中文字体集回退（如 SimSun / Microsoft YaHei），避免落到西文字体。
    HFONT f = CreateFontW(cy - 2, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                          GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                          L"Microsoft YaHei UI");
    HGDIOBJ oldFont = f ? SelectObject(hdc, f) : nullptr;
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, OPAQUE);
    RECT rc = {0, 0, cx, cy};
    DrawTextW(hdc, glyph, -1, &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // CreateBitmap 要求单色掩码每行按 WORD（2 字节 = 16 像素）对齐；
    // 透明处掩码置 1、字形处掩码置 0，兼顾 16 位色远程桌面 / Server 控制台。
    const int maskStride = ((cx + 15) / 16) * 2;
    std::vector<BYTE> maskBits(static_cast<size_t>(maskStride) * cy, 0xFF);

    // DIB 32bpp 内存布局为 0xAARRGGBB，而 COLORREF 为 0x00BBGGRR，需显式重组通道
    const DWORD bgraColor = (static_cast<DWORD>(GetRValue(rgb)) << 16) |
                            (static_cast<DWORD>(GetGValue(rgb)) << 8) |
                            static_cast<DWORD>(GetBValue(rgb));
    DWORD* px = static_cast<DWORD*>(bits);
    for (int y = 0; y < cy; ++y) {
        for (int x = 0; x < cx; ++x) {
            int i = y * cx + x;
            DWORD p = px[i];
            DWORD r = (p >> 16) & 0xFF;
            DWORD g = (p >> 8) & 0xFF;
            DWORD b = p & 0xFF;
            DWORD coverage = (r + 2 * g + b) / 4; // 白字黑底亮度即覆盖率
            if (coverage > 0) {
                px[i] = (coverage << 24) | bgraColor;
                maskBits[static_cast<size_t>(y) * maskStride + (x / 8)] &=
                    static_cast<BYTE>(~(0x80 >> (x % 8)));
            } else {
                px[i] = 0; // 透明像素 RGB 清零，避免预乘路径泛白块
            }
        }
    }

    if (oldFont) { SelectObject(hdc, oldFont); DeleteObject(f); }
    SelectObject(hdc, oldBmp);

    HBITMAP hmask = CreateBitmap(cx, cy, 1, 1, maskBits.data());

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmp;
    ii.hbmMask = hmask;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmp);
    if (hmask) DeleteObject(hmask);
    DeleteDC(hdc);
    ReleaseDC(nullptr, hdcScreen);
    return hIcon;
}

static const COLORREF kGlyphForLightTaskbar = RGB(0x20, 0x20, 0x28); // 浅色任务栏用近黑字
static const COLORREF kGlyphForDarkTaskbar  = RGB(0xFF, 0xFF, 0xFF); // 深色任务栏用纯白字

namespace {
// message-only 窗口类名（异步补发 OnUpdate 用，构造期创建）
const wchar_t* kPokeWndClass = L"QiangYinLangBarPoke";

// 判断任务栏是否为浅色底：
// - 优先处理高对比度主题（取系统按钮底色亮度）
// - 常规主题读 HKCU\...\Personalize\SystemUsesLightTheme（1=浅色任务栏，0=深色任务栏）
// - 若注册表无此键：Windows Server 2016/2019（Win10 1607/1809）尚未引入浅色任务栏，
//   任务栏恒为纯黑底色，按 false（深色）处理；Win11 / Server 2025（build>=22000）默认浅色。
bool IsTaskbarLightTheme() {
    HIGHCONTRASTW hc = {};
    hc.cbSize = sizeof(hc);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) &&
        (hc.dwFlags & HCF_HIGHCONTRASTON)) {
        COLORREF bg = GetSysColor(COLOR_BTNFACE);
        int lum = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
        return lum >= 128;
    }
    DWORD v = 0, size = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &size) == ERROR_SUCCESS) {
        return v != 0;
    }
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        auto fn = reinterpret_cast<RtlGetVersionFn>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
        if (fn) {
            OSVERSIONINFOW osvi = {};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (fn(&osvi) == 0) return osvi.dwBuildNumber >= 22000;
        }
    }
    return false;
}
}

CQiangYinLangBarItem::CQiangYinLangBarItem(ILangBarHost* host, bool chinese)
    : _cRef(1), _host(host), _chinese(chinese) {
    _hIconZhLight = RenderGlyphIcon(L"中", kGlyphForLightTaskbar);
    _hIconEnLight = RenderGlyphIcon(L"英", kGlyphForLightTaskbar);
    _hIconZhDark  = RenderGlyphIcon(L"中", kGlyphForDarkTaskbar);
    _hIconEnDark  = RenderGlyphIcon(L"英", kGlyphForDarkTaskbar);
    // poke 窗口在构造期创建（Activate 上下文），绝不在系统回调里建窗。
    // 任务栏要收到 OnUpdate 才渲染图标，接入 sink 后经此窗口异步补发。
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSW tmp = {};
    if (!GetClassInfoW(hInst, kPokeWndClass, &tmp)) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = PokeWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = kPokeWndClass;
        RegisterClassW(&wc);
    }
    _pokeWnd = CreateWindowExW(0, kPokeWndClass, L"", WS_POPUP,
                               0, 0, 0, 0, HWND_MESSAGE, nullptr,
                               hInst, this);
    {
        wchar_t b[128];
        wsprintfW(b, L"LangBar init: lightTaskbar=%d icons=%d,%d,%d,%d poke=%d",
                  IsTaskbarLightTheme() ? 1 : 0,
                  _hIconZhLight ? 1 : 0, _hIconEnLight ? 1 : 0,
                  _hIconZhDark ? 1 : 0, _hIconEnDark ? 1 : 0,
                  _pokeWnd ? 1 : 0);
        qy::log::Write(b);
    }
}

CQiangYinLangBarItem::~CQiangYinLangBarItem() {
    if (_pSink) _pSink->Release();
    if (_pokeWnd) { KillTimer(_pokeWnd, 1); DestroyWindow(_pokeWnd); }
    if (_hIconZhLight) DestroyIcon(_hIconZhLight);
    if (_hIconEnLight) DestroyIcon(_hIconEnLight);
    if (_hIconZhDark)  DestroyIcon(_hIconZhDark);
    if (_hIconEnDark)  DestroyIcon(_hIconEnDark);
}

// ---- IUnknown ----
STDMETHODIMP CQiangYinLangBarItem::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    // ITfLangBarItemButton 继承自 ITfLangBarItem，无需单独列基类
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) ||
        IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppv = static_cast<ITfLangBarItemButton*>(this);
    } else if (IsEqualIID(riid, IID_ITfSource)) {
        *ppv = static_cast<ITfSource*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CQiangYinLangBarItem::AddRef() { return InterlockedIncrement(&_cRef); }

STDMETHODIMP_(ULONG) CQiangYinLangBarItem::Release() {
    LONG c = InterlockedDecrement(&_cRef);
    if (c == 0) delete this;
    return c;
}

// ---- ITfLangBarItem ----
STDMETHODIMP CQiangYinLangBarItem::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (!pInfo) return E_POINTER;
    pInfo->clsidService = CLSID_QiangYinTextService;
    pInfo->guidItem = GUID_QY_LBI_INPUTMODE;
    // BTN_MENU：右键出菜单（任务栏发 OnClick(TF_LBI_CLK_RIGHT)，浮动语言栏走 InitMenu）
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU |
                     TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort = 1;
    lstrcpyW(pInfo->szDescription, L"强音 中/英状态");
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::GetStatus(DWORD* pdwStatus) {
    if (!pdwStatus) return E_POINTER;
    *pdwStatus = _status;
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::Show(BOOL fShow) {
    DWORD bit = fShow ? 0 : TF_LBI_STATUS_HIDDEN;
    if ((_status & TF_LBI_STATUS_HIDDEN) != bit) {
        if (fShow) _status &= ~TF_LBI_STATUS_HIDDEN;
        else _status |= TF_LBI_STATUS_HIDDEN;
        NotifyIconChanged();
    }
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::GetTooltipString(BSTR* pbstrToolTip) {
    if (!pbstrToolTip) return E_POINTER;
    *pbstrToolTip = SysAllocString(L"强音输入法：左键切换 中/英，右键打开菜单");
    return *pbstrToolTip ? S_OK : E_OUTOFMEMORY;
}

// ---- ITfLangBarItemButton ----
static const UINT kMenuIdSettings   = 100;
static const UINT kMenuIdPunct      = 101;
static const UINT kMenuIdFullShape  = 102;
static const UINT kMenuIdUserDict   = 103;
static const UINT kMenuIdAddWord    = 104;
static const UINT kMenuIdHorizontal = 105;
static const UINT kMenuIdAbout      = 106;

STDMETHODIMP CQiangYinLangBarItem::OnClick(TfLBIClick click, POINT pt, const RECT*) {
    if (click == TF_LBI_CLK_LEFT) {
        if (_host) _host->OnLangBarToggleMode();
    } else if (click == TF_LBI_CLK_RIGHT && _host) {
        // 右键菜单：借用当前进程的焦点窗口当属主（weasel 同款做法）。
        // \t 后是快捷键，菜单会右对齐显示在加速键栏
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu,
                    MF_STRING | (_host->LangBarIsChinesePunct() ? MF_CHECKED : 0),
                    kMenuIdPunct, L"中文标点\tCtrl+.");
        AppendMenuW(menu,
                    MF_STRING | (_host->LangBarIsFullShape() ? MF_CHECKED : 0),
                    kMenuIdFullShape, L"全角模式\tShift+空格");
        AppendMenuW(menu,
                    MF_STRING | (_host->LangBarIsHorizontal() ? MF_CHECKED : 0),
                    kMenuIdHorizontal, L"横排候选");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuIdUserDict, L"打开用户词库...");
        AppendMenuW(menu, MF_STRING, kMenuIdAddWord, L"造词\tCtrl++");
        AppendMenuW(menu, MF_STRING, kMenuIdSettings, L"设置...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuIdAbout, L"关于...");
        HWND owner = GetFocus();
        if (!owner) owner = GetForegroundWindow();
        if (owner) {
            UINT cmd = TrackPopupMenuEx(menu,
                                        TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        pt.x, pt.y, owner, nullptr);
            switch (cmd) {
            case kMenuIdPunct:      _host->OnLangBarTogglePunct(); break;
            case kMenuIdFullShape:  _host->OnLangBarToggleFullShape(); break;
            case kMenuIdHorizontal: _host->OnLangBarToggleHorizontal(); break;
            case kMenuIdUserDict:   _host->OnLangBarOpenUserDict(); break;
            case kMenuIdAddWord:    _host->OnLangBarAddWord(); break;
            case kMenuIdSettings:   _host->OnLangBarOpenSettings(); break;
            case kMenuIdAbout:      _host->OnLangBarOpenAbout(); break;
            default: break;
            }
        }
        DestroyMenu(menu);
    }
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::InitMenu(ITfMenu* pMenu) {
    // 浮动语言栏路径：把菜单项加进系统传来的菜单
    if (pMenu && _host) {
        pMenu->AddMenuItem(kMenuIdPunct,
                           _host->LangBarIsChinesePunct() ? TF_LBMENUF_CHECKED : 0,
                           nullptr, nullptr, L"中文标点\tCtrl+.", 11, nullptr);
        pMenu->AddMenuItem(kMenuIdFullShape,
                           _host->LangBarIsFullShape() ? TF_LBMENUF_CHECKED : 0,
                           nullptr, nullptr, L"全角模式\tShift+空格", 12, nullptr);
        pMenu->AddMenuItem(kMenuIdHorizontal,
                           _host->LangBarIsHorizontal() ? TF_LBMENUF_CHECKED : 0,
                           nullptr, nullptr, L"横排候选", 4, nullptr);
        pMenu->AddMenuItem(kMenuIdUserDict, 0, nullptr, nullptr,
                           L"打开用户词库", 6, nullptr);
        pMenu->AddMenuItem(kMenuIdAddWord, 0, nullptr, nullptr,
                           L"造词\tCtrl++", 8, nullptr);
        pMenu->AddMenuItem(kMenuIdSettings, 0, nullptr, nullptr,
                           L"设置...", 5, nullptr);
        pMenu->AddMenuItem(kMenuIdAbout, 0, nullptr, nullptr,
                           L"关于...", 5, nullptr);
    }
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::OnMenuSelect(UINT wID) {
    if (!_host) return S_OK;
    switch (wID) {
    case kMenuIdPunct:      _host->OnLangBarTogglePunct(); break;
    case kMenuIdFullShape:  _host->OnLangBarToggleFullShape(); break;
    case kMenuIdHorizontal: _host->OnLangBarToggleHorizontal(); break;
    case kMenuIdUserDict:   _host->OnLangBarOpenUserDict(); break;
    case kMenuIdAddWord:    _host->OnLangBarAddWord(); break;
    case kMenuIdSettings:   _host->OnLangBarOpenSettings(); break;
    case kMenuIdAbout:      _host->OnLangBarOpenAbout(); break;
    default: break;
    }
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_POINTER;
    // 每次取图标时按当前任务栏深浅色选择对应字色，交给系统的是副本
    const bool lightTaskbar = IsTaskbarLightTheme();
    HICON src = _chinese
        ? (lightTaskbar ? _hIconZhLight : _hIconZhDark)
        : (lightTaskbar ? _hIconEnLight : _hIconEnDark);
    if (!src) return E_FAIL;
    *phIcon = CopyIcon(src);
    return *phIcon ? S_OK : E_FAIL;
}

STDMETHODIMP CQiangYinLangBarItem::GetText(BSTR* pbstrText) {
    if (!pbstrText) return E_POINTER;
    *pbstrText = SysAllocString(_chinese ? L"中" : L"英");
    return *pbstrText ? S_OK : E_OUTOFMEMORY;
}

// ---- ITfSource ----
static const DWORD kLangBarSinkCookie = 0x51424152; // 'QBAR'

STDMETHODIMP CQiangYinLangBarItem::AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) {
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
        return CONNECT_E_CANNOTCONNECT;
    if (_pSink) {
        qy::log::Write(L"LangBar AdviseSink: refused (already connected)");
        return CONNECT_E_ADVISELIMIT;
    }
    if (!punk || !pdwCookie)
        return E_POINTER;
    HRESULT hr = punk->QueryInterface(IID_ITfLangBarItemSink,
                                      reinterpret_cast<void**>(&_pSink));
    if (FAILED(hr)) { qy::log::Write(L"LangBar AdviseSink: QI failed"); return hr; }
    *pdwCookie = kLangBarSinkCookie;
    qy::log::Write(L"LangBar AdviseSink: connected");
    // 只安排异步通知，绝不在 AdviseSink 内同步调 _pSink->OnUpdate：
    // AdviseSink 发生在系统 AddItem 流程中（系统持有项表锁），同步重入
    // 系统sink 在开始菜单/搜索等系统进程里可能死锁——开始菜单长时间
    // 无响应的嫌疑根因。150ms 后经 poke 窗口在普通消息上下文里补发。
    SchedulePoke();
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != kLangBarSinkCookie || !_pSink)
        return CONNECT_E_NOCONNECTION;
    qy::log::Write(L"LangBar UnadviseSink");
    _pSink->Release();
    _pSink = nullptr;
    return S_OK;
}

void CQiangYinLangBarItem::SetChineseMode(bool chinese) {
    if (chinese == _chinese) return;
    _chinese = chinese;
    NotifyIconChanged();
}

void CQiangYinLangBarItem::NotifyIconChanged() {
    if (!_pSink) return;
    _pSink->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP | TF_LBI_STATUS);
}

// ---- 异步补发：message-only 窗口 + 一次性定时器 ----

LRESULT CALLBACK CQiangYinLangBarItem::PokeWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CQiangYinLangBarItem* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<CQiangYinLangBarItem*>(
            reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<CQiangYinLangBarItem*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (msg == WM_TIMER) {
        KillTimer(hwnd, 1);
        if (self && self->_pSink) {
            // 普通消息上下文（不在系统回调里），同步通知安全
            self->NotifyIconChanged();
            qy::log::Write(L"LangBar deferred poke sent");
        }
        return 0;
    }
    if (msg == WM_DESTROY && self)
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void CQiangYinLangBarItem::SchedulePoke() {
    if (_pokeWnd) SetTimer(_pokeWnd, 1, 150, nullptr);
}

} // namespace qy
