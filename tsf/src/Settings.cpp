#include "Settings.h"

namespace qy {

static const wchar_t* kRegKey = L"Software\\QiangYin";

static DWORD ReadDword(const wchar_t* name, DWORD def) {
    DWORD v = 0, size = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, name, RRF_RT_REG_DWORD,
                     nullptr, &v, &size) == ERROR_SUCCESS)
        return v;
    return def;
}

void Settings::Load() {
    showHint       = ReadDword(L"ShowHint", 1) != 0;
    autoCommitFull = ReadDword(L"AutoCommitFull", 1) != 0;
    { // EnterMode：0=清除 1=上屏首选 2=编码上屏；兼容旧版 EnterClears
      DWORD m = ReadDword(L"EnterMode", 0xFFFFFFFF);
      if (m == 0xFFFFFFFF)
          m = ReadDword(L"EnterClears", 1) != 0 ? 0 : 1;
      enterMode = (m <= 2) ? (int)m : 0;
    }
    switchClears   = ReadDword(L"SwitchClears", 1) != 0;
    alwaysChinese  = ReadDword(L"AlwaysChinese", 0) != 0;
    chinesePunct   = ReadDword(L"ChinesePunct", 1) != 0;
    fullShape      = ReadDword(L"FullShape", 0) != 0;
    engAutoHalfWidth = ReadDword(L"EngAutoHalfWidth", 1) != 0;
    engAutoEnPunct   = ReadDword(L"EngAutoEnPunct", 0) != 0;
    DWORD c = ReadDword(L"CandidateCount", 10);
    candidateCount = (c >= 5 && c <= 10) ? (int)c : 10; // 选字键最多 0=第10，候选上限 10
}

bool Settings::Save() const {
    HKEY h = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_WRITE,
                        nullptr, &h, nullptr) != ERROR_SUCCESS)
        return false;
    struct { const wchar_t* name; DWORD v; } vals[] = {
        {L"ShowHint",       showHint ? 1u : 0u},
        {L"AutoCommitFull", autoCommitFull ? 1u : 0u},
        {L"EnterMode",      static_cast<DWORD>(enterMode)},
        {L"SwitchClears",   switchClears ? 1u : 0u},
        {L"AlwaysChinese",  alwaysChinese ? 1u : 0u},
        {L"ChinesePunct",   chinesePunct ? 1u : 0u},
        {L"FullShape",      fullShape ? 1u : 0u},
        {L"EngAutoHalfWidth", engAutoHalfWidth ? 1u : 0u},
        {L"EngAutoEnPunct",   engAutoEnPunct ? 1u : 0u},
        {L"CandidateCount", static_cast<DWORD>(candidateCount)},
    };
    for (const auto& kv : vals)
        RegSetValueExW(h, kv.name, 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&kv.v), sizeof(kv.v));
    RegCloseKey(h);
    return true;
}

} // namespace qy
