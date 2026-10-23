#pragma once
// 设置对话框：模态，资源模板 IDD_SETTINGS（qiangyin.rc）
#include <windows.h>
#include "Settings.h"

namespace qy {

// 弹出设置对话框。返回 true = 用户点了确定（s 已被更新并写回注册表）。
bool ShowSettingsDialog(HWND owner, Settings& s);

} // namespace qy
