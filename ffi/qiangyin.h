/* qiangyin.h — 强音输入法引擎 C ABI
 *
 * 链接 target/{debug,release}/libqiangyin.so（Linux）
 * 或 target/.../qiangyin.dll（Windows，见 ffi/README.md）。
 *
 * 字符串约定：
 * - 所有字符串为 UTF-8、C 风格 NUL 结尾；
 * - qy_candidate_text/code 返回的指针在下一次 qy_query 或 qy_free
 *   之前有效，如需长期持有请自行拷贝；
 * - 任何函数收到 NULL 都不会崩溃。
 */
#ifndef QIANGYIN_H
#define QIANGYIN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明句柄 */
typedef struct QyEngine QyEngine;

/* 创建引擎；失败返回 NULL */
QyEngine *qy_new(void);

/* 加载码表文件，可多次调用追加。返回累计条目数，失败返回 -1 */
long qy_load(QyEngine *e, const char *const *paths, size_t n);

/* 前缀查询，返回候选个数（<= limit） */
size_t qy_query(QyEngine *e, const char *prefix, size_t limit);

/* 第 i 个候选的文本 / 编码；越界返回 NULL */
const char *qy_candidate_text(const QyEngine *e, size_t i);
const char *qy_candidate_code(const QyEngine *e, size_t i);

/* 已加载总条目数 */
size_t qy_count(const QyEngine *e);

/* 释放引擎 */
void qy_free(QyEngine *e);

#ifdef __cplusplus
}
#endif

#endif /* QIANGYIN_H */
