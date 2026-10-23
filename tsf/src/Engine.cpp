// Rust 引擎动态加载实现
#include "Engine.h"
#include "QyLog.h"

namespace qy {

namespace { int g_moduleAnchor = 0; } // 取模块句柄用的地址锚点

Engine::Engine() {
    wchar_t path[MAX_PATH] = {0};
    HMODULE self = nullptr;
    // 取本 DLL 自身路径（从本模块内静态变量地址反查）
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&g_moduleAnchor), &self);
    if (self) {
        GetModuleFileNameW(self, path, MAX_PATH);
        moduleDir_ = path;
        size_t pos = moduleDir_.find_last_of(L"\\/");
        if (pos != std::wstring::npos) moduleDir_.resize(pos);
        FreeLibrary(self);
    }
}

Engine::~Engine() { Unload(); }

std::wstring Engine::FindFile(const std::wstring& name) const {
    const std::wstring dirs[] = { moduleDir_, L".", };
    for (const auto& d : dirs) {
        if (d.empty()) continue;
        std::wstring p = d + L"\\" + name;
        DWORD attr = GetFileAttributesW(p.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            return p;
        // data\ 子目录也找一下
        p = d + L"\\data\\" + name;
        attr = GetFileAttributesW(p.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            return p;
    }
    return std::wstring();
}

bool Engine::LoadFunctions() {
    pNew_   = reinterpret_cast<FnNew>(GetProcAddress(dll_, "qy_new"));
    pLoad_  = reinterpret_cast<FnLoad>(GetProcAddress(dll_, "qy_load"));
    pQuery_ = reinterpret_cast<FnQuery>(GetProcAddress(dll_, "qy_query"));
    pText_  = reinterpret_cast<FnText>(GetProcAddress(dll_, "qy_candidate_text"));
    pCode_  = reinterpret_cast<FnCode>(GetProcAddress(dll_, "qy_candidate_code"));
    pCount_ = reinterpret_cast<FnCount>(GetProcAddress(dll_, "qy_count"));
    pFree_  = reinterpret_cast<FnFree>(GetProcAddress(dll_, "qy_free"));
    return pNew_ && pLoad_ && pQuery_ && pText_ && pCode_ && pCount_ && pFree_;
}

bool Engine::Load() {
    if (dll_) return true;
    QYLOG("Engine::Load begin");
    std::wstring dllPath = FindFile(L"qiangyin.dll");
    if (dllPath.empty()) { QYLOG("Engine::Load qiangyin.dll not found"); return false; }
    { wchar_t b[512]; wsprintfW(b, L"Engine::Load dll=%s", dllPath.c_str()); qy::log::Write(b); }
    dll_ = LoadLibraryW(dllPath.c_str());
    if (!dll_) { QYLOG("Engine::Load LoadLibrary failed"); return false; }
    if (!LoadFunctions()) { QYLOG("Engine::Load GetProcAddress failed"); Unload(); return false; }

    engine_ = pNew_();
    if (!engine_) { Unload(); return false; }

    // 码表：系统表必须，usercode 可选
    std::wstring mb = FindFile(L"mb.txt");
    std::wstring user = FindFile(L"usercode.txt");
    if (mb.empty()) { QYLOG("Engine::Load mb.txt not found"); Unload(); return false; }
    { wchar_t b[512]; wsprintfW(b, L"Engine::Load mb=%s", mb.c_str()); qy::log::Write(b); }

    // 路径用 UTF-8 传给 Rust（qy_load 要求 UTF-8）
    auto toUtf8 = [](const std::wstring& w) -> std::string {
        if (w.empty()) return std::string();
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (n <= 1) return std::string();
        std::string s(n - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
        return s;
    };
    std::string mbUtf8 = toUtf8(mb);

    std::vector<const char*> paths;
    paths.push_back(mbUtf8.c_str());
    std::string userUtf8;
    if (!user.empty()) {
        userUtf8 = toUtf8(user);
        paths.push_back(userUtf8.c_str());
    }
    if (pLoad_(engine_, paths.data(), paths.size()) < 0) { QYLOG("Engine::Load qy_load failed"); Unload(); return false; }
    { wchar_t b[128]; wsprintfW(b, L"Engine::Load OK count=%llu", (unsigned long long)pCount_(engine_)); qy::log::Write(b); }
    return true;
}

void Engine::Unload() {
    if (engine_ && pFree_) { pFree_(engine_); engine_ = nullptr; }
    if (dll_) { FreeLibrary(dll_); dll_ = nullptr; }
    pNew_ = nullptr; pLoad_ = nullptr; pQuery_ = nullptr;
    pText_ = nullptr; pCode_ = nullptr; pCount_ = nullptr; pFree_ = nullptr;
}

std::vector<Candidate> Engine::Query(const std::string& prefix, size_t limit) {
    std::vector<Candidate> out;
    if (!engine_ || !pQuery_) return out;
    size_t cnt = pQuery_(engine_, prefix.c_str(), limit);
    for (size_t i = 0; i < cnt; ++i) {
        const char* t = pText_(engine_, i);
        const char* c = pCode_(engine_, i);
        if (t && c) out.push_back({t, c});
    }
    return out;
}

size_t Engine::Count() const {
    return (engine_ && pCount_) ? pCount_(engine_) : 0;
}

} // namespace qy
