#pragma once
// 用户设置：存 HKCU\Software\QiangYin 下的 DWORD 值（每用户，无需管理员）。
// 默认值 = v0.1 的硬编码行为，保证升级后行为不变。
#include <windows.h>

namespace qy {

struct Settings {
    bool showHint = true;        // 候选窗编码提示
    bool autoCommitFull = true;  // 组字满 4 码且唯一候选时自动上屏
    bool enterClears = true;     // Enter=清除输入；false=上屏首选
    bool alwaysChinese = false;  // 切换到强音时总是中文模式（默认记住上次）
    bool chinesePunct = true;    // 中文模式下标点转全角
    bool fullShape = false;      // 全角模式（字母/数字/空格输出全角，Shift+空格 切换）
    int  candidateCount = 10;    // 候选数量（5/10）

    void Load();          // 读注册表，缺失/非法值回落默认
    bool Save() const;    // 写回注册表
};

} // namespace qy
