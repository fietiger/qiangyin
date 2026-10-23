# ime-engine（超强音形查词引擎原型）

用 Rust 实现的输入法查词引擎原型：从文本码表加载，用 trie 做前缀查询。
纯逻辑层，不依赖任何操作系统 API。

## 布局

```
ime-engine/
├── Cargo.toml
├── data/
│   ├── mb.txt        # 系统码表（12.3 万条，"编码 文本"/行，来自 fietiger/mbtrans）
│   └── usercode.txt  # 用户词库（追加在系统词库之后）
├── src/
│   ├── lib.rs        # Engine：trie 索引 + query()
│   └── main.rs       # ime-demo：终端演示，模拟候选过程
└── tests/
    └── engine_test.rs  # 基于真实码表的集成测试
```

## 运行

```bash
cargo test                       # 单元测试 + 真实码表集成测试
cargo run -- data/mb.txt          # 交互演示：输入编码看候选
printf 'hc\naab\n' | cargo run -- data/mb.txt
```

## Engine API（给将来 TSF 外壳用）

```rust
let e = Engine::load(&["data/mb.txt", "data/usercode.txt"])?;
let cands: Vec<&Entry> = e.query("hc", 9);  // 精确匹配优先，其余按码表顺序
// Entry { code: "hc", text: "好" }
```

单次查询 < 1ms（12 万条，见 `perf_single_query_fast` 测试）。

## 下一步（路线图）

1. **FFI 导出**：把 Engine 编成 `cdylib`，导出 C ABI
   （`engine_load / engine_query / engine_free`），供 C++ TSF 外壳调用。
2. **TSF 外壳（Windows, C++）**：`ITfTextInputProcessor` 实现，
   按键处理、组字（composition）、候选窗口 UI。退格 bug 的修复点在这里：
   组字缓冲非空时，`VK_BACK` 必须由输入法吃掉（`pfEaten=TRUE`），不下发给程序。
3. **增强**：整句连打、简拼/模糊音、自学习词频（另存用户词频文件，不污染码表）。
