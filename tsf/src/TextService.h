#pragma once
// 强音输入法 TSF 文本服务主类。
// 实现：ITfTextInputProcessor / ITfThreadMgrEventSink / ITfKeyEventSink /
//       ITfCompositionSink / ITfDisplayAttributeProvider / IKeyHandlerHost
// 不用 ATL，手写 COM（保证 mingw 也能编译检查）。
#include <windows.h>
#include "TsfCompat.h" // msctf.h + ctfutb.h + mingw 缺失声明的补齐
#include <string>
#include <vector>
#include "KeyHandler.h"
#include "Engine.h"
#include "CandidateWindow.h"
#include "LangBar.h"
#include "Settings.h"

namespace qy {

class CQiangYinTextService :
    public ITfTextInputProcessor,
    public ITfThreadMgrEventSink,
    public ITfKeyEventSink,
    public ITfCompositionSink,
    public ITfDisplayAttributeProvider,
    public ITfCompartmentEventSink,
    public IKeyHandlerHost,
    public ILangBarHost
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

    // ITfCompartmentEventSink（任务栏点中/英时系统回调）
    STDMETHODIMP OnChange(REFGUID rguid) override;

    // IKeyHandlerHost（被 KeyHandler 回调；内部再开 edit session 落到 TSF）
    void OnCompositionStarted() override;
    void OnCompositionUpdated(const std::string& bufferUtf8) override;
    void OnCommitText(const std::string& textUtf8) override;
    void OnCancelComposition() override;
    void OnModeChanged(bool chinese) override;
    void OnFullShapeChanged(bool full) override; // Shift+空格切换全/半角
    void OnPunctChanged(bool chinesePunct) override; // Ctrl+. 切标点

    // ILangBarHost（任务栏中/英图标交互）
    void OnLangBarToggleMode() override;
    void OnLangBarOpenSettings() override;
    void OnLangBarTogglePunct() override;        // 右键菜单：中文标点
    void OnLangBarToggleFullShape() override;    // 右键菜单：全角模式
    void OnLangBarOpenUserDict() override;       // 右键菜单：打开用户词库
    bool LangBarIsChinesePunct() const override;
    bool LangBarIsFullShape() const override;

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
    void InitLangBar();   // Activate：添加任务栏中/英指示项
    void UninitLangBar(); // Deactivate：移除

    static std::vector<qy::Candidate> EngineQueryTrampoline(const std::string& prefix, void* ctx);

    LONG _cRef = 1;
    ITfThreadMgr* _pThreadMgr = nullptr;
    TfClientId _tfClientId = TF_CLIENTID_NULL;
    ITfContext* _pContext = nullptr;
    ITfComposition* _pComposition = nullptr;
    bool _composing = false;

    DWORD _dwThreadMgrSinkCookie = TF_INVALID_COOKIE;
    // 中/英转换模式 compartment（状态数据源，供系统组件读取）
    ITfCompartment* _pConvCompartment = nullptr;
    DWORD _dwConvSinkCookie = TF_INVALID_COOKIE;
    void SetConversionModeCompartment(bool chinese);
    // 任务栏中/英指示（GUID_LBI_INPUTMODE 语言栏项，图标真正显示靠它）
    CQiangYinLangBarItem* _pLangBarItem = nullptr;
    Settings settings_; // 用户设置（Activate 时加载，KeyHandler 持有指针）
TfGuidAtom _gaDisplayAttr = TF_INVALID_GUIDATOM;

    KeyHandler _keyHandler;
    Engine _engine;
    bool _engineTried = false;
    CandidateWindow _candWnd;
};

} // namespace qy
