#pragma once
// 设置对话框：模态，资源模板 IDD_SETTINGS（qiangyin.rc）
#include <windows.h>
#include "Settings.h"

namespace qy {

// 弹出设置对话框。返回 true = 用户点了确定（s 已被更新并写回注册表）。
// dictCount 供对话框内的「关于...」按钮展示当前词库加载条数。
bool ShowSettingsDialog(HWND owner, Settings& s, size_t dictCount = 0);

} // namespace qy
