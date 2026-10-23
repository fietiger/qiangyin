// 强音 TSF 文本服务实现
#include <new> // std::nothrow
#include <utility> // std::move
#include <objbase.h> // CoCreateInstance（显式包含，不依赖 msctf.h 的传递）
#include <oleauto.h> // SysAllocString, VARIANT
#include "TextService.h"
#include "Guids.h"
#include "QyLog.h"
#include "Utf.h"
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
    for (auto& c : self->_engine.Query(prefix, 9))
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
        pCat->RegisterGUID(GUID_QiangYinDisplayAttr, &_gaDisplayAttr);
        pCat->Release();
    }

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
        _pComposition->EndComposition(ec); // 会触发 OnCompositionTerminated
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
    }
    HideCandidates();
    return S_OK;
}

void CQiangYinTextService::UpdateCandidateWindow(TfEditCookie ec, ITfRange* range) {
    auto texts = CurrentCandidateTexts();
    if (texts.empty()) { HideCandidates(); return; }

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
        POINT pt = {};
        GetCaretPos(&pt);
        x = pt.x; y = pt.y + 20;
    }
    if (_candWnd.IsVisible()) {
        _candWnd.Update(texts);
        QYLOG("cand window Update");
    } else {
        _candWnd.ShowAt(x, y, texts);
        wchar_t b[128]; wsprintfW(b, L"cand window ShowAt x=%d y=%d n=%u", x, y, (unsigned)texts.size());
        qy::log::Write(b);
    }
}

std::vector<std::wstring> CQiangYinTextService::CurrentCandidateTexts() {
    std::vector<std::wstring> out;
    const std::string& buf = _keyHandler.buffer();
    if (buf.empty()) return out;
    if (!_engineTried) {
        _engineTried = true;
        if (!_engine.Load())
            OutputDebugStringW(L"[qiangyin] engine load failed\n");
    }
    if (!_engine.IsLoaded()) return out;
    for (auto& c : _engine.Query(buf, 9))
        out.push_back(Utf8ToWide(c.text));
    return out;
}

void CQiangYinTextService::HideCandidates() {
    _candWnd.Hide();
}

} // namespace qy
