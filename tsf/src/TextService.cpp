// 强音 TSF 文本服务实现
#include <new> // std::nothrow
#include <utility> // std::move
#include <objbase.h> // CoCreateInstance（显式包含，不依赖 msctf.h 的传递）
#include <oleauto.h> // SysAllocString, VARIANT
#include "TextService.h"
#include "Guids.h"
#include "QyLog.h"
#include "Utf.h"
#include "SettingsDialog.h"
#include <shellapi.h> // ShellExecuteW
#include <assert.h>

namespace qy {

// ---------------------------------------------------------------------------
// edit session 基类与具体 session
// ---------------------------------------------------------------------------
class CEditSessionBase : public ITfEditSession {
public:
    explicit CEditSessionBase(CQiangYinTextService* svc) : _svc(svc), _cRef(1) {
        _svc->AddRef();
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppv = static_cast<ITfEditSession*>(this);
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
protected:
    virtual ~CEditSessionBase() { _svc->Release(); }
    CQiangYinTextService* _svc;
    LONG _cRef;
};

class CStartCompositionSession : public CEditSessionBase {
public:
    using CEditSessionBase::CEditSessionBase;
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        return _svc->OnEditStartComposition(ec);
    }
};

class CUpdateCompositionSession : public CEditSessionBase {
public:
    CUpdateCompositionSession(CQiangYinTextService* svc, std::wstring text)
        : CEditSessionBase(svc), _text(std::move(text)) {}
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        return _svc->OnEditUpdateComposition(ec, _text);
    }
private:
    std::wstring _text;
};

class CCommitSession : public CEditSessionBase {
public:
    CCommitSession(CQiangYinTextService* svc, std::wstring text)
        : CEditSessionBase(svc), _text(std::move(text)) {}
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        return _svc->OnEditCommit(ec, _text);
    }
private:
    std::wstring _text;
};

class CCancelSession : public CEditSessionBase {
public:
    using CEditSessionBase::CEditSessionBase;
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        return _svc->OnEditCancel(ec);
    }
};

// ---------------------------------------------------------------------------
// 显示属性：组字下划线
// ---------------------------------------------------------------------------
class CDisplayAttributeInfo : public ITfDisplayAttributeInfo {
public:
    CDisplayAttributeInfo() : _cRef(1) {}
    virtual ~CDisplayAttributeInfo() = default;
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
            *ppv = static_cast<ITfDisplayAttributeInfo*>(this);
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
    STDMETHODIMP GetGUID(GUID* pguid) override {
        if (!pguid) return E_POINTER;
        *pguid = GUID_QiangYinDisplayAttr;
        return S_OK;
    }
    STDMETHODIMP GetDescription(BSTR* pbstr) override {
        if (!pbstr) return E_POINTER;
        *pbstr = SysAllocString(L"强音组字");
        return *pbstr ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) override {
        if (!pda) return E_POINTER;
        pda->crText.type = TF_CT_NONE;
        pda->crBk.type = TF_CT_NONE;
        pda->lsStyle = TF_LS_SOLID;
        pda->fBoldLine = FALSE;
        pda->crLine.type = TF_CT_COLORREF;
        pda->crLine.cr = RGB(0x1A, 0x73, 0xE8);
        pda->bAttr = TF_ATTR_TARGET_CONVERTED;
        return S_OK;
    }
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Reset() override { return S_OK; }
private:
    LONG _cRef;
};

class CEnumDisplayAttributeInfo : public IEnumTfDisplayAttributeInfo {
public:
    CEnumDisplayAttributeInfo() : _cRef(1), _index(0) {}
    virtual ~CEnumDisplayAttributeInfo() = default;
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
            *ppv = static_cast<IEnumTfDisplayAttributeInfo*>(this);
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
    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** ppEnum) override {
        if (!ppEnum) return E_POINTER;
        *ppEnum = new (std::nothrow) CEnumDisplayAttributeInfo();
        return *ppEnum ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP Next(ULONG ulCount, ITfDisplayAttributeInfo** rgInfo, ULONG* pcFetched) override {
        ULONG fetched = 0;
        while (fetched < ulCount && _index < 1) {
            rgInfo[fetched] = new (std::nothrow) CDisplayAttributeInfo();
            if (!rgInfo[fetched]) return E_OUTOFMEMORY;
            ++fetched; ++_index;
        }
        if (pcFetched) *pcFetched = fetched;
        return fetched == ulCount ? S_OK : S_FALSE;
    }
    STDMETHODIMP Reset() override { _index = 0; return S_OK; }
    STDMETHODIMP Skip(ULONG ulCount) override {
        _index += ulCount;
        return _index <= 1 ? S_OK : S_FALSE;
    }
private:
    LONG _cRef;
    ULONG _index;
};

// ---------------------------------------------------------------------------
// CQiangYinTextService
// ---------------------------------------------------------------------------
CQiangYinTextService::CQiangYinTextService() : _keyHandler(this) {
    _keyHandler.SetEngineQuery(&CQiangYinTextService::EngineQueryTrampoline, this);
}

CQiangYinTextService::~CQiangYinTextService() = default;

std::vector<qy::Candidate> CQiangYinTextService::EngineQueryTrampoline(
    const std::string& prefix, void* ctx) {
    auto* self = static_cast<CQiangYinTextService*>(ctx);
    std::vector<qy::Candidate> out;
    if (!self->_engineTried) {
        self->_engineTried = true;
        if (!self->_engine.Load())
            OutputDebugStringW(L"[qiangyin] engine load failed\n");
    }
    if (!self->_engine.IsLoaded()) { QYLOG("query: engine not loaded"); return out; }
    size_t limit = (size_t)self->settings_.candidateCount;
    for (auto& c : self->_engine.Query(prefix, limit))
        out.push_back({c.text, c.code});
    { wchar_t b[128]; wsprintfW(b, L"query prefix=%S -> %u candidates", prefix.c_str(), (unsigned)out.size()); qy::log::Write(b); }
    return out;
}

// ---- IUnknown ----
STDMETHODIMP CQiangYinTextService::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppv = static_cast<ITfTextInputProcessor*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppv = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppv = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppv = static_cast<ITfCompositionSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
        *ppv = static_cast<ITfDisplayAttributeProvider*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompartmentEventSink)) {
        *ppv = static_cast<ITfCompartmentEventSink*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CQiangYinTextService::AddRef() {
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CQiangYinTextService::Release() {
    LONG c = InterlockedDecrement(&_cRef);
    if (c == 0) delete this;
    return c;
}

// ---- ITfTextInputProcessor ----
STDMETHODIMP CQiangYinTextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    QYLOG("Activate called");
    _pThreadMgr = ptim;
    _pThreadMgr->AddRef();
    _tfClientId = tid;

    // 用户设置：注入 KeyHandler（指针，改设置原地生效）；默认中文模式在此应用
    settings_.Load();
    _keyHandler.SetSettings(&settings_);
    _keyHandler.set_full_shape(settings_.fullShape);
    if (settings_.alwaysChinese) _keyHandler.set_chinese_mode(true);

    ITfSource* pSource = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource,
                                              reinterpret_cast<void**>(&pSource)))) {
        pSource->AdviseSink(IID_ITfThreadMgrEventSink,
                            static_cast<ITfThreadMgrEventSink*>(this),
                            &_dwThreadMgrSinkCookie);
        pSource->Release();
    }

    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                              reinterpret_cast<void**>(&pKeystrokeMgr)))) {
        pKeystrokeMgr->AdviseKeyEventSink(tid,
            static_cast<ITfKeyEventSink*>(this), TRUE);
        pKeystrokeMgr->Release();
    }

    // 注册分类：键盘输入法 + 显示属性提供者（组字下划线），并取显示属性 GUID 的 atom
    // GUID_TFCAT_TIP_KEYBOARD 让任务栏识别这是一个键盘输入法，否则任务栏不显示
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
        // 声明“输入模式 compartment”能力：任务栏中/英指示需要（weasel/mozc 都注册）
        pCat->RegisterCategory(CLSID_QiangYinTextService,
                               GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
                               CLSID_QiangYinTextService);
        pCat->RegisterGUID(GUID_QiangYinDisplayAttr, &_gaDisplayAttr);
        pCat->Release();
    }

    // 中/英转换模式 compartment：任务栏徽标的数据源
    // TF_CONVERSIONMODE_NATIVE=1 显示"中"，TF_CONVERSIONMODE_ALPHANUMERIC 显示"英"
    ITfCompartmentMgr* pCompMgr = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfCompartmentMgr,
                                              reinterpret_cast<void**>(&pCompMgr)))) {
        if (SUCCEEDED(pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                                               &_pConvCompartment)) && _pConvCompartment) {
            SetConversionModeCompartment(_keyHandler.chinese_mode());
            // 订阅：用户在任务栏点中/英时 OnChange 被回调
            ITfSource* pConvSrc = nullptr;
            if (SUCCEEDED(_pConvCompartment->QueryInterface(IID_ITfSource,
                                                           reinterpret_cast<void**>(&pConvSrc)))) {
                pConvSrc->AdviseSink(IID_ITfCompartmentEventSink,
                                     static_cast<ITfCompartmentEventSink*>(this),
                                     &_dwConvSinkCookie);
                pConvSrc->Release();
            }
        }
        pCompMgr->Release();
    }

    // 任务栏中/英指示：添加 GUID_LBI_INPUTMODE 语言栏项（显示靠它，不是 compartment）
    InitLangBar();

    // 取当前顶层 context
    ITfDocumentMgr* pDocMgr = nullptr;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr) {
        pDocMgr->GetTop(&_pContext);
        pDocMgr->Release();
    }
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::Deactivate() {
    QYLOG("Deactivate called");
    HideCandidates();
    _keyHandler.Reset();

    // 先移除任务栏中/英指示项（要用 _pThreadMgr QI 出来的 mgr）
    UninitLangBar();

    // 解除转换模式 compartment 订阅
    if (_pConvCompartment) {
        ITfSource* pConvSrc = nullptr;
        if (SUCCEEDED(_pConvCompartment->QueryInterface(IID_ITfSource,
                                                       reinterpret_cast<void**>(&pConvSrc)))) {
            if (_dwConvSinkCookie != TF_INVALID_COOKIE)
                pConvSrc->UnadviseSink(_dwConvSinkCookie);
            pConvSrc->Release();
        }
        _pConvCompartment->Release();
        _pConvCompartment = nullptr;
        _dwConvSinkCookie = TF_INVALID_COOKIE;
    }

    if (_pThreadMgr) {
        ITfSource* pSource = nullptr;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource,
                                                  reinterpret_cast<void**>(&pSource)))) {
            if (_dwThreadMgrSinkCookie != TF_INVALID_COOKIE)
                pSource->UnadviseSink(_dwThreadMgrSinkCookie);
            pSource->Release();
        }
        ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                                  reinterpret_cast<void**>(&pKeystrokeMgr)))) {
            pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
            pKeystrokeMgr->Release();
        }
    }

    if (_pComposition) { _pComposition->Release(); _pComposition = nullptr; }
    if (_pContext) { _pContext->Release(); _pContext = nullptr; }
    if (_pThreadMgr) { _pThreadMgr->Release(); _pThreadMgr = nullptr; }
    _tfClientId = TF_CLIENTID_NULL;
    _dwThreadMgrSinkCookie = TF_INVALID_COOKIE;
    _composing = false;
    _engine.Unload();
    _engineTried = false;
    return S_OK;
}

// ---- ITfThreadMgrEventSink ----
STDMETHODIMP CQiangYinTextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP CQiangYinTextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }

STDMETHODIMP CQiangYinTextService::OnSetFocus(ITfDocumentMgr* pdimFocus,
                                              ITfDocumentMgr*) {
    if (_pContext) { _pContext->Release(); _pContext = nullptr; }
    if (pdimFocus) pdimFocus->GetTop(&_pContext);
    // 焦点切换：只有在没有活动组字时才丢弃状态。
    // 组字中的焦点通知（如候选窗相关）不应中断输入；
    // 真正的上下文切换会走 OnCompositionTerminated 做清理。
    if (!_composing) {
        _keyHandler.Reset();
        HideCandidates();
    }
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::OnPushContext(ITfContext* pic) {
    if (_pContext) _pContext->Release();
    _pContext = pic;
    if (_pContext) _pContext->AddRef();
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::OnPopContext(ITfContext*) {
    if (_pContext) { _pContext->Release(); _pContext = nullptr; }
    _keyHandler.Reset();
    HideCandidates();
    return S_OK;
}

// ---- ITfKeyEventSink ----
STDMETHODIMP CQiangYinTextService::OnSetFocus(BOOL fForeground) {
    // 失去前台时：组字中不丢状态（由 OnCompositionTerminated 清理），
    // 避免候选窗显示期间的焦点抖动把输入灭掉。
    if (!fForeground && !_composing) {
        _keyHandler.Reset();
        HideCandidates();
    }
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::OnTestKeyDown(ITfContext*, WPARAM wParam,
                                                 LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;
    *pfEaten = _keyHandler.WouldEatKey(static_cast<UINT>(wParam), lParam) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::OnKeyDown(ITfContext*, WPARAM wParam,
                                             LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;
    *pfEaten = _keyHandler.HandleKeyDown(static_cast<UINT>(wParam), lParam) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::OnTestKeyUp(ITfContext*, WPARAM wParam,
                                                LPARAM, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;
    UINT vk = static_cast<UINT>(wParam);
    *pfEaten = (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::OnKeyUp(ITfContext*, WPARAM wParam,
                                           LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;
    *pfEaten = _keyHandler.HandleKeyUp(static_cast<UINT>(wParam), lParam) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CQiangYinTextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* pfEaten) {
    if (!pfEaten) return E_POINTER;
    *pfEaten = FALSE; // v0.1 不用 preserved key
    return S_OK;
}

// ---- ITfCompositionSink ----
STDMETHODIMP CQiangYinTextService::OnCompositionTerminated(TfEditCookie,
                                                           ITfComposition* pComposition) {
    QYLOG("OnCompositionTerminated");
    if (pComposition == _pComposition) {
        _pComposition->Release();
        _pComposition = nullptr;
    }
    _composing = false;
    _keyHandler.Reset();
    HideCandidates();
    return S_OK;
}

// ---- ITfDisplayAttributeProvider ----
STDMETHODIMP CQiangYinTextService::EnumDisplayAttributeInfo(
    IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = new (std::nothrow) CEnumDisplayAttributeInfo();
    return *ppEnum ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CQiangYinTextService::GetDisplayAttributeInfo(
    REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
    if (!ppInfo) return E_POINTER;
    if (!IsEqualGUID(guid, GUID_QiangYinDisplayAttr)) {
        *ppInfo = nullptr;
        return E_INVALIDARG;
    }
    *ppInfo = new (std::nothrow) CDisplayAttributeInfo();
    return *ppInfo ? S_OK : E_OUTOFMEMORY;
}

// ---- IKeyHandlerHost ----
void CQiangYinTextService::OnCompositionStarted() {
    if (!_pContext) return;
    RequestEditSession(TF_ES_SYNC | TF_ES_READWRITE, new (std::nothrow) CStartCompositionSession(this));
}

void CQiangYinTextService::OnCompositionUpdated(const std::string& bufferUtf8) {
    if (!_pContext) return;
    RequestEditSession(TF_ES_SYNC | TF_ES_READWRITE,
        new (std::nothrow) CUpdateCompositionSession(this, Utf8ToWide(bufferUtf8)));
}

void CQiangYinTextService::OnCommitText(const std::string& textUtf8) {
    if (!_pContext || textUtf8.empty()) {
        HideCandidates();
        return;
    }
    RequestEditSession(TF_ES_SYNC | TF_ES_READWRITE,
        new (std::nothrow) CCommitSession(this, Utf8ToWide(textUtf8)));
}

void CQiangYinTextService::OnCancelComposition() {
    if (!_pContext) return;
    RequestEditSession(TF_ES_SYNC | TF_ES_READWRITE,
        new (std::nothrow) CCancelSession(this));
}

void CQiangYinTextService::OnModeChanged(bool chinese) {
    wchar_t buf[64];
    wsprintfW(buf, L"[qiangyin] mode=%s\n", chinese ? L"CN" : L"EN");
    OutputDebugStringW(buf);
    // Shift 切换后同步到系统 compartment（数据源）
    SetConversionModeCompartment(chinese);
    // 任务栏中/英图标跟着变（通知系统重取 GetIcon）
    if (_pLangBarItem) _pLangBarItem->SetChineseMode(chinese);
}

void CQiangYinTextService::OnLangBarToggleMode() {
    _keyHandler.SetChineseMode(!_keyHandler.chinese_mode());
}

// Shift+空格切换全/半角：持久化（KeyHandler 自己持有 fullShape_ 状态）
void CQiangYinTextService::OnFullShapeChanged(bool full) {
    settings_.fullShape = full;
    settings_.Save();
    wchar_t b[64];
    wsprintfW(b, L"fullShape=%d saved", full ? 1 : 0);
    qy::log::Write(b);
}

void CQiangYinTextService::OnPunctChanged(bool chinesePunct) {
    settings_.chinesePunct = chinesePunct;
    settings_.Save();
    wchar_t b[64];
    wsprintfW(b, L"punct=%d saved", chinesePunct ? 1 : 0);
    qy::log::Write(b);
}

void CQiangYinTextService::OnLangBarTogglePunct() {
    OnPunctChanged(!settings_.chinesePunct);
}

void CQiangYinTextService::OnLangBarToggleFullShape() {
    bool nv = !_keyHandler.full_shape();
    _keyHandler.set_full_shape(nv);
    settings_.fullShape = nv;
    settings_.Save();
}

bool CQiangYinTextService::LangBarIsChinesePunct() const {
    return settings_.chinesePunct;
}

bool CQiangYinTextService::LangBarIsFullShape() const {
    return _keyHandler.full_shape();
}

// 右键菜单：打开用户词库（没有就创建带说明的模板），保存后输入法自动热重载
void CQiangYinTextService::OnLangBarOpenUserDict() {
    if (!Engine::EnsureUserTable()) { QYLOG("open user dict: ensure failed"); return; }
    std::wstring path = Engine::UserTablePath();
    HINSTANCE h = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr,
                                SW_SHOWNORMAL);
    wchar_t b[128];
    wsprintfW(b, L"open user dict: %s (hInst>32=%d)", path.c_str(),
              (int)(reinterpret_cast<INT_PTR>(h) > 32));
    qy::log::Write(b);
}

// 任务栏图标右键菜单 → 设置对话框（模态，运行在前台应用进程里）
void CQiangYinTextService::OnLangBarOpenSettings() {
    Settings s = settings_;
    if (ShowSettingsDialog(GetFocus(), s)) {
        settings_ = s; // KeyHandler 持有的是本成员的指针，原地生效
        _keyHandler.set_full_shape(settings_.fullShape);
        if (settings_.alwaysChinese && !_keyHandler.chinese_mode())
            _keyHandler.SetChineseMode(true);
        QYLOG("settings saved");
    }
}

// 添加任务栏中/英指示项：item GUID 必须是 GUID_LBI_INPUTMODE，
// 否则系统只当普通语言栏按钮，不会显示在任务栏输入指示器里。
void CQiangYinTextService::InitLangBar() {
    if (_pLangBarItem) return;
    ITfLangBarItemMgr* pMgr = nullptr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr,
                                           reinterpret_cast<void**>(&pMgr))) || !pMgr) {
        QYLOG("InitLangBar: no ITfLangBarItemMgr");
        return;
    }
    _pLangBarItem = new (std::nothrow) CQiangYinLangBarItem(
        static_cast<ILangBarHost*>(this), _keyHandler.chinese_mode());
    if (!_pLangBarItem) { pMgr->Release(); return; }
    HRESULT hr = pMgr->AddItem(_pLangBarItem);
    wchar_t b[64];
    wsprintfW(b, L"InitLangBar AddItem hr=0x%08X", (DWORD)hr);
    qy::log::Write(b);
    pMgr->Release();
}

void CQiangYinTextService::UninitLangBar() {
    if (!_pLangBarItem) return;
    if (_pThreadMgr) {
        ITfLangBarItemMgr* pMgr = nullptr;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr,
                                                  reinterpret_cast<void**>(&pMgr))) && pMgr) {
            pMgr->RemoveItem(_pLangBarItem);
            pMgr->Release();
        }
    }
    _pLangBarItem->Release(); // mgr 持有引用已随 RemoveItem 释放
    _pLangBarItem = nullptr;
}

// 把中英状态写入系统转换模式 compartment（任务栏中/英徽标的数据源）
void CQiangYinTextService::SetConversionModeCompartment(bool chinese) {
    if (!_pConvCompartment) return;
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    // 中文：TF_CONVERSIONMODE_NATIVE；英文：TF_CONVERSIONMODE_ALPHANUMERIC
    v.lVal = chinese ? TF_CONVERSIONMODE_NATIVE : TF_CONVERSIONMODE_ALPHANUMERIC;
    _pConvCompartment->SetValue(_tfClientId, &v);
    wchar_t b[64];
    wsprintfW(b, L"SetConversionMode chinese=%d", chinese ? 1 : 0);
    qy::log::Write(b);
}

// ITfCompartmentEventSink：用户在任务栏点了中/英徽标，同步回内部状态
STDMETHODIMP CQiangYinTextService::OnChange(REFGUID rguid) {
    if (!IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION))
        return S_OK;
    if (!_pConvCompartment) return S_OK;
    VARIANT v;
    VariantInit(&v);
    // 注意：GetValue 只取 1 个参数（VARIANT*），SetValue 才要传 tid
    if (SUCCEEDED(_pConvCompartment->GetValue(&v)) && v.vt == VT_I4) {
        bool chinese = (v.lVal & TF_CONVERSIONMODE_NATIVE) != 0;
        // SetChineseMode 内部会处理组字中先上屏；OnModeChanged 回写 compartment 是等幂的
        _keyHandler.SetChineseMode(chinese);
        wchar_t b[64];
        wsprintfW(b, L"OnChange conversionmode chinese=%d", chinese ? 1 : 0);
        qy::log::Write(b);
    }
    VariantClear(&v);
    return S_OK;
}

HRESULT CQiangYinTextService::RequestEditSession(DWORD flags, ITfEditSession* session) {
    if (!_pContext || !session) {
        if (session) session->Release();
        return E_FAIL;
    }
    HRESULT hrSession = S_OK;
    HRESULT hr = _pContext->RequestEditSession(_tfClientId, session, flags, &hrSession);
    session->Release();
    return SUCCEEDED(hr) ? hrSession : hr;
}

// ---- edit session 实际工作 ----
HRESULT CQiangYinTextService::OnEditStartComposition(TfEditCookie ec) {
    { wchar_t b[128]; wsprintfW(b, L"OnEditStartComposition _composing=%d _pComp=%p", _composing ? 1 : 0, _pComposition); qy::log::Write(b); }
    if (_composing || !_pContext) return S_OK;

    ITfRange* pRange = nullptr;
    TF_SELECTION sel = {};
    ULONG fetched = 0;
    HRESULT hr = _pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched);
    if (FAILED(hr) || fetched == 0) return S_OK;
    pRange = sel.range;
    pRange->Collapse(ec, TF_ANCHOR_START);

    ITfContextComposition* pCC = nullptr;
    hr = _pContext->QueryInterface(IID_ITfContextComposition,
                                   reinterpret_cast<void**>(&pCC));
    if (SUCCEEDED(hr)) {
        hr = pCC->StartComposition(ec, pRange,
            static_cast<ITfCompositionSink*>(this), &_pComposition);
        pCC->Release();
    }
    pRange->Release();
    if (SUCCEEDED(hr) && _pComposition) _composing = true;
    return S_OK;
}

static void SetRangeAttribute(TfEditCookie ec, ITfContext* pic, ITfRange* range,
                              TfGuidAtom gaDisplayAttr) {
    // 给组字区间加上我们的下划线显示属性（写法参照 rime/weasel）：
    // GUID_PROP_ATTRIBUTE 属性的值就是显示属性 GUID 的 TfGuidAtom，
    // TSF 看到后会回调我们的 ITfDisplayAttributeProvider 取具体样式。
    if (gaDisplayAttr == TF_INVALID_GUIDATOM) return;
    ITfProperty* pProp = nullptr;
    if (SUCCEEDED(pic->GetProperty(GUID_PROP_ATTRIBUTE, &pProp)) && pProp) {
        VARIANT var;
        var.vt = VT_I4; // TfGuidAtom
        var.lVal = (LONG)gaDisplayAttr;
        pProp->SetValue(ec, range, &var);
        pProp->Release();
    }
}

HRESULT CQiangYinTextService::OnEditUpdateComposition(TfEditCookie ec,
                                                      const std::wstring& text) {
    if (!_composing || !_pComposition || !_pContext) return S_OK;

    ITfRange* pRange = nullptr;
    if (FAILED(_pComposition->GetRange(&pRange)) || !pRange) return S_OK;

    pRange->SetText(ec, 0, text.c_str(), (LONG)text.size());
    SetRangeAttribute(ec, _pContext, pRange, _gaDisplayAttr);

    // 光标移到组字末尾
    pRange->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION sel = {};
    sel.range = pRange;
    sel.style.ase = TF_AE_NONE;
    sel.style.fInterimChar = FALSE;
    _pContext->SetSelection(ec, 1, &sel);

    UpdateCandidateWindow(ec, pRange);
    pRange->Release();
    return S_OK;
}

HRESULT CQiangYinTextService::OnEditCommit(TfEditCookie ec, const std::wstring& text) {
    if (!_pContext) return S_OK;
    if (_composing && _pComposition) {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(_pComposition->GetRange(&pRange)) && pRange) {
            pRange->SetText(ec, 0, text.c_str(), (LONG)text.size());
            // 光标移到提交文本末尾
            pRange->Collapse(ec, TF_ANCHOR_END);
            TF_SELECTION sel = {};
            sel.range = pRange;
            sel.style.ase = TF_AE_NONE;
            sel.style.fInterimChar = FALSE;
            _pContext->SetSelection(ec, 1, &sel);
            pRange->Release();
        }
        _pComposition->EndComposition(ec);
        // 防御：OnCompositionTerminated 可能不被触发（如 sink 未正确注册），
        // 手动清理状态，避免 _composing 卡住导致后续无法输入。
        // OnCompositionTerminated 里的清理是幂等的，重复调用无害。
        if (_composing) {
            QYLOG("OnEditCommit: manual cleanup after EndComposition");
            _composing = false;
            if (_pComposition) { _pComposition->Release(); _pComposition = nullptr; }
            _keyHandler.Reset();
        }
    } else {
        // 无组字：直接在光标处插入（标点等）
        ITfRange* pRange = nullptr;
        TF_SELECTION sel = {};
        ULONG fetched = 0;
        if (SUCCEEDED(_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched))
            && fetched > 0) {
            pRange = sel.range;
            pRange->Collapse(ec, TF_ANCHOR_START);
            pRange->SetText(ec, 0, text.c_str(), (LONG)text.size());
            pRange->Collapse(ec, TF_ANCHOR_END);
            TF_SELECTION sel2 = {};
            sel2.range = pRange;
            sel2.style.ase = TF_AE_NONE;
            sel2.style.fInterimChar = FALSE;
            _pContext->SetSelection(ec, 1, &sel2);
            pRange->Release();
        }
    }
    HideCandidates();
    return S_OK;
}

HRESULT CQiangYinTextService::OnEditCancel(TfEditCookie ec) {
    if (_composing && _pComposition) {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(_pComposition->GetRange(&pRange)) && pRange) {
            pRange->SetText(ec, 0, L"", 0); // 清掉组字残留
            pRange->Release();
        }
        _pComposition->EndComposition(ec);
        // 防御：同 OnEditCommit，手动清理防卡住
        if (_composing) {
            _composing = false;
            if (_pComposition) { _pComposition->Release(); _pComposition = nullptr; }
            _keyHandler.Reset();
        }
    }
    HideCandidates();
    return S_OK;
}

void CQiangYinTextService::UpdateCandidateWindow(TfEditCookie ec, ITfRange* range) {
    auto items = CurrentCandidates();
    if (items.empty()) { HideCandidates(); return; }

    // 取组字区的屏幕坐标，把候选窗放下面
    int x = 0, y = 0;
    bool placed = false;
    ITfContextView* pView = nullptr;
    if (SUCCEEDED(_pContext->GetActiveView(&pView)) && pView) {
        RECT rc = {};
        BOOL clipped = FALSE;
        if (SUCCEEDED(pView->GetTextExt(ec, range, &rc, &clipped))) {
            x = rc.left; y = rc.bottom + 2;
            placed = true;
        }
        pView->Release();
    }
    if (!placed) {
        // GetCaretPos 返回客户区坐标，WS_POPUP 需要屏幕坐标，必须转换
        POINT pt = {};
        GetCaretPos(&pt);
        HWND hwndFocus = GetFocus();
        if (hwndFocus) ClientToScreen(hwndFocus, &pt);
        x = pt.x; y = pt.y + 20;
        // 兜底：如果还是拿不到有效坐标，放屏幕中央偏下
        if (x <= 0 && y <= 0) {
            x = GetSystemMetrics(SM_CXSCREEN) / 2 - 100;
            y = GetSystemMetrics(SM_CYSCREEN) / 2 + 100;
        }
    }
    if (_candWnd.IsVisible()) {
        _candWnd.Update(items);
        QYLOG("cand window Update");
    } else {
        _candWnd.ShowAt(x, y, items);
        wchar_t b[128]; wsprintfW(b, L"cand window ShowAt x=%d y=%d n=%u", x, y, (unsigned)items.size());
        qy::log::Write(b);
    }
}

// 查当前组字缓冲的候选（文本 + 编码提示）。
// hint = 该候选完整编码去掉已输入前缀后的剩余部分：
// 输入 t 时"ta 凸"的提示是 a——照着提示继续打就能上屏该字。
std::vector<CandidateItem> CQiangYinTextService::CurrentCandidates() {
    std::vector<CandidateItem> out;
    const std::string& buf = _keyHandler.buffer();
    if (buf.empty()) return out;
    if (!_engineTried) {
        _engineTried = true;
        if (!_engine.Load())
            OutputDebugStringW(L"[qiangyin] engine load failed\n");
    }
    if (!_engine.IsLoaded()) return out;
    for (auto& c : _engine.Query(buf, (size_t)settings_.candidateCount)) {
        CandidateItem it;
        it.text = Utf8ToWide(c.text);
        if (settings_.showHint && c.code.size() > buf.size())
            it.hint = Utf8ToWide(c.code.substr(buf.size()));
        out.push_back(std::move(it));
    }
    return out;
}

void CQiangYinTextService::HideCandidates() {
    _candWnd.Hide();
}

} // namespace qy
