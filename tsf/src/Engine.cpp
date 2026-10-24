// Rust 引擎动态加载实现
#include "Engine.h"
#include "QyLog.h"
#include <cstring> // strlen

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

std::wstring Engine::UserTablePath() {
    wchar_t buf[MAX_PATH] = {0};
    if (!ExpandEnvironmentStringsW(L"%APPDATA%\\QiangYin\\usercode.txt", buf, MAX_PATH))
        return std::wstring();
    return buf;
}

bool Engine::EnsureUserTable() {
    std::wstring path = UserTablePath();
    if (path.empty()) return false;
    std::wstring dir = path.substr(0, path.find_last_of(L"\\"));
    CreateDirectoryW(dir.c_str(), nullptr); // 已存在会失败，无视
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
        return true; // 已存在，不覆盖
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    // 模板必须写成 UTF-8：记事本凭内容识别编码，保存时才会保持 UTF-8，
    // 否则存成 UTF-16 会被引擎拒收（见 IsUtf8File）
    static const char* tmpl =
        "\xEF\xBB\xBF" // UTF-8 BOM
        "# 强音用户词库：每行一条「编码 文本」（空格分隔），# 开头为注释\r\n"
        "# 请保持 UTF-8 编码保存（记事本默认即是）\r\n"
        "# 保存后继续打字会自动重载；与系统码表同码时，用户词排在前面\r\n"
        "# 示例（去掉行首 # 即生效）：\r\n"
        "# qq 强音输入法\r\n";
    DWORD written = 0;
    WriteFile(h, tmpl, (DWORD)strlen(tmpl), &written, nullptr);
    CloseHandle(h);
    return true;
}

unsigned long long Engine::FileMtime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
        return 0;
    ULARGE_INTEGER m;
    m.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    m.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    return m.QuadPart;
}

bool Engine::IsUtf8File(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string buf;
    char chunk[4096];
    DWORD read = 0;
    while (ReadFile(h, chunk, sizeof(chunk), &read, nullptr) && read > 0)
        buf.append(chunk, read);
    CloseHandle(h);
    if (buf.empty()) return true;
    // MB_ERR_INVALID_CHARS：非法 UTF-8 返回 0（UTF-16/GBK 保存的文件都会在此现形）
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                buf.data(), (int)buf.size(), nullptr, 0);
    return n > 0;
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

    // 码表：系统表必须。加载顺序决定同码优先级（引擎按表序稳定排序）：
    //   用户词库（%APPDATA%，无需管理员）→ 系统码表 → 安装目录补充表
    std::wstring mb = FindFile(L"mb.txt");
    std::wstring extra = FindFile(L"usercode.txt"); // 安装目录自带的补充表
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

    userFile_.clear();
    userMtime_ = 0;
    std::vector<std::string> utf8s; // 字符串本体，paths 指向这里
    std::wstring myUser = UserTablePath();
    if (!myUser.empty() && GetFileAttributesW(myUser.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (IsUtf8File(myUser)) {
            userFile_ = myUser;
            userMtime_ = FileMtime(myUser);
            utf8s.push_back(toUtf8(myUser));
            { wchar_t b[512]; wsprintfW(b, L"Engine::Load user=%s", myUser.c_str()); qy::log::Write(b); }
        } else {
            // Rust 引擎遇到非 UTF-8 文件会让整个 qy_load 失败，必须在这里拦下
            QYLOG("Engine::Load user table NOT utf-8, skipped");
        }
    }
    utf8s.push_back(toUtf8(mb));
    if (!extra.empty())
        utf8s.push_back(toUtf8(extra));

    std::vector<const char*> paths;
    for (const auto& s : utf8s) paths.push_back(s.c_str());
    if (pLoad_(engine_, paths.data(), paths.size()) < 0) { QYLOG("Engine::Load qy_load failed"); Unload(); return false; }
    { wchar_t b[128]; wsprintfW(b, L"Engine::Load OK count=%lu", (unsigned long)pCount_(engine_)); qy::log::Write(b); }
    return true;
}

void Engine::Unload() {
    if (engine_ && pFree_) { pFree_(engine_); engine_ = nullptr; }
    if (dll_) { FreeLibrary(dll_); dll_ = nullptr; }
    pNew_ = nullptr; pLoad_ = nullptr; pQuery_ = nullptr;
    pText_ = nullptr; pCode_ = nullptr; pCount_ = nullptr; pFree_ = nullptr;
}

void Engine::CheckUserTableChanged() {
    if (!engine_ || userFile_.empty()) return;
    unsigned long long m = FileMtime(userFile_);
    if (m == 0 || m == userMtime_) return;
    QYLOG("Engine:: user table changed, reloading");
    Unload(); // Load 会重读用户词库并刷新 userMtime_
    Load();
}

std::vector<Candidate> Engine::Query(const std::string& prefix, size_t limit) {
    CheckUserTableChanged(); // 每次 stat 一下，编辑保存后无需重启输入法
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
