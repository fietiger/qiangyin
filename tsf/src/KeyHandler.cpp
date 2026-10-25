// 按键状态机实现
#include "KeyHandler.h"
#include <windows.h>

namespace qy {

namespace {
// BMP 码点 → UTF-8（本项目用到的都在 3 字节区段）
std::string ToUtf8(unsigned cp) {
	std::string s;
	s += static_cast<char>(0xE0 | (cp >> 12));
	s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
	s += static_cast<char>(0x80 | (cp & 0x3F));
	return s;
}

// 全角块 U+FF01..U+FF5E 与 ASCII 0x21..0x7E 一一对应；空格单独映射 U+3000
// 注意基址是 0xFEE0（'!'0x21→FF01），写成 0xFF00 会错进半角片假名块（ｱｹ）
std::string FullWidthAscii(char c) {
	if (c == ' ') return ToUtf8(0x3000);
	if (c >= 0x21 && c <= 0x7E) return ToUtf8(0xFEE0 + static_cast<unsigned char>(c));
	return std::string();
}

// 英文直通模式下，该键在全角时应输出的全角字符（空串=该键不转）
// 按 US 键盘布局把 VK(+Shift) 还原成 ASCII，再查全角块
std::string FullWidthForKey(UINT vkey, bool shift) {
	char c = 0;
	if (vkey == VK_SPACE) {
		c = ' ';
	} else if (vkey >= 'A' && vkey <= 'Z') {
		c = shift ? static_cast<char>(vkey) : static_cast<char>(vkey - 'A' + 'a');
	} else if (vkey >= '0' && vkey <= '9') {
		static const char kShiftDigits[] = "!@#$%^&*()";
		c = shift ? kShiftDigits[vkey - '0'] : static_cast<char>(vkey);
	} else {
		switch (vkey) {
		case VK_OEM_1:      c = shift ? ':'  : ';';  break;
		case VK_OEM_PLUS:   c = shift ? '+'  : '=';  break;
		case VK_OEM_COMMA:  c = shift ? '<'  : ',';  break;
		case VK_OEM_MINUS:  c = shift ? '_'  : '-';  break;
		case VK_OEM_PERIOD: c = shift ? '>'  : '.';  break;
		case VK_OEM_2:      c = shift ? '?'  : '/';  break;
		case VK_OEM_3:      c = shift ? '~'  : '`';  break;
		case VK_OEM_4:      c = shift ? '{'  : '[';  break;
		case VK_OEM_5:      c = shift ? '|'  : '\\'; break;
		case VK_OEM_6:      c = shift ? '}'  : ']';  break;
		case VK_OEM_7:      c = shift ? '"'  : '\''; break;
		default: return std::string();
		}
	}
	return FullWidthAscii(c);
}
} // namespace

KeyHandler::KeyHandler(IKeyHandlerHost* host): host_(host) {}

void KeyHandler::Reset() {
buffer_.clear();
shiftDown_ = false;
shiftUsed_ = false;
page_ = 0;
sel_ = 0;
}

size_t KeyHandler::PageSize() const {
return settings_ ? (size_t)settings_->candidateCount : 10;
}

int KeyHandler::PageCount() const {
size_t total = QueryCandidates().size();
if (total == 0) return 0;
return (int)((total + PageSize() - 1) / PageSize());
}

void KeyHandler::PageMove(int delta) {
int pc = PageCount();
if (pc <= 1) return; // 只有一页（或没有候选）不翻
int p = page_ + delta;
if (p < 0) p = 0;
if (p > pc - 1) p = pc - 1;
if (p != page_) {
page_ = p;
sel_ = 0;
// buffer 没变，借组字刷新通道让候选窗重画当前页
host_->OnCompositionUpdated(buffer_);
}
}

// ↑/↓：页内移动高亮；到页边界则翻页（翻页后高亮落在新页首/尾）
void KeyHandler::SelectionMove(int delta) {
int cnt = (int)QueryCandidates().size();
if (cnt == 0) return;
size_t ps = PageSize();
int pageItems = (int)((size_t)(page_ + 1) * ps <= (size_t)cnt ? ps : (size_t)cnt - (size_t)page_ * ps);
int s = sel_ + delta;
	if (s < 0) {           // 越过页首 → 上一页尾（上翻落的是非末页，必满页）
	if (page_ > 0) { PageMove(-1); sel_ = (int)ps - 1; }
	return;
	}
if (s >= pageItems) {  // 越过页尾 → 下一页首
if (page_ < PageCount() - 1) { PageMove(1); sel_ = 0; }
return;
}
sel_ = s;
host_->OnCompositionUpdated(buffer_);
}

std::vector<Candidate> KeyHandler::CurrentPage(size_t* totalOut) const {
std::vector<Candidate> all = QueryCandidates();
if (totalOut) *totalOut = all.size();
size_t start = (size_t)page_ * PageSize();
if (start >= all.size()) return {};
size_t n = PageSize() < all.size() - start ? PageSize() : all.size() - start;
return std::vector<Candidate>(all.begin() + (long)start, all.begin() + (long)(start + n));
}

bool KeyHandler::HasCtrlAlt() const {
return (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000);
}

bool KeyHandler::CtrlOnly() const {
return (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000) &&
       !shiftDown_ && !((GetKeyState(VK_SHIFT) & 0x8000));
}

bool KeyHandler::CtrlHeld() const {
return (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000);
}

std::vector<Candidate> KeyHandler::QueryCandidates() const {
if (query_ &&!buffer_.empty()) return query_(buffer_, queryCtx_);
return {};
}

void KeyHandler::CommitTopCandidate() {
auto cands = QueryCandidates();
if (!cands.empty()) {
// 空格/标点兜底上屏：当前页的高亮项（与候选窗所见一致）
size_t idx = (size_t)page_ * PageSize() + (size_t)sel_;
if (idx >= cands.size()) idx = (size_t)page_ * PageSize();
	host_->OnCommitText(cands[idx].text);
} else {
	host_->OnCommitText(buffer_); // 无候选：原样上屏（极少见）
}
buffer_.clear();
page_ = 0;
sel_ = 0;
}

void KeyHandler::ToggleMode() {
    SetChineseMode(!chinese_);
}

void KeyHandler::SetChineseMode(bool chinese) {
    if (chinese == chinese_) return;
    if (composing()) {
        // 切换时组字怎么处理（与 Enter 三态同构）：
        // 0=清空（默认，直接切）1=上屏首选（原版风格）2=编码上屏（字母原样进文）
        switch (SwitchMode()) {
        case 1:
            CommitTopCandidate();
            break;
        case 2:
            host_->OnCommitText(buffer_);
            buffer_.clear();
            page_ = 0;
            break;
        default:
            buffer_.clear();
            page_ = 0;
            host_->OnCancelComposition();
            break;
        }
    }
    chinese_ = chinese;
    if (!chinese_) {
        // 可选：切到英文时自动收拾全角/中文标点状态（全角英文不好打）
        if (EngAutoHalfWidth() && fullShape_) {
            fullShape_ = false;
            host_->OnFullShapeChanged(false);
        }
        if (EngAutoEnPunct() && ChinesePunct())
            host_->OnPunctChanged(false);
    }
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
	if (vkey == VK_OEM_PERIOD && CtrlOnly())
	return true; // Ctrl+.：切换中文/英文标点（经典 IME 快捷键）

	if ((vkey == VK_OEM_PLUS || vkey == VK_ADD) && CtrlHeld())
	return true; // Ctrl+'+'：在线造词（原版超强音形同款）

	// 翻页键：-/= 与 ↑/↓（组字中；Shift/Ctrl/Alt 组合放行给应用）
	{
	bool noMod = !((GetKeyState(VK_SHIFT) | GetKeyState(VK_CONTROL) |
	                GetKeyState(VK_MENU)) & 0x8000);
	if (composing() && noMod &&
	    (vkey == VK_OEM_MINUS || vkey == VK_OEM_PLUS ||
	     vkey == VK_UP || vkey == VK_DOWN))
	return true;
	}

	if (HasCtrlAlt()) return false; // 快捷键一律放行

	if (vkey == VK_SHIFT || vkey == VK_LSHIFT || vkey == VK_RSHIFT)
	return true; // Shift 点按用于切换，交由 HandleKeyUp 判定

	// Shift+空格：全/半角切换（组字中不切，空格仍按选字处理）
	if (vkey == VK_SPACE && !composing() && (GetKeyState(VK_SHIFT) & 0x8000))
	return true;

	if (!chinese_) { // 英文模式
	if (fullShape_) {
		if (!FullWidthForKey(vkey, (GetKeyState(VK_SHIFT) & 0x8000) != 0).empty())
		return true;
	}
	return false; // 其余按键放行
	}

	if (vkey >= 'A' && vkey <= 'Z') return true;
	if (vkey == VK_BACK) return composing(); // ★ 核心修复：组字中退格必吃
	if (vkey == VK_SPACE) return composing() || fullShape_; // 全角时空格出全角空格
	if (vkey == VK_RETURN || vkey == VK_ESCAPE)
	return composing();
	{ // 数字：组字中选字；全角且无 Shift 时上全角数字（Shift+数字归标点表）
	bool shiftNow = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	if (vkey >= '1' && vkey <= '9') return composing() || (fullShape_ && !shiftNow);
	if (vkey == '0') return composing() || (fullShape_ && !shiftNow);
	}
	if (vkey == VK_OEM_1 || vkey == VK_OEM_7) return composing(); // ;/' 选字
	std::string dummy;
	if (ChinesePunct() && IsPunctKey(vkey, dummy)) return true;
	return false;
}

// 组字被打断时按 Enter 行为三态收尾：0=清空 1=首选上屏 2=编码上屏
void KeyHandler::FlushCompositionEnterLike() {
	switch (EnterMode()) {
	case 1:
	CommitTopCandidate();
	break;
	case 2:
	host_->OnCommitText(buffer_);
	buffer_.clear();
	page_ = 0;
	sel_ = 0;
	break;
	default:
	buffer_.clear();
	page_ = 0;
	sel_ = 0;
	host_->OnCancelComposition();
	break;
	}
}

bool KeyHandler::HandleKeyDown(UINT vkey, LPARAM lParam) {
if (vkey == VK_SHIFT || vkey == VK_LSHIFT || vkey == VK_RSHIFT) {
if (!shiftDown_) { shiftDown_ = true; shiftUsed_ = false;}
return true; // 吃掉，等 KeyUp 再决定是否切换
}
	if (shiftDown_) shiftUsed_ = true; // Shift 按住期间有其他键：不是点按

	// Ctrl+.：切换中文/英文标点（经典 IME 快捷键，组字中也不受影响）
	if (vkey == VK_OEM_PERIOD && CtrlOnly()) {
	host_->OnPunctChanged(!ChinesePunct());
	return true;
	}

	// Ctrl+'+'（主键区 = 或小键盘 +）：在线造词，弹对话框
	if ((vkey == VK_OEM_PLUS || vkey == VK_ADD) && CtrlHeld()) {
	host_->OnOpenAddWord();
	return true;
	}

	if (HasCtrlAlt()) return false;

	// Shift+空格：切换全/半角（组字中不切，空格仍按选字上屏处理）
	if (shiftDown_ && vkey == VK_SPACE && !composing()) {
	fullShape_ = !fullShape_;
	host_->OnFullShapeChanged(fullShape_);
	return true;
	}

	if (!chinese_) {
	// 全角模式：英文直通的字母/数字/符号/空格转全角上屏
	if (fullShape_) {
		std::string fw = FullWidthForKey(vkey, shiftDown_);
		if (!fw.empty()) { host_->OnCommitText(fw); return true; }
	}
	return false;
	}

	// 翻页：- = ↑ ↓（原版超强音形同款：组字中按 -/= 纯翻页，不上屏）
	// ↑/↓ 是页内移动高亮（越界翻页）；-/= 是纯翻页
	if (composing() && !shiftDown_ &&
	    (vkey == VK_OEM_MINUS || vkey == VK_OEM_PLUS ||
	     vkey == VK_UP || vkey == VK_DOWN)) {
	if (vkey == VK_OEM_MINUS) PageMove(-1);
	else if (vkey == VK_OEM_PLUS) PageMove(1);
	else if (vkey == VK_UP) SelectionMove(-1);
	else SelectionMove(1);
	return true;
	}

	if (vkey >= 'A' && vkey <= 'Z') {
	// 大写字母必输出英文：Shift 按住 或 CapsLock 开启（大小写锁定模式）
	// 都不进组字直接上屏；组字中先按 Enter 三态收尾
	bool caps = (GetKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_CAPITAL) & 1);
	if (caps) {
		if (composing()) FlushCompositionEnterLike();
		host_->OnCommitText(std::string(1, (char)vkey));
		return true;
	}
	bool start = buffer_.empty();
	page_ = 0; // 新码开始，回到第一页
	sel_ = 0;
	buffer_ += static_cast<char>(vkey - 'A' + 'a');
	// 四键上屏：满 4 码且唯一候选时直接上屏（超强音形行为），不用等空格
	if (AutoCommitFull() && buffer_.size() >= 4) {
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
	page_ = 0;
	sel_ = 0;
	if (buffer_.empty()) host_->OnCancelComposition();
	else host_->OnCompositionUpdated(buffer_);
	return true;
	}

	if (vkey == VK_SPACE) {
	if (!composing()) {
		if (fullShape_) { host_->OnCommitText(ToUtf8(0x3000)); return true; } // 全角空格
		return false;
	}
	CommitTopCandidate();
	return true;
	}

	if (vkey == VK_RETURN) {
	if (!composing()) return false;
	// 可配置：0=清除输入（原版默认）1=上屏首选 2=编码上屏（想要英文字母直接回车）
	FlushCompositionEnterLike();
	return true;
	}

	if (vkey == VK_ESCAPE) {
	if (!composing()) return false;
	buffer_.clear();
	page_ = 0;
	sel_ = 0;
	host_->OnCancelComposition();
	return true;
	}

	// 全角模式：非组字时数字直接上全角（Shift+数字走标点表出 ！＠＃ 等，不在此处理）
	if (fullShape_ && !composing() && !shiftDown_ && vkey >= '0' && vkey <= '9') {
	host_->OnCommitText(ToUtf8(0xFF10 + (vkey - '0')));
	return true;
	}

	if ((vkey >= '1' && vkey <= '9') || vkey == '0' || vkey == VK_OEM_1 || vkey == VK_OEM_7) {
	if (!composing()) return false;
	auto cands = QueryCandidates();
	size_t idx = 0; // 页内序号
	if (vkey >= '1' && vkey <= '9') idx = static_cast<size_t>(vkey - '1');
	else if (vkey == '0') idx = 9;                    // 0=第10候选
	else if (vkey == VK_OEM_1) idx = 1;               // ;=第2候选
	else if (vkey == VK_OEM_7) idx = 2;               // '=第3候选
	size_t absIdx = (size_t)page_ * PageSize() + idx; // 翻页后选的是当前页的候选
	if (absIdx < cands.size()) {
		host_->OnCommitText(cands[absIdx].text);
		buffer_.clear();
		page_ = 0;
		sel_ = 0;
	}
	// 序号越界也吃掉：保持组字状态不乱
	return true;
	}

	std::string punct;
	if (ChinesePunct() && IsPunctKey(vkey, punct)) {
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
