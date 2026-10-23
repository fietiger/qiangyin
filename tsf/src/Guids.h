#pragma once
// 强音输入法 TSF 外壳用到的 GUID。固定值，勿改（改了等于换了一个输入法）。
#include <guiddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const GUID CLSID_QiangYinTextService; // COM 类 ID
extern const GUID GUID_QiangYinProfile;      // 语言配置 ID
extern const GUID GUID_QiangYinDisplayAttr;  // 组字下划线显示属性 ID
extern const GUID GUID_QY_LBI_INPUTMODE;     // 系统语言栏 input mode 项 GUID（任务栏中/英指示）

#ifdef __cplusplus
}
#endif
