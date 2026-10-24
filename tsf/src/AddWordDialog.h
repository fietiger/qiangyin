#pragma once
// 在线加词对话框（Ctrl+'+' 呼出，对齐原版超强音形）：
// 词条预填最近上屏的两个汉字（↑/↓ 调词长），编码按词组规则自动算出，
// 可选剪贴板造词。确定后由调用方写入用户词库。
#include <windows.h>
#include <string>

namespace qy {

struct AddWordContext {
    std::wstring recent;   // 最近上屏的汉字（旧→新，尾部最新），过滤过标点
    std::wstring mbPath;   // 系统码表路径（用于反查单字编码自动算词组码）
};

// 按超强音形词组规则计算编码（二字=每字前两码；三字=首字前两码+后两字首码；
// 四字=各字首码；多字=前三字+末字首码）。单字编码取码表里该字最长的条目（全码）。
std::wstring ComputeWordCode(const std::wstring& mbPath, const std::wstring& word);

// 模态对话框。返回 true=确定（outWord/outCode 为确认值）。
bool ShowAddWordDialog(HWND owner, AddWordContext& ctx,
                       std::wstring& outWord, std::wstring& outCode);

} // namespace qy
