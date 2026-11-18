#pragma once
// 资源 ID：qiangyin.rc 与对话框代码共用（rc 里 #include "src/resids.h"）
#define IDD_SETTINGS      101
#define IDD_ADDWORD       102
#define IDD_ABOUT         103

#define IDC_SHOW_HINT      1001
#define IDC_AUTO_COMMIT    1002
#define IDC_CHINESE_PUNCT  1003
#define IDC_ALWAYS_CHINESE 1004
#define IDC_ENTER_CLEARS   1005
#define IDC_ENTER_COMMIT   1006
#define IDC_CAND_COMBO     1007
#define IDC_FULL_SHAPE     1008
#define IDC_ENG_HALFWIDTH  1009
#define IDC_ENG_ENPUNCT    1010

// 在线加词（IDD_ADDWORD）
#define IDC_AW_WORD        1101
#define IDC_AW_CODE        1102
#define IDC_AW_CLIP        1103
#define IDC_AW_WARN        1104

// Enter 行为 / 切换中英行为（各三选一）
#define IDC_ENTER_CODE     1011
#define IDC_SW_CLEARS      1012
#define IDC_SW_COMMIT      1013
#define IDC_SW_CODE        1014

// 候选窗排列方向、字号与关于按钮
#define IDC_CAND_VERTICAL   1015
#define IDC_CAND_HORIZONTAL 1016
#define IDC_OPEN_ABOUT      1017
#define IDC_FONT_COMBO      1018

// 关于对话框（IDD_ABOUT）
#define IDC_ABOUT_TITLE     1201
#define IDC_ABOUT_VERSION   1202
#define IDC_ABOUT_ENGINE    1203
#define IDC_ABOUT_DESC      1204
#define IDC_ABOUT_REPO      1205
#define IDC_ABOUT_USERPATH  1206
