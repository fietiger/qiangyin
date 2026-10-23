/* test_ffi.c — C ABI 端到端验证
 *
 * 编译运行：
 *   cargo build
 *   gcc ffi/test_ffi.c -o /tmp/test_ffi -Iffi -Ltarget/debug -lqiangyin
 *   LD_LIBRARY_PATH=target/debug /tmp/test_ffi
 */
#include <stdio.h>
#include "qiangyin.h"

int main(void) {
    QyEngine *e = qy_new();
    if (!e) {
        fprintf(stderr, "qy_new failed\n");
        return 1;
    }

    const char *paths[] = {"data/mb.txt", "data/usercode.txt"};
    long n = qy_load(e, paths, 2);
    printf("loaded %ld entries (qy_count=%zu)\n", n, qy_count(e));

    /* 空指针鲁棒性 */
    printf("null query -> %zu\n", qy_query(NULL, "hc", 9));
    printf("null text  -> %p\n", (void *)qy_candidate_text(NULL, 0));

    const char *queries[] = {"hc", "aab", "cqzju", "zzzz", ""};
    for (int q = 0; q < 5; q++) {
        size_t cnt = qy_query(e, queries[q], 5);
        printf("[%s] %zu candidates:\n", queries[q], cnt);
        for (size_t i = 0; i < cnt; i++) {
            printf("  %zu. %s [%s]\n", i + 1,
                   qy_candidate_text(e, i), qy_candidate_code(e, i));
        }
    }
    /* 越界读取 */
    printf("oob -> %p\n", (void *)qy_candidate_text(e, 99999));

    qy_free(e);
    qy_free(NULL); /* 空释放不崩溃 */
    printf("OK\n");
    return 0;
}
