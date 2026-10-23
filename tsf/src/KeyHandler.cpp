// 按键状态机实现
#include "KeyHandler.h"
#include <windows.h>

namespace qy {

KeyHandler::KeyHandler(IKeyHandlerHost* host): host_(host) {}

void KeyHandler::Reset() {
buffer_.clear();
shiftDown_ = false;
shiftUsed_ = false;
}

bool KeyHandler::HasCtrlAlt() const {
return (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000);
}

std::vector<Candidate> KeyHandler::QueryCandidates() const {
if (query_ &&!buffer_.empty()) return query_(buffer_, queryCtx_);
return {};
}

void KeyHandler::CommitTopCandidate() {
auto cands = QueryCandidates();
if (!cands.empty())
host_->OnCommitText(cands[0].text);
else
host_->OnCommitText(buffer_); // 无候选：原样上屏（极少见）
buffer_.clear();
}

void KeyHandler::ToggleMode() {
if (composing()) {
// 切换前先把组字上屏，避免中英文切换丢组字——这正是原 bug 的场景
CommitTopCandidate();
}
chinese_ =!chinese_;
host_->OnModeChanged(chinese_);
}

// 中文标点映射（v0.1 固定表；引号配对等以后再说）
bool KeyHandler::IsPunctKey(UINT vkey, std::string& outUtf8) const {
struct Map { UINT vk; const char* zh;};
static const Map kMap[] = {
{ VK_OEM_COMMA, "\xEF\xBC\x8C"}, // ，
{ VK_OEM_PERIOD, "\xE3\x80\x82"}, // 。
{ VK_OEM_2, "\xE3\x80\x81"}, // 、
{ VK_OEM_1, "\xEF\xBC\x9B"}, // ；
{ VK_OEM_3, "\xEF\xBD\x9E"}, // ～
{ VK_OEM_4, "\xE3\x80\x90"}, //
{ VK_OEM_5, "\xE3\x80\x81"}, // 、
{ VK_OEM_7, "\xE2\x80\x99"}, //’
};
// 注：数字行带 Shift 的标点（!@# 等）v0.1 不处理——Shift 按住期间的按键
// 会被 shiftUsed_ 标记，不会触发点按切换；字符本身放行给程序。
for (const auto& m: kMap) {
if (m.vk == vkey) { outUtf8 = m.zh; return true;}
}
return false;
}

bool KeyHandler::WouldEatKey(UINT vkey, LPARAM /*lParam*/) const {
if (HasCtrlAlt()) return false; // 快捷键一律放行

if (vkey == VK_SHIFT || vkey == VK_LSHIFT || vkey == VK_RSHIFT)
return true; // Shift 点按用于切换，交由 HandleKeyUp 判定

if (!chinese_) return false; // 英文模式：除了 Shift 都放行

if (vkey >= 'A' && vkey <= 'Z') return true;
if (vkey == VK_BACK) return composing(); // ★ 核心修复：组字中退格必吃
if (vkey == VK_SPACE || vkey == VK_RETURN || vkey == VK_ESCAPE)
return composing();
if (vkey >= '1' && vkey <= '9') return composing();
std::string dummy;
if (IsPunctKey(vkey, dummy)) return true;
return false;
}

bool KeyHandler::HandleKeyDown(UINT vkey, LPARAM lParam) {
if (vkey == VK_SHIFT || vkey == VK_LSHIFT || vkey == VK_RSHIFT) {
if (!shiftDown_) { shiftDown_ = true; shiftUsed_ = false;}
return true; // 吃掉，等 KeyUp 再决定是否切换
}
if (shiftDown_) shiftUsed_ = true; // Shift 按住期间有其他键：不是点按
if (HasCtrlAlt()) return false;

if (!chinese_) return false;

if (vkey >= 'A' && vkey <= 'Z') {
bool start = buffer_.empty();
buffer_ += static_cast<char>(vkey - 'A' + 'a');
if (start) host_->OnCompositionStarted();
else host_->OnCompositionUpdated(buffer_);
return true;
}

if (vkey == VK_BACK) {
// ★ 核心修复：组字缓冲非空时，退格只删缓冲里的字符，
// 必须吃掉，绝不能漏到输入框（原 bug：把 D 删了）。
if (!composing()) return false;
buffer_.pop_back();
if (buffer_.empty()) host_->OnCancelComposition();
else host_->OnCompositionUpdated(buffer_);
return true;
}

if (vkey == VK_SPACE || vkey == VK_RETURN) {
if (!composing()) return false;
CommitTopCandidate();
return true;
}

if (vkey == VK_ESCAPE) {
if (!composing()) return false;
buffer_.clear();
host_->OnCancelComposition();
return true;
}

if (vkey >= '1' && vkey <= '9') {
if (!composing()) return false;
auto cands = QueryCandidates();
size_t idx = static_cast<size_t>(vkey - '1');
if (idx < cands.size()) {
host_->OnCommitText(cands[idx].text);
buffer_.clear();
}
// 序号越界也吃掉：保持组字状态不乱（行为开关，见 README）
return true;
}

std::string punct;
if (IsPunctKey(vkey, punct)) {
if (composing()) CommitTopCandidate();
host_->OnCommitText(punct);
return true;
}

return false;
}

bool KeyHandler::HandleKeyUp(UINT vkey, LPARAM /*lParam*/) {
if (vkey == VK_SHIFT || vkey == VK_LSHIFT || vkey == VK_RSHIFT) {
bool tap = shiftDown_ &&!shiftUsed_;
shiftDown_ = false;
shiftUsed_ = false;
if (tap) { ToggleMode(); return true;}
}
return false;
}

} // namespace qy
