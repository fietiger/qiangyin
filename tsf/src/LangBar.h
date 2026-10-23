#pragma once
// 任务栏 中/英 状态指示（语言栏 input mode 项）。
// 机制（与 rime/weasel、Google mozc、PIME 相同）：
//   向 ITfLangBarItemMgr 添加一个 item GUID = GUID_LBI_INPUTMODE 的
//   ITfLangBarItemButton，Win8+ 任务栏输入指示器会渲染它的 GetIcon；
//   中英切换时通过 ITfLangBarItemSink::OnUpdate(TF_LBI_ICON) 通知刷新。
//   GUID_LBI_INPUTMODE 的值各 SDK 头文件里都没有，需自己定义
//   （见 Guids.cpp 的 GUID_QY_LBI_INPUTMODE）。
#include <windows.h>
#include "TsfCompat.h" // msctf.h + ctfutb.h + mingw 补齐

namespace qy {

// 语言栏按钮回调宿主（非 COM，纯 C++ 接口）
class ILangBarHost {
public:
    virtual void OnLangBarToggleMode() = 0; // 任务栏图标左键：切换中/英
protected:
    ~ILangBarHost() = default;
};

class CQiangYinLangBarItem : public ITfLangBarItemButton, public ITfSource {
public:
    CQiangYinLangBarItem(ILangBarHost* host, bool chinese);
    ~CQiangYinLangBarItem();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfLangBarItem
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo) override;
    STDMETHODIMP GetStatus(DWORD* pdwStatus) override;
    STDMETHODIMP Show(BOOL fShow) override;
    STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip) override;

    // ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) override;
    STDMETHODIMP InitMenu(ITfMenu* pMenu) override;
    STDMETHODIMP OnMenuSelect(UINT wID) override;
    STDMETHODIMP GetIcon(HICON* phIcon) override;
    STDMETHODIMP GetText(BSTR* pbstrText) override;

    // ITfSource（系统 AdviseSink 进来拿 ITfLangBarItemSink，用于 OnUpdate 通知）
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) override;
    STDMETHODIMP UnadviseSink(DWORD dwCookie) override;

    // 中英状态变化：记住并通知任务栏重取图标
    void SetChineseMode(bool chinese);

private:
    LONG _cRef;
    ILangBarHost* _host;      // 弱引用（属主是 TextService）
    bool _chinese;
    HICON _hIconZh = nullptr; // “中”
    HICON _hIconEn = nullptr; // “英”
    ITfLangBarItemSink* _pSink = nullptr;
    DWORD _status = 0;

    void NotifyIconChanged();
};

} // namespace qy
