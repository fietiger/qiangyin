#pragma once
// 关于对话框：显示强音输入法版本号、架构/构建信息、词库状态与项目主页入口
#include <windows.h>
#include <stddef.h>

namespace qy {

// 弹出模态「关于」对话框；dictCount 为当前已加载词条数（0 表示按需加载尚未触发）
void ShowAboutDialog(HWND owner, size_t dictCount = 0);

} // namespace qy
