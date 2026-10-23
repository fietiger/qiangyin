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
struct Map { UINT vk; const char* zh; const char* zhShift; };
static const Map kMap[] = {
// vk, 无Shift, Shift+该键（按用户配置表 [-非编码符号转义定义表-]）
{ VK_OEM_COMMA, "\xEF\xBC\x8C", "\xE3\x80\x8A\xE3\x80\x88"}, // ，→《〈
{ VK_OEM_PERIOD, "\xE3\x80\x82", "\xE3\x80\x89\xE3\x80\x8A"}, // 。→〉》
{ VK_OEM_2, "\xE3\x80\x81", "\xEF\xBC\x9F"}, // 、→？
{ VK_OEM_1, "\xEF\xBC\x9B", "\xEF\xBC\x9A"}, // ；→：
{ VK_OEM_3, "\xEF\xBD\x9E", "\x7E"}, // ～→~
{ VK_OEM_4, "\xE3\x80\x90", "\x7B"}, // →{
{ VK_OEM_5, "\xE3\x80\x81", "\xE4\xB8\xA8"}, // 、→·
{ VK_OEM_7, "\xE2\x80\x99", "\xE2\x80\x9C"}, // ’→“
{ VK_OEM_MINUS, "\xEF\xBC\x8D", "\xE2\x80\x94\xE2\x80\x94"}, // －→——
{ VK_OEM_PLUS, "\xEF\xBC\x8B", "\x2B"}, // ＋→+
{ VK_OEM_4, "\xE3\x80\x90", "\x7B"}, // （→{
{ VK_OEM_6, "\xE3\x80\x91", "\x7D"}, // ）→}
};
// 数字行 Shift 标点（按配置表）
if (shiftDown_) {
switch (vkey) {
case '1': outUtf8 = "\xEF\xBC\x81"; return true; // ！
case '2': outUtf8 = "\x40"; return true; // @
case '3': outUtf8 = "\x23"; return true; // #
case '4': outUtf8 = "\xEF\xBF\xA5"; return true; // ￥
case '5': outUtf8 = "\x25"; return true; // %
case '6': outUtf8 = "\xE2\x80\xA6\xE2\x80\xA6"; return true; // ……
case '7': outUtf8 = "\x26"; return true; // &
case '8': outUtf8 = "\x2A"; return true; // *
case '9': outUtf8 = "\xEF\xBC\x88"; return true; // （
case '0': outUtf8 = "\xEF\xBC\x89"; return true; // ）
}
}
for (const auto& m: kMap) {
if (m.vk == vkey) { outUtf8 = (shiftDown_ && m.zhShift) ? m.zhShift : m.zh; return true;}
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
if (vkey == '0' || vkey == VK_OEM_1 || vkey == VK_OEM_7) return composing(); // 0/;/'
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
// 四键上屏：满 4 码且唯一候选时直接上屏（超强音形行为），不用等空格
if (buffer_.size() >= 4) {
auto cands = QueryCandidates();
// 原版配置：候选唯一自动上屏码长=4，自动上屏仅限全码=是
if (cands.size() == 1 && cands[0].code == buffer_) {
host_->OnCommitText(cands[0].text);
buffer_.clear();
return true;
}
}
if (start) host_->OnCompositionStarted();
// 第一个码也要刷新组字内容和候选窗，否则用户感觉像没击键（盲打）
host_->OnCompositionUpdated(buffer_);
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

if (vkey == VK_SPACE) {
if (!composing()) return false;
CommitTopCandidate();
return true;
}

if (vkey == VK_RETURN) {
// 原版配置：键Enter=清除输入（不是上屏！）
if (!composing()) return false;
buffer_.clear();
host_->OnCancelComposition();
return true;
}

if (vkey == VK_ESCAPE) {
if (!composing()) return false;
buffer_.clear();
host_->OnCancelComposition();
return true;
}

if ((vkey >= '1' && vkey <= '9') || vkey == '0' || vkey == VK_OEM_1 || vkey == VK_OEM_7) {
if (!composing()) return false;
auto cands = QueryCandidates();
size_t idx = 0;
if (vkey >= '1' && vkey <= '9') idx = static_cast<size_t>(vkey - '1');
else if (vkey == '0') idx = 9;                    // 0=第10候选
else if (vkey == VK_OEM_1) idx = 1;               // ;=第2候选
else if (vkey == VK_OEM_7) idx = 2;               // '=第3候选
if (idx < cands.size()) {
host_->OnCommitText(cands[idx].text);
buffer_.clear();
}
// 序号越界也吃掉：保持组字状态不乱
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
