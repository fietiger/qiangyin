//! C ABI 导出：给 C++ TSF 外壳调用。
//!
//! 约定：
//! - `qy_new` 返回不透明句柄，不再用时调 `qy_free` 释放；
//! - `qy_candidate_text` / `qy_candidate_code` 返回的指针指向引擎内部缓存，
//!   在下一次 `qy_query` 或 `qy_free` 之前有效——如需长期持有请自行拷贝；
//! - 所有字符串都是 UTF-8、C 风格 NUL 结尾；
//! - 任何函数收到空指针都不会崩溃（返回 0 / NULL / -1）。

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_long, c_ulong};
use std::ptr;
use std::slice;

use crate::Engine;

/// 不透明句柄。`last` 缓存最近一次查询的候选（owned CString），
/// 保证返回给 C 的指针在下次查询前一直有效。
pub struct QyEngine {
    engine: Engine,
    last: Vec<(CString, CString)>, // (text, code)
}

/// 创建引擎句柄。失败（内存不足）时返回 NULL。
#[no_mangle]
pub extern "C" fn qy_new() -> *mut QyEngine {
    let qy = QyEngine {
        engine: Engine::new(),
        last: Vec::new(),
    };
    Box::into_raw(Box::new(qy))
}

/// 加载码表文件。可多次调用追加（比如先系统码表、后用户词库）。
/// `paths` 为 `n` 个 UTF-8 路径字符串的数组。
/// 返回累计加载的条目数，失败返回 -1。
#[no_mangle]
pub unsafe extern "C" fn qy_load(
    handle: *mut QyEngine,
    paths: *const *const c_char,
    n: c_ulong,
) -> c_long {
    if handle.is_null() || (paths.is_null() && n > 0) {
        return -1;
    }
    let qy = unsafe { &mut *handle };
    let raw = unsafe { slice::from_raw_parts(paths, n as usize) };
    let mut list: Vec<&str> = Vec::with_capacity(raw.len());
    for &p in raw {
        if p.is_null() {
            return -1;
        }
        match unsafe { CStr::from_ptr(p) }.to_str() {
            Ok(s) => list.push(s),
            Err(_) => return -1,
        }
    }
    match qy.engine.load_files(&list) {
        Ok(()) => qy.engine.len() as c_long,
        Err(_) => -1,
    }
}

/// 前缀查询：返回候选个数（不超过 `limit`）。
/// 之后用 `qy_candidate_text` / `qy_candidate_code` 按下标读取。
#[no_mangle]
pub unsafe extern "C" fn qy_query(
    handle: *mut QyEngine,
    prefix: *const c_char,
    limit: c_ulong,
) -> c_ulong {
    if handle.is_null() || prefix.is_null() {
        return 0;
    }
    let qy = unsafe { &mut *handle };
    let pfx = match unsafe { CStr::from_ptr(prefix) }.to_str() {
        Ok(s) => s,
        Err(_) => return 0,
    };
    qy.last.clear();
    for e in qy.engine.query(pfx, limit as usize) {
        // 条目不可能含 NUL；防御性跳过
        if let (Ok(t), Ok(c)) = (
            CString::new(e.text.as_str()),
            CString::new(e.code.as_str()),
        ) {
            qy.last.push((t, c));
        }
    }
    qy.last.len() as c_ulong
}

/// 第 `i` 个候选的文本（UTF-8）。越界返回 NULL。
#[no_mangle]
pub unsafe extern "C" fn qy_candidate_text(
    handle: *const QyEngine,
    i: c_ulong,
) -> *const c_char {
    if handle.is_null() {
        return ptr::null();
    }
    let qy = unsafe { &*handle };
    qy.last
        .get(i as usize)
        .map(|(t, _)| t.as_ptr())
        .unwrap_or(ptr::null())
}

/// 第 `i` 个候选的编码（UTF-8）。越界返回 NULL。
#[no_mangle]
pub unsafe extern "C" fn qy_candidate_code(
    handle: *const QyEngine,
    i: c_ulong,
) -> *const c_char {
    if handle.is_null() {
        return ptr::null();
    }
    let qy = unsafe { &*handle };
    qy.last
        .get(i as usize)
        .map(|(_, c)| c.as_ptr())
        .unwrap_or(ptr::null())
}

/// 已加载的总条目数。
#[no_mangle]
pub unsafe extern "C" fn qy_count(handle: *const QyEngine) -> c_ulong {
    if handle.is_null() {
        return 0;
    }
    unsafe { (*handle).engine.len() as c_ulong }
}

/// 释放句柄。
#[no_mangle]
pub unsafe extern "C" fn qy_free(handle: *mut QyEngine) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}
