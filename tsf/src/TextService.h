#pragma once
// 强音输入法 TSF 文本服务主类。
// 实现：ITfTextInputProcessor / ITfThreadMgrEventSink / ITfKeyEventSink /
//       ITfCompositionSink / ITfDisplayAttributeProvider / IKeyHandlerHost
// 不用 ATL，手写 COM（保证 mingw 也能编译检查）。
#include <windows.h>
#include "TsfCompat.h" // msctf.h + mingw 缺失声明的补齐
#include <string>
#include <vector>
#include "KeyHandler.h"
#include "Engine.h"
#include "CandidateWindow.h"

namespace qy {

class CQiangYinTextService :
    public ITfTextInputProcessor,
    public ITfThreadMgrEventSink,
    public ITfKeyEventSink,
    public ITfCompositionSink,
    public ITfDisplayAttributeProvider,
    public IKeyHandlerHost
{
public:
    CQiangYinTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
    STDMETHODIMP Deactivate() override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* pdimPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pic) override;
    STDMETHODIMP OnPopContext(ITfContext* pic) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) override;

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) override;

    // IKeyHandlerHost（被 KeyHandler 回调；内部再开 edit session 落到 TSF）
    void OnCompositionStarted() override;
    void OnCompositionUpdated(const std::string& bufferUtf8) override;
    void OnCommitText(const std::string& textUtf8) override;
    void OnCancelComposition() override;
    void OnModeChanged(bool chinese) override;

    // edit session 回调（已有 ec）
    HRESULT OnEditStartComposition(TfEditCookie ec);
    HRESULT OnEditUpdateComposition(TfEditCookie ec, const std::wstring& text);
    HRESULT OnEditCommit(TfEditCookie ec, const std::wstring& text);
    HRESULT OnEditCancel(TfEditCookie ec);

    ITfContext* GetContext() const { return _pContext; }

private:
    ~CQiangYinTextService();
    HRESULT RequestEditSession(DWORD flags, ITfEditSession* session);
    void UpdateCandidateWindow(TfEditCookie ec, ITfRange* range);
    void HideCandidates();
    std::vector<std::wstring> CurrentCandidateTexts();

    static std::vector<qy::Candidate> EngineQueryTrampoline(const std::string& prefix, void* ctx);

    LONG _cRef = 1;
    ITfThreadMgr* _pThreadMgr = nullptr;
    TfClientId _tfClientId = TF_CLIENTID_NULL;
    ITfContext* _pContext = nullptr;
    ITfComposition* _pComposition = nullptr;
    bool _composing = false;

    DWORD _dwThreadMgrSinkCookie = TF_INVALID_COOKIE;

    KeyHandler _keyHandler;
    Engine _engine;
    bool _engineTried = false;
    CandidateWindow _candWnd;
};

} // namespace qy
