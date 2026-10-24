#pragma once
// 按键状态机：纯逻辑，不依赖 TSF，可单独单元测试。
//
// 行为（v0.1，对标超强音形）：
// - 中文模式：a-z 进入组字；Backspace 回退组字缓冲（*关键修复*：组字中
//   Backspace 永远被输入法吃掉，绝不下发给程序）；Space/Enter 上屏首选；
//   Esc 取消组字；1-9 选重；Shift 点按切换中/英文；标点自动转全角。
// - 英文模式：只处理 Shift 切换，其余按键全部放行。
// - 任何带 Ctrl/Alt 的组合键一律放行（不抢快捷键）。
#include <windows.h>
#include <string>
#include <vector>
#include "Candidate.h"
#include "Settings.h"

namespace qy {

// TextService 实现此接口，把组字/上屏等动作落到 TSF。
class IKeyHandlerHost {
public:
    virtual ~IKeyHandlerHost() = default;
    virtual void OnCompositionStarted() = 0;
    virtual void OnCompositionUpdated(const std::string& buffer) = 0; // buffer 变化
    virtual void OnCommitText(const std::string& text) = 0;       // 上屏
    virtual void OnCancelComposition() = 0;
    virtual void OnModeChanged(bool chinese) = 0;
    virtual void OnFullShapeChanged(bool full) = 0;               // 全/半角切换（宿主负责持久化）
    virtual void OnPunctChanged(bool chinesePunct) = 0;           // Ctrl+. 切标点（宿主负责持久化）
    virtual void OnOpenAddWord() = 0;                             // Ctrl+'+' 在线造词
};

class KeyHandler {
public:
    explicit KeyHandler(IKeyHandlerHost* host);

    // 注入用户设置（属主是 TextService，Activate 时设置；空指针=默认行为，供单测）
    void SetSettings(const Settings* s) { settings_ = s; }

    // OnTestKeyDown 用：只判断吃不吃，不改变状态。必须与 HandleKeyDown 的
    // 吃/放行决策一致（否则 TSF 会行为异常）。
    bool WouldEatKey(UINT vkey, LPARAM lParam) const;
    // OnKeyDown 用：执行动作，返回是否吃掉。
    bool HandleKeyDown(UINT vkey, LPARAM lParam);
    // OnKeyUp 用：主要用于 Shift 点按检测。
    bool HandleKeyUp(UINT vkey, LPARAM lParam);

    void Reset(); // 焦点切换/强制重置：清空组字缓冲等临时状态
    bool chinese_mode() const { return chinese_; }
    void set_chinese_mode(bool chinese) { chinese_ = chinese; } // 任务栏点中/英时同步
    void SetChineseMode(bool chinese); // 对外切换：组字中先上屏再切（供任务栏回调）
    bool full_shape() const { return fullShape_; }
    void set_full_shape(bool full) { fullShape_ = full; }       // Activate/设置对话框/任务栏菜单同步
    const std::string& buffer() const { return buffer_; }
    bool composing() const { return !buffer_.empty(); }

    // 候选翻页（-/= 与 ↑/↓）。查询上限：深翻页封顶（v 开头 161 条全够用）
    static constexpr size_t kMaxCandidates = 500;
    size_t PageSize() const;             // 每页个数 = 设置的候选数量
    int PageCount() const;               // 总页数（按查询上限截断）
    void PageMove(int delta);            // 翻页（边界夹紧，不环绕）
    // 当前页候选切片；totalOut（可空）返回候选总数
    std::vector<Candidate> CurrentPage(size_t* totalOut = nullptr) const;
    int page() const { return page_; }

    void SetEngineQuery(std::vector<Candidate> (*query)(const std::string&, void*),
                        void* ctx) {
        query_ = query; queryCtx_ = ctx;
    }

private:
    bool HasCtrlAlt() const;
    bool IsPunctKey(UINT vkey, std::string& outUtf8) const;
    std::vector<Candidate> QueryCandidates() const;
    void CommitTopCandidate(); // 上屏首选（无候选则上屏 buffer 原样）
    void FlushCompositionEnterLike(); // 组字被打断时按 Enter 三态收尾（大写直出等复用）
    void ToggleMode();

    // 设置读取（空指针回落默认，保持可单测）
    bool AutoCommitFull() const { return !settings_ || settings_->autoCommitFull; }
    int  EnterMode() const { return settings_ ? settings_->enterMode : 0; }   // 0清除/1首选/2编码
    int  SwitchMode() const { return settings_ ? settings_->switchMode : 0; } // 0清空/1首选/2编码
    bool ChinesePunct() const { return !settings_ || settings_->chinesePunct; }
    bool EngAutoHalfWidth() const { return !settings_ || settings_->engAutoHalfWidth; }
    bool EngAutoEnPunct() const { return !settings_ || settings_->engAutoEnPunct; }
    bool CtrlOnly() const; // 仅 Ctrl 按住（无 Shift/Alt）——Ctrl+. 切标点用
    bool CtrlHeld() const; // Ctrl 按住（允许 Shift，无 Alt）——Ctrl+'+' 造词用

    IKeyHandlerHost* host_;
    const Settings* settings_ = nullptr;
    bool chinese_ = true;      // 默认中文模式
    bool fullShape_ = false;   // 全角模式：直通类按键输出全角（编码输入不受影响）
    int page_ = 0;             // 候选当前页（0 起；缓冲变化时归零）
    std::string buffer_;       // 组字缓冲：a-z
    bool shiftDown_ = false;   // Shift 是否按下（用于点按检测）
    bool shiftUsed_ = false;   // Shift 按下期间有无其他按键

    std::vector<Candidate> (*query_)(const std::string&, void*) = nullptr;
    void* queryCtx_ = nullptr;
};

} // namespace qy
