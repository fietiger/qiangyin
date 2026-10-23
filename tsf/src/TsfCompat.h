#pragma once
// TSF 兼容层：mingw-w64 的 msctf.h 缺少 ITfDisplayAttributeProvider 的声明，
// MSVC 的 Windows SDK 里是有的。这里只在 mingw 下补。
//
// 注意：IID_Xxx 由 DEFINE_GUID 声明为变量而非宏，所以不能用 #ifndef IID_Xxx
// 做判断（那样永远为真），改用编译器宏区分。
#include <msctf.h>

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
#endif // __MINGW32__

#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL ((TfClientId)0)
#endif

#ifndef TF_INVALID_GUIDATOM
#define TF_INVALID_GUIDATOM ((TfGuidAtom)-1)
#endif

// 转换模式 compartment 相关（任务栏中/英徽标）：mingw 的 msctf.h 可能缺，兜底
#ifndef GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION
DEFINE_GUID(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
    0xccf05a62, 0x8b7d, 0x11d2, 0xb4, 0xb9, 0x0, 0xc0, 0x4f, 0x68, 0xe6, 0xdd);
#endif
#ifndef TF_CONVERSIONMODE_ALPHANUMERIC
#define TF_CONVERSIONMODE_ALPHANUMERIC 0x0000
#endif
#ifndef TF_CONVERSIONMODE_NATIVE
#define TF_CONVERSIONMODE_NATIVE 0x0001
#endif
