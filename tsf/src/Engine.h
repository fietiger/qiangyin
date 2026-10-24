#pragma once
// Rust 引擎 qiangyin.dll 的动态加载封装。
// DLL 与码表查找顺序：本 DLL 所在目录 -> 程序当前目录 -> 系统 PATH。
#include <windows.h>
#include <string>
#include <vector>
#include "Candidate.h"

namespace qy {

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // 用户词库（%APPDATA%\QiangYin\usercode.txt）：
    // 加载顺序在系统码表之前，同码条目排在系统词前面；文件被修改后
    // 下次查询自动热重载（见 Query）。
    static std::wstring UserTablePath();
    static bool EnsureUserTable(); // 目录/文件不存在则创建（带 UTF-8 模板注释）

    // 加载 qiangyin.dll + 码表。成功返回 true。
    bool Load();
    void Unload();
    bool IsLoaded() const { return dll_ != nullptr && engine_ != nullptr; }

    // 前缀查询，返回候选（精确匹配优先，其余按码表顺序）。线程：调用线程。
    std::vector<Candidate> Query(const std::string& prefix, size_t limit = 9);
    size_t Count() const;

private:
    // qiangyin.h 的函数指针（避免直接 include Rust 头，保持解耦）
    struct QyEngine;
    using FnNew = QyEngine* (*)();
    using FnLoad = long (*)(QyEngine*, const char* const*, size_t);
    using FnQuery = size_t (*)(QyEngine*, const char*, size_t);
    using FnText = const char* (*)(const QyEngine*, size_t);
    using FnCode = const char* (*)(const QyEngine*, size_t);
    using FnCount = size_t (*)(const QyEngine*);
    using FnFree = void (*)(QyEngine*);

    bool LoadFunctions();
    std::wstring FindFile(const std::wstring& name) const; // 在候选目录中找文件
    void CheckUserTableChanged(); // Query 前调：用户词库 mtime 变了就整体重载
    static bool IsUtf8File(const std::wstring& path); // 防 UTF-16/GBK 文件拖垮整个引擎
    static unsigned long long FileMtime(const std::wstring& path); // 0=取不到
    std::wstring moduleDir_; // 本 DLL 所在目录

    HMODULE dll_ = nullptr;
    QyEngine* engine_ = nullptr;
    FnNew pNew_ = nullptr;
    FnLoad pLoad_ = nullptr;
    FnQuery pQuery_ = nullptr;
    FnText pText_ = nullptr;
    FnCode pCode_ = nullptr;
    FnCount pCount_ = nullptr;
    FnFree pFree_ = nullptr;

    std::wstring userFile_;               // 用户词库路径（空=没配/不可用）
    unsigned long long userMtime_ = 0;    // 上次加载时的 mtime
};

} // namespace qy
