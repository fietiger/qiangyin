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

    // 加载 qiangyin.dll + data/mb.txt + data/usercode.txt。成功返回 true。
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
};

} // namespace qy
