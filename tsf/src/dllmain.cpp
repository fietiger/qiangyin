// DLL 入口、类厂、注册/反注册
#include <new> // std::nothrow
#include <windows.h>
#include <msctf.h>
#include <objbase.h> // CoCreateInstance, StringFromGUID2（显式包含）
#include "TextService.h"
#include "Guids.h"
#include "QyLog.h"

// TF_IPP_CAPS_SYSTRAYSUPPORT (Win8+)：允许输入法显示在任务栏系统托盘
// 正常 msctf.h 里有，这里兜底以防旧 SDK 缺定义
#ifndef TF_IPP_CAPS_SYSTRAYSUPPORT
#define TF_IPP_CAPS_SYSTRAYSUPPORT 0x0040
#endif

static LONG g_dllRefCount = 0;
static HMODULE g_hModule = nullptr;

// ---- 类厂 ----
class CClassFactory : public IClassFactory {
public:
    CClassFactory() : _cRef(1) { InterlockedIncrement(&g_dllRefCount); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG c = InterlockedDecrement(&_cRef);
        if (c == 0) delete this;
        return c;
    }
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override {
        QYLOG("CreateInstance called");
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        qy::CQiangYinTextService* svc = new (std::nothrow) qy::CQiangYinTextService();
        if (!svc) return E_OUTOFMEMORY;
        HRESULT hr = svc->QueryInterface(riid, ppv);
        svc->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) InterlockedIncrement(&g_dllRefCount);
        else InterlockedDecrement(&g_dllRefCount);
        return S_OK;
    }
private:
    virtual ~CClassFactory() { InterlockedDecrement(&g_dllRefCount); }
    LONG _cRef;
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    QYLOG("DllGetClassObject called");
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_QiangYinTextService))
        return CLASS_E_CLASSNOTAVAILABLE;
    CClassFactory* factory = new (std::nothrow) CClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_dllRefCount == 0 ? S_OK : S_FALSE;
}

static HRESULT RegisterServer() {
    wchar_t clsidStr[64] = {0};
    StringFromGUID2(CLSID_QiangYinTextService, clsidStr, 64);

    wchar_t modulePath[MAX_PATH] = {0};
    GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);

    // HKCR\CLSID\{...}\InprocServer32
    wchar_t keyPath[128] = {0};
    wsprintfW(keyPath, L"CLSID\\%s", clsidStr);

    HKEY hKey = nullptr, hSub = nullptr;
    DWORD disp = 0;
    HRESULT hr = E_FAIL;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hKey, &disp) != ERROR_SUCCESS)
        return E_FAIL;
    // 默认值：描述
    const wchar_t* desc = L"强音输入法";
    RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(desc),
                   (DWORD)(wcslen(desc) + 1) * sizeof(wchar_t));
    if (RegCreateKeyExW(hKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE,
                        nullptr, &hSub, &disp) == ERROR_SUCCESS) {
        RegSetValueExW(hSub, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(modulePath),
                       (DWORD)(wcslen(modulePath) + 1) * sizeof(wchar_t));
        RegSetValueExW(hSub, L"ThreadingModel", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(L"Apartment"),
                       10 * sizeof(wchar_t));
        RegCloseKey(hSub);
        hr = S_OK;
    }
    RegCloseKey(hKey);
    if (FAILED(hr)) return hr;

    // 注册 TSF profile
    ITfInputProcessorProfileMgr* pMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITfInputProcessorProfileMgr,
                          reinterpret_cast<void**>(&pMgr));
    if (SUCCEEDED(hr)) {
        hr = pMgr->RegisterProfile(
            CLSID_QiangYinTextService,
            MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
            GUID_QiangYinProfile,
            L"强音", 2,                       // 描述及长度
            modulePath, (ULONG)wcslen(modulePath), // 图标文件及长度
            0,                        // 图标索引
            nullptr,                  // hklSubstitute
            0,                        // dwPreferredLayout
            TRUE,                     // 默认启用
            TF_IPP_CAPS_SYSTRAYSUPPORT); // Win8+：允许显示在任务栏系统托盘
        pMgr->Release();
    }
    if (FAILED(hr)) return hr;

    // 注册分类：让任务栏识别为键盘输入法（安装时注册，Activate 里也会再注册一次）
    // GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT：系统托盘集成（Chewing 等开源输入法都注册这个，
    // 仅靠 RegisterProfile 的 TF_IPP_CAPS_SYSTRAYSUPPORT flag 不够）
    ITfCategoryMgr* pCat = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(&pCat)))) {
        pCat->RegisterCategory(CLSID_QiangYinTextService,
                               GUID_TFCAT_TIP_KEYBOARD,
                               CLSID_QiangYinTextService);
        pCat->RegisterCategory(CLSID_QiangYinTextService,
                               GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
                               CLSID_QiangYinTextService);
        pCat->RegisterCategory(CLSID_QiangYinTextService,
                               GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
                               CLSID_QiangYinTextService);
        pCat->RegisterCategory(CLSID_QiangYinTextService,
                               GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
                               CLSID_QiangYinTextService);
        pCat->RegisterCategory(CLSID_QiangYinTextService,
                               GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
                               CLSID_QiangYinTextService);
        pCat->Release();
    }
    return hr;
}

static HRESULT UnregisterServer() {
    ITfInputProcessorProfileMgr* pMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfileMgr,
                                  reinterpret_cast<void**>(&pMgr));
    if (SUCCEEDED(hr)) {
        pMgr->UnregisterProfile(CLSID_QiangYinTextService,
                                MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
                                GUID_QiangYinProfile, 0);
        pMgr->Release();
    }
    // 注销分类
    ITfCategoryMgr* pCat = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(&pCat)))) {
        pCat->UnregisterCategory(CLSID_QiangYinTextService,
                                 GUID_TFCAT_TIP_KEYBOARD,
                                 CLSID_QiangYinTextService);
        pCat->UnregisterCategory(CLSID_QiangYinTextService,
                                 GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
                                 CLSID_QiangYinTextService);
        pCat->UnregisterCategory(CLSID_QiangYinTextService,
                                 GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
                                 CLSID_QiangYinTextService);
        pCat->UnregisterCategory(CLSID_QiangYinTextService,
                                 GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
                                 CLSID_QiangYinTextService);
        pCat->UnregisterCategory(CLSID_QiangYinTextService,
                                 GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
                                 CLSID_QiangYinTextService);
        pCat->Release();
    }
    wchar_t clsidStr[64] = {0};
    StringFromGUID2(CLSID_QiangYinTextService, clsidStr, 64);
    wchar_t keyPath[128] = {0};
    wsprintfW(keyPath, L"CLSID\\%s", clsidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath);
    return S_OK;
}

STDAPI DllRegisterServer() { return RegisterServer(); }
STDAPI DllUnregisterServer() { return UnregisterServer(); }
