//! 基于真实码表 data/mb.txt 的集成测试。
//! 运行：cargo test

use qiangyin::Engine;

fn real_engine() -> Engine {
    Engine::load(&["data/mb.txt"]).expect("加载 data/mb.txt 失败")
}

#[test]
fn loads_full_table() {
    let e = real_engine();
    assert!(
        e.len() >= 120_000,
        "期望 12 万条以上，实际 {}",
        e.len()
    );
    assert_eq!(e.skipped_lines, 0, "真实码表不应有非法行");
}

#[test]
fn query_a_first_candidate_is_an() {
    let e = real_engine();
    let c = e.query("a", 9);
    assert!(!c.is_empty());
    assert_eq!(c[0].code, "a");
    assert_eq!(c[0].text, "安");
}

#[test]
fn query_hc_is_hao() {
    // 用户 bug 报告里的例子：组字 "hc" 时"好"应排第一（精确匹配优先），
    // 后面还有 hc 开头的其他候选（hca 滹、hcf 淮……共 564 条）
    let e = real_engine();
    let c = e.query("hc", 9);
    assert_eq!(c.len(), 9);
    assert_eq!(c[0].code, "hc");
    assert_eq!(c[0].text, "好");
    assert!(c.iter().all(|x| x.code.starts_with("hc")));
}

#[test]
fn query_aab_returns_phrases() {
    let e = real_engine();
    let c = e.query("aab", 9);
    assert!(!c.is_empty());
    assert!(c.iter().all(|x| x.code.starts_with("aab")));
    // 若有精确匹配（3 位码），它们必须排在更长的前缀匹配之前
    let first_long = c.iter().position(|x| x.code.len() > 3);
    if let Some(pos) = first_long {
        assert!(c[..pos].iter().all(|x| x.code.len() == 3));
    }
}

#[test]
fn perf_single_query_fast() {
    use std::time::Instant;
    let e = real_engine();
    let t = Instant::now();
    for _ in 0..1000 {
        let _ = e.query("aabq", 9);
    }
    let per = t.elapsed().as_micros() / 1000;
    assert!(per < 1000, "单次查询 {per}µs，太慢了");
}

#[test]
fn user_dict_appends() {
    // 用户词库追加在系统词库之后
    let e = Engine::load(&["data/mb.txt", "data/usercode.txt"]).unwrap();
    let c = e.query("cqzju", 9);
    assert!(!c.is_empty());
    assert_eq!(c[0].text, "重载");
}
