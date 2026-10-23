# FFI（C ABI）说明

`src/ffi.rs` 把查词引擎导出为 C ABI，C++ TSF 外壳直接链接调用。
头文件：`ffi/qiangyin.h`；验证程序：`ffi/test_ffi.c`。

## API 一览

```c
QyEngine *qy_new(void);
long   qy_load(QyEngine *e, const char *const *paths, size_t n);
size_t qy_query(QyEngine *e, const char *prefix, size_t limit);
const char *qy_candidate_text(const QyEngine *e, size_t i);
const char *qy_candidate_code(const QyEngine *e, size_t i);
size_t qy_count(const QyEngine *e);
void   qy_free(QyEngine *e);
```

典型调用顺序：`qy_new` → `qy_load`（可多次追加）→ 循环 `qy_query` +
`qy_candidate_*` → `qy_free`。候选字符串指针在下次 `qy_query` 前有效。

## Linux 构建验证

```bash
cargo build
gcc ffi/test_ffi.c -o /tmp/test_ffi -Iffi -Ltarget/debug -lqiangyin
LD_LIBRARY_PATH=target/debug /tmp/test_ffi
```

## Windows 构建（给 TSF 外壳用）

不用在本地装 Rust：每次 push 到 master 后，GitHub Actions 会自动在
Windows 云主机上编译，产物 `qiangyin.dll` 在仓库的
Actions → 最新一次 run → Artifacts → `qiangyin-windows-x64` 里下载。
配合 `ffi/qiangyin.h` 和 `data/` 下的码表文件，即可被 C++（MSVC）链接。

想在本地编译也可以：装 Rust（https://rustup.rs/）后 `cargo build --release`，
产物在 `target\release\qiangyin.dll`。注意用 `--release`，
debug 版的查词速度会慢一个数量级。

## 给 TSF 外壳的集成建议

- 引擎初始化一次（输入法加载时 `qy_new` + `qy_load`），每次按键只调
  `qy_query`（<1ms，可在 UI 线程直接调用）；
- 组字、退格、中英文切换等按键逻辑全部放在 C++ 外壳：
  组字缓冲非空时 `VK_BACK` 必须吃掉（`pfEaten=TRUE`），不下发给程序——
  这正是原来超强音形退格 bug 的修复点；
- 用户自学习词频建议外壳另存文件，不要回写系统码表。
