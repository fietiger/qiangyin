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
    void SetHorizontal(bool horizontal) { horizontal_ = horizontal; }
    // 在屏幕坐标 (x, y) 显示；items 为当前页候选，page/pageCount 为页码（1 起），
    // sel 为页内高亮行/项（0 起，↑/↓ 或 ←/→ 移动；空格上屏高亮项）
    void ShowAt(int x, int y, const std::vector<CandidateItem>& items,
                int page, int pageCount, int sel);
    // 组字更新时重排：x, y 为最新输入点屏幕坐标。高度随候选数自适应；
    // 上方模式下锚定下沿——框体缩小时下移，保持贴近输入点
    void Update(int x, int y, const std::vector<CandidateItem>& items,
                int page, int pageCount, int sel);
    void Hide();
    bool IsVisible() const;

private:
    struct HorizSlot {
        int x = 0;
        int y = 0;
        int w = 0;
        int numW = 0;
        int textW = 0;
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void OnPaint();
    void RecalcSize(int maxBarWidth);
    // 计算贴输入点的最终位置：默认在输入点下方；下方放不下时翻到上方并
    // 锚定下沿（框体缩小时下移，近边始终不动）
    void ApplyLayout(int x, int y, const RECT& wa, int& outX, int& outY);
    void UpdateTheme(); // 跟随 Windows 深浅色（每次显示/更新时读一次注册表）

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    std::vector<CandidateItem> items_;
    std::vector<HorizSlot> hSlots_;
    int page_ = 1;        // 当前页（1 起）
    int pageCount_ = 1;   // 总页数
    int sel_ = 0;         // 页内高亮行/项（0 起）
    int lineHeight_ = 24;
    int width_ = 120;
    int h_ = 60;          // 当前高度（随候选数或行数自适应）
    bool above_ = false;  // 当前是否为“输入点上方”模式（下沿锚定）
    bool horizontal_ = false; // false=竖排（默认），true=横排
    int numW_ = 24; // 竖排序号 "N. " 列宽
    int pageW_ = 0; // 页码 "n/m" 宽度
    bool dark_ = false;   // 系统深色模式（Win11 Dark 配色）
};

} // namespace qy
