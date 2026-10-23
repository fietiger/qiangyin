//! 超强音形输入法查词引擎原型。
//!
//! - 从文本码表（`编码 文本`，每行一条）加载全部条目
//! - 用 trie 按编码建立索引，支持按键过程中的前缀查询
//! - 候选排序：精确匹配（编码长度 == 输入长度）优先，其余按码表原有顺序
//!
//! 这是纯逻辑层：不依赖任何操作系统 API，将来 TSF 外壳（C++）
//! 可以通过 FFI（cdylib）调用它，或把逻辑平移到 C++。

use std::fs::File;
use std::io::{BufRead, BufReader};

/// 码表中的一条记录。
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry {
    /// 字母编码，如 "hc"（仅 a-z）
    pub code: String,
    /// 对应的汉字或词组，如 "好"
    pub text: String,
}

/// trie 节点。每个节点记录其子树下所有条目下标（按码表文件顺序），
/// 这样前缀查询直接取到有序候选，无需遍历收集再排序。
struct Node {
    children: [Option<Box<Node>>; 26],
    subtree: Vec<usize>,
}

impl Node {
    fn new() -> Self {
        Self {
            children: Default::default(),
            subtree: Vec::new(),
        }
    }
}

/// 查词引擎：加载一次，多次查询。
pub struct Engine {
    entries: Vec<Entry>,
    root: Node,
    /// 加载时跳过的非法行数（空行、注释、编码非法等）
    pub skipped_lines: usize,
}

impl Engine {
    /// 从一个或多个码表文件加载（后面的文件追加在后面，用户词库可放在最后）。
    ///
    /// 文件格式：每行 `编码 文本`（空格分隔），`#` 开头为注释，空行忽略。
    pub fn load(paths: &[&str]) -> Result<Self, String> {
        let mut engine = Engine {
            entries: Vec::new(),
            root: Node::new(),
            skipped_lines: 0,
        };
        for path in paths {
            let file = File::open(path).map_err(|e| format!("打不开 {path}: {e}"))?;
            for (lineno, line) in BufReader::new(file).lines().enumerate() {
                let line = line.map_err(|e| format!("{path} 第 {} 行读取失败: {e}", lineno + 1))?;
                let line = line.trim();
                if line.is_empty() || line.starts_with('#') {
                    continue;
                }
                let mut parts = line.splitn(2, char::is_whitespace);
                let code = parts.next().unwrap_or("");
                let text = parts.next().unwrap_or("").trim();
                if !is_valid_code(code) || text.is_empty() {
                    engine.skipped_lines += 1;
                    continue;
                }
                let idx = engine.entries.len();
                engine.entries.push(Entry {
                    code: code.to_string(),
                    text: text.to_string(),
                });
                engine.insert(idx, code.as_bytes());
            }
        }
        Ok(engine)
    }

    fn insert(&mut self, idx: usize, code: &[u8]) {
        let mut node = &mut self.root;
        node.subtree.push(idx);
        for &b in code {
            let child = &mut node.children[(b - b'a') as usize];
            let child = child.get_or_insert_with(|| Box::new(Node::new()));
            child.subtree.push(idx);
            node = child;
        }
    }

    /// 前缀查询：返回最多 `limit` 个候选。
    ///
    /// - 输入会被转小写；空串、含非 a-z 字符、或无匹配时返回空
    /// - 排序：编码与输入等长的精确匹配在前，其余保持码表原顺序
    pub fn query(&self, prefix: &str, limit: usize) -> Vec<&Entry> {
        if limit == 0 {
            return Vec::new();
        }
        let prefix = prefix.to_ascii_lowercase();
        if prefix.is_empty() || !prefix.bytes().all(|b| b.is_ascii_lowercase()) {
            return Vec::new();
        }
        let mut node = &self.root;
        for &b in prefix.as_bytes() {
            match &node.children[(b - b'a') as usize] {
                Some(child) => node = child,
                None => return Vec::new(),
            }
        }
        let plen = prefix.len();
        let mut exact = Vec::new();
        let mut rest = Vec::new();
        for &idx in node.subtree.iter() {
            // subtree 已按文件顺序排列，这里做稳定分区
            let e = &self.entries[idx];
            if e.code.len() == plen {
                exact.push(e);
            } else {
                rest.push(e);
            }
            if exact.len() + rest.len() >= limit {
                break;
            }
        }
        exact.into_iter().chain(rest).take(limit).collect()
    }

    /// 已加载的条目总数。
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }
}

fn is_valid_code(code: &str) -> bool {
    !code.is_empty() && code.bytes().all(|b| b.is_ascii_lowercase())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tiny_engine() -> Engine {
        // 构造一个内存小表，不依赖外部文件
        let entries = vec![
            ("a", "安"),
            ("hc", "好"),
            ("haa", "哈"),
            ("hcb", "号"),
            ("hc", "喝"), // 同码多字：保持文件顺序
        ];
        let mut engine = Engine {
            entries: entries
                .into_iter()
                .map(|(c, t)| Entry {
                    code: c.to_string(),
                    text: t.to_string(),
                })
                .collect(),
            root: Node::new(),
            skipped_lines: 0,
        };
        for idx in 0..engine.entries.len() {
            let code = engine.entries[idx].code.clone();
            engine.insert(idx, code.as_bytes());
        }
        engine
    }

    #[test]
    fn exact_match_first() {
        let e = tiny_engine();
        let c = e.query("hc", 10);
        assert_eq!(c.len(), 3);
        // 精确匹配（长度==2）在前，保持文件顺序
        assert_eq!(c[0].text, "好");
        assert_eq!(c[1].text, "喝");
        assert_eq!(c[2].text, "号"); // hcb 是前缀匹配，排后
    }

    #[test]
    fn prefix_match() {
        let e = tiny_engine();
        let c = e.query("h", 10);
        assert_eq!(c.len(), 4);
        assert!(c.iter().all(|x| x.code.starts_with('h')));
    }

    #[test]
    fn invalid_input_empty() {
        let e = tiny_engine();
        assert!(e.query("", 10).is_empty());
        assert!(e.query("H1", 10).is_empty());
        assert!(e.query("xyz", 10).is_empty());
    }

    #[test]
    fn limit_respected() {
        let e = tiny_engine();
        assert_eq!(e.query("h", 2).len(), 2);
        assert!(e.query("h", 0).is_empty());
    }

    #[test]
    fn case_insensitive() {
        let e = tiny_engine();
        assert_eq!(e.query("HC", 10)[0].text, "好");
    }
}
