# 强音 / qiangyin

音形输入法「强音」的查词引擎：Rust 实现，trie 索引 + 前缀查询，
可导出 C ABI 给 Windows TSF 外壳调用。

## 布局

```
.
├── Cargo.toml
├── data/
│   ├── mb.txt        # 系统码表（12.3 万条，"编码 文本"/行）
│   └── usercode.txt  # 用户词库（追加在系统词库之后）
├── src/
│   ├── lib.rs        # Engine：trie 索引 + query()
│   ├── ffi.rs        # C ABI 导出（qy_new/qy_load/qy_query/...）
│   └── main.rs       # qiangyin-demo：终端演示，模拟候选过程
├── ffi/
│   ├── qiangyin.h    # C 头文件（给 C++ 外壳用）
│   ├── test_ffi.c    # C ABI 端到端验证程序
│   └── README.md     # FFI 说明
├── tests/
│   └── engine_test.rs  # 基于真实码表的集成测试
└── .github/workflows/build.yml  # 自动构建 Windows DLL / Linux so
```

## 本地运行

```bash
cargo test                       # 单元测试 + 真实码表集成测试
cargo run -- data/mb.txt          # 交互演示：输入编码看候选
printf 'hc\naab\n' | cargo run -- data/mb.txt
```

## 拿 Windows DLL（不用装 Rust）

push 到 master 后，GitHub Actions 自动编译：
仓库页面 → Actions → 最新 run → Artifacts → 下载 `qiangyin-windows-x64`，
里面就是 `qiangyin.dll`。配合 `ffi/qiangyin.h` 和 `data/` 码表给 C++ 用。

## 安装包

- **正式版**：[Releases](../../releases) 下载 `QiangYin-Setup.exe`（永久地址，无需登录）
- 开发版：Actions → 最新 run → Artifacts → `qiangyin-setup-windows-x64`（需登录 GitHub，90 天过期）
- 发布：打 `v*` tag 并 push 即自动构建并发布 Release

## Engine API（Rust）

```rust
let e = Engine::load(&["data/mb.txt", "data/usercode.txt"])?;
let cands: Vec<&Entry> = e.query("hc", 9);  // 精确匹配优先，其余按码表顺序
// Entry { code: "hc", text: "好" }
```

单次查询 < 1ms（12 万条，见 `perf_single_query_fast` 测试）。

## 路线图

1. ~~FFI 导出~~：已完成（`qy_new / qy_load / qy_query / qy_candidate_text / qy_candidate_code / qy_count / qy_free`）
2. **TSF 外壳（Windows, C++）**：`ITfTextInputProcessor` 实现，
   按键处理、组字（composition）、候选窗口 UI。退格 bug 的修复点在这里：
   组字缓冲非空时，`VK_BACK` 必须由输入法吃掉（`pfEaten=TRUE`），不下发给程序。
3. **增强**：整句连打、简拼/模糊音、自学习词频（另存用户词频文件，不污染码表）。
