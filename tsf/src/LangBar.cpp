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

    HFONT f = CreateFontW(cy - 2, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                          L"Microsoft YaHei UI");
    HGDIOBJ oldFont = f ? SelectObject(hdc, f) : nullptr;
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, OPAQUE);
    RECT rc = {0, 0, cx, cy};
    DrawTextW(hdc, glyph, -1, &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DWORD* px = static_cast<DWORD*>(bits);
    for (int i = 0; i < cx * cy; ++i) {
        DWORD coverage = px[i] & 0xFF; // 白字的 R 分量 = 覆盖率
        px[i] = (coverage << 24) | rgb;
    }

    if (oldFont) { SelectObject(hdc, oldFont); DeleteObject(f); }
    SelectObject(hdc, oldBmp);

    // 32bpp 带 alpha 时掩码不会被用到，全 0 即可
    std::vector<BYTE> zeroMask(static_cast<size_t>((cx + 7) / 8) * cy, 0);
    HBITMAP hmask = CreateBitmap(cx, cy, 1, 1, zeroMask.data());

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

static const COLORREF kGlyphColor = RGB(0x20, 0x20, 0x28); // 近黑，浅/深主题任务栏药丸上都可见

CQiangYinLangBarItem::CQiangYinLangBarItem(ILangBarHost* host, bool chinese)
    : _cRef(1), _host(host), _chinese(chinese) {
    _hIconZh = RenderGlyphIcon(L"中", kGlyphColor);
    _hIconEn = RenderGlyphIcon(L"英", kGlyphColor);
}

CQiangYinLangBarItem::~CQiangYinLangBarItem() {
    if (_pSink) _pSink->Release();
    if (_hIconZh) DestroyIcon(_hIconZh);
    if (_hIconEn) DestroyIcon(_hIconEn);
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
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
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
    *pbstrToolTip = SysAllocString(L"强音输入法：点击切换 中/英");
    return *pbstrToolTip ? S_OK : E_OUTOFMEMORY;
}

// ---- ITfLangBarItemButton ----
STDMETHODIMP CQiangYinLangBarItem::OnClick(TfLBIClick click, POINT, const RECT*) {
    if (click == TF_LBI_CLK_LEFT && _host)
        _host->OnLangBarToggleMode();
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::InitMenu(ITfMenu*) { return S_OK; } // 无菜单

STDMETHODIMP CQiangYinLangBarItem::OnMenuSelect(UINT) { return S_OK; }

STDMETHODIMP CQiangYinLangBarItem::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_POINTER;
    // 交给系统的是副本，系统销毁与否都不影响我们持有的原件
    HICON src = _chinese ? _hIconZh : _hIconEn;
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
    if (_pSink)
        return CONNECT_E_ADVISELIMIT;
    if (!punk || !pdwCookie)
        return E_POINTER;
    HRESULT hr = punk->QueryInterface(IID_ITfLangBarItemSink,
                                      reinterpret_cast<void**>(&_pSink));
    if (FAILED(hr)) return hr;
    *pdwCookie = kLangBarSinkCookie;
    return S_OK;
}

STDMETHODIMP CQiangYinLangBarItem::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != kLangBarSinkCookie || !_pSink)
        return CONNECT_E_NOCONNECTION;
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

} // namespace qy
