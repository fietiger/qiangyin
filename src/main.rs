//! 演示程序：在终端里模拟输入法的候选过程。
//!
//! 用法：
//!   cargo run -- data/mb.txt            # 然后逐行输入编码，空行跳过，输入 exit 退出
//!   echo hc | cargo run -- data/mb.txt
//!
//!   echo "aab" | cargo run -- data/mb.txt data/usercode.txt   # 追加用户词库

use qiangyin::Engine;
use std::io::{self, BufRead};

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.is_empty() {
        eprintln!("用法: qiangyin-demo <码表文件> [更多码表文件...]");
        std::process::exit(1);
    }
    let paths: Vec<&str> = args.iter().map(String::as_str).collect();
    let engine = Engine::load(&paths).unwrap_or_else(|e| {
        eprintln!("加载失败: {e}");
        std::process::exit(1);
    });
    eprintln!(
        "已加载 {} 条（跳过 {} 行非法数据）",
        engine.len(),
        engine.skipped_lines
    );
    eprintln!("输入编码查看候选（每页 9 个），exit 退出：");

    let stdin = io::stdin();
    for line in stdin.lock().lines() {
        let q = line.unwrap_or_default();
        let q = q.trim();
        if q.is_empty() {
            continue;
        }
        if q.eq_ignore_ascii_case("exit") || q.eq_ignore_ascii_case("quit") {
            break;
        }
        let cands = engine.query(q, 9);
        if cands.is_empty() {
            println!("  （无候选）");
            continue;
        }
        for (i, e) in cands.iter().enumerate() {
            println!("  {}. {}  [{}]", i + 1, e.text, e.code);
        }
    }
}
