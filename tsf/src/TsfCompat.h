#pragma once
// TSF 兼容层：mingw-w64 的 msctf.h 缺少 ITfDisplayAttributeProvider /
// IEnumTfDisplayAttributeInfo 的声明（Windows SDK 里有）。
// 这里用官方 GUID 补上；MSVC 编译时因宏已定义不会走到这里。
#include <msctf.h>

#ifndef IID_IEnumTfDisplayAttributeInfo
// mingw 有 struct 定义但缺 IID 宏，补上
DEFINE_GUID(IID_IEnumTfDisplayAttributeInfo,
    0x7cef04d7, 0x8672, 0x44f8, 0xa2, 0xb8, 0x28, 0xd1, 0x55, 0x9a, 0x87, 0xb2);
#endif

#ifndef IID_ITfDisplayAttributeProvider
// mingw 整个缺失，MSVC 的 SDK 里有；用官方 GUID 补上
DEFINE_GUID(IID_ITfDisplayAttributeProvider,
    0xfee47777, 0x163c, 0x4769, 0x99, 0x6a, 0x6e, 0x9c, 0x50, 0xd8, 0x9c, 0x22);

struct ITfDisplayAttributeProvider : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(
        IEnumTfDisplayAttributeInfo** ppEnum) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(
        REFGUID guid, ITfDisplayAttributeInfo** ppInfo) = 0;
};

#endif // IID_ITfDisplayAttributeProvider

#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL ((TfClientId)0)
#endif

#ifndef TF_INVALID_GUIDATOM
#define TF_INVALID_GUIDATOM ((TfGuidAtom)-1)
#endif
