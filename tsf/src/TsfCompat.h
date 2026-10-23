#pragma once
// TSF 兼容层：mingw-w64 的 msctf.h 缺少 ITfDisplayAttributeProvider 的声明，
// MSVC 的 Windows SDK 里是有的。这里只在 mingw 下补。
//
// 语言栏（任务栏中/英指示）相关：ITfLangBarItemButton / TfLBIClick / TF_LBI_*
// 在 MSVC SDK 和 mingw 里都在 ctfutb.h，但 msctf.h 不会自动包含它，需显式引入；
// mingw 的 ctfutb.h 缺 ITfLangBarItemButton / ITfMenu / TfLBIClick / TF_LBI_* 常量，
// 同样在下面补。
//
// 注意：IID_Xxx 由 DEFINE_GUID 声明为变量而非宏，所以不能用 #ifndef IID_Xxx
// 做判断（那样永远为真），改用编译器宏区分。
#include <msctf.h>
#include <ctfutb.h> // 语言栏接口（两个工具链都有，只是 mingw 的不全）

#ifdef __MINGW32__
DEFINE_GUID(IID_ITfDisplayAttributeProvider,
    0xfee47777, 0x163c, 0x4769, 0x99, 0x6a, 0x6e, 0x9c, 0x50, 0xd8, 0x9c, 0x22);

struct ITfDisplayAttributeProvider : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(
        IEnumTfDisplayAttributeInfo** ppEnum) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(
        REFGUID guid, ITfDisplayAttributeInfo** ppInfo) = 0;
};

// mingw 有 IEnumTfDisplayAttributeInfo 的 struct 定义但 uuid 库缺 IID，补声明
DEFINE_GUID(IID_IEnumTfDisplayAttributeInfo,
    0x7cef04d7, 0x8672, 0x44f8, 0xa2, 0xb8, 0x28, 0xd1, 0x55, 0x9a, 0x87, 0xb2);

// ---- 语言栏按钮（mingw ctfutb.h 缺失，按 Windows SDK ctfutb.h 的值补齐） ----
#ifndef TF_LBI_ICON
#define TF_LBI_ICON                0x00000001
#define TF_LBI_TEXT                0x00000002
#define TF_LBI_TOOLTIP             0x00000004
#define TF_LBI_BITMAP              0x00000008
#define TF_LBI_STATUS              0x00010000
#endif
#ifndef TF_LBI_STYLE_SHOWNINTRAY
#define TF_LBI_STYLE_SHOWNINTRAY   0x00000002
#define TF_LBI_STYLE_BTN_BUTTON    0x00010000
#define TF_LBI_STYLE_BTN_MENU      0x00020000
#endif
#ifndef TF_LBI_STATUS_HIDDEN
#define TF_LBI_STATUS_HIDDEN       0x00000001
#define TF_LBI_STATUS_DISABLED     0x00000002
#endif

#ifndef __TFLBICLICK_DEFINED__
#define __TFLBICLICK_DEFINED__
typedef enum TfLBIClick {
    TF_LBI_CLK_RIGHT = 1,
    TF_LBI_CLK_LEFT = 2
} TfLBIClick;
#endif

interface ITfMenu;

DEFINE_GUID(IID_ITfLangBarItemButton,
    0x28c7f1d0, 0xde25, 0x11d2, 0xaf, 0xdd, 0x00, 0x10, 0x5a, 0x27, 0x99, 0xb5);

struct ITfLangBarItemButton : public ITfLangBarItem {
    virtual HRESULT STDMETHODCALLTYPE OnClick(
        TfLBIClick click, POINT pt, const RECT* prcArea) = 0;
    virtual HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu* pMenu) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT wID) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIcon(HICON* phIcon) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetText(BSTR* pbstrText) = 0;
};

// mingw 的 uuid 库也没有这几个 TIP 能力分类的 GUID（定义在 Guids.cpp）：
// SYSTRAYSUPPORT / UIELEMENTENABLED / IMMERSIVESUPPORT / INPUTMODECOMPARTMENT
DEFINE_GUID(GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    0x25504fb4, 0x7bab, 0x4bc1, 0x9c, 0x69, 0xcf, 0x81, 0x89, 0x0f, 0x0e, 0xf5);
DEFINE_GUID(GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    0x49d2f9cf, 0x1f5e, 0x11d7, 0xa6, 0xd3, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);
DEFINE_GUID(GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    0x13a016df, 0x560b, 0x46cd, 0x94, 0x7a, 0x4c, 0x3a, 0xf1, 0xe0, 0xe3, 0x5d);
DEFINE_GUID(GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    0xccf05dd7, 0x4a87, 0x11d7, 0xa6, 0xe2, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);
// mingw 也没有输入模式转换 compartment 的 GUID（定义在 Guids.cpp）
DEFINE_GUID(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
    0xccf05dd8, 0x4a87, 0x11d7, 0xa6, 0xe2, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);
#endif // __MINGW32__

#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL ((TfClientId)0)
#endif

#ifndef TF_INVALID_GUIDATOM
#define TF_INVALID_GUIDATOM ((TfGuidAtom)-1)
#endif

// 转换模式 compartment 相关（任务栏中/英徽标）：标准 TSF 定义，msctf.h 里有
// TF_CONVERSIONMODE_* 是宏，msctf.h 一定有；这里只防万一
#ifndef TF_CONVERSIONMODE_ALPHANUMERIC
#define TF_CONVERSIONMODE_ALPHANUMERIC 0x0000
#endif
#ifndef TF_CONVERSIONMODE_NATIVE
#define TF_CONVERSIONMODE_NATIVE 0x0001
#endif
