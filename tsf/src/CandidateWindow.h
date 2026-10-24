#pragma once
// 候选窗口：WS_POPUP + 置顶 + 不抢焦点，显示最多 9 个候选。
// 由 TextService 在组字更新时驱动：ShowAt/Update/Hide。
#include <windows.h>
#include <string>
#include <vector>

namespace qy {

struct CandidateItem {
    std::wstring text; // 候选文本（UTF-16）
    std::wstring hint; // 编码提示：补全该候选还差的编码（可空）
};

class CandidateWindow {
public:
    CandidateWindow();
    ~CandidateWindow();

    CandidateWindow(const CandidateWindow&) = delete;
    CandidateWindow& operator=(const CandidateWindow&) = delete;

    bool Create();
    void Destroy();
    // 在屏幕坐标 (x, y) 显示；items 为当前页候选，page/pageCount 为页码（1 起）
    void ShowAt(int x, int y, const std::vector<CandidateItem>& items, int page, int pageCount);
    void Update(const std::vector<CandidateItem>& items, int page, int pageCount);
    void Hide();
    bool IsVisible() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void OnPaint();
    void RecalcSize();

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    std::vector<CandidateItem> items_;
    int page_ = 1;        // 当前页（1 起）
    int pageCount_ = 1;   // 总页数
    int lineHeight_ = 24;
    int width_ = 120;
    int numW_ = 24; // 序号 "N. " 列宽
    int pageW_ = 0; // 页码 "n/m" 宽度
};

} // namespace qy
