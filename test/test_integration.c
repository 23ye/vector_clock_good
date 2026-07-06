/*
 * 集成测试 - 验证完整流程
 *
 * 测试内容:
 * 1. 日志生成 → 加载 → 因果排序
 * 2. 多节点日志因果序验证
 * 3. 查询过滤功能
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "store.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define mkdir(path) mkdir(path, 0755)
#endif

/* 测试计数器 */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { printf("  TEST: %-50s", name); } while (0)

#define PASS() \
    do { printf("[PASS]\n"); tests_passed++; } while (0)

#define FAIL(msg) \
    do { printf("[FAIL] %s\n", msg); tests_failed++; } while (0)

#define ASSERT(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while (0)

/* 创建测试日志 */
static void create_test_logs(const char *dir)
{
    char filepath[512];
    FILE *fp;

    /* 创建目录 */
    mkdir(dir);

    /* Node-01 日志 */
    snprintf(filepath, sizeof(filepath), "%s/node1.jsonl", dir);
    fp = fopen(filepath, "w");
    if (fp) {
        /* A: VC=[1,0,0] */
        fprintf(fp, "{\"node_id\":\"node-01\",\"vector_clock\":{\"node-0\":1,\"node-1\":0,\"node-2\":0},"
                "\"timestamp\":1000,\"level\":\"INFO\",\"message\":\"A: Request received\"}\n");
        /* C: VC=[2,1,0] - 知道 B 发生了 */
        fprintf(fp, "{\"node_id\":\"node-01\",\"vector_clock\":{\"node-0\":2,\"node-1\":1,\"node-2\":0},"
                "\"timestamp\":3000,\"level\":\"INFO\",\"message\":\"C: Got response from node-02\"}\n");
        /* F: VC=[3,1,0] */
        fprintf(fp, "{\"node_id\":\"node-01\",\"vector_clock\":{\"node-0\":3,\"node-1\":1,\"node-2\":0},"
                "\"timestamp\":6000,\"level\":\"INFO\",\"message\":\"F: Request completed\"}\n");
        fclose(fp);
    }

    /* Node-02 日志 */
    snprintf(filepath, sizeof(filepath), "%s/node2.jsonl", dir);
    fp = fopen(filepath, "w");
    if (fp) {
        /* B: VC=[1,1,0] - 收到 A 的消息 */
        fprintf(fp, "{\"node_id\":\"node-02\",\"vector_clock\":{\"node-0\":1,\"node-1\":1,\"node-2\":0},"
                "\"timestamp\":2000,\"level\":\"INFO\",\"message\":\"B: Processing lookup\"}\n");
        /* D: VC=[1,2,0] */
        fprintf(fp, "{\"node_id\":\"node-02\",\"vector_clock\":{\"node-0\":1,\"node-1\":2,\"node-2\":0},"
                "\"timestamp\":4000,\"level\":\"WARN\",\"message\":\"D: Cache miss\"}\n");
        fclose(fp);
    }

    /* Node-03 日志 */
    snprintf(filepath, sizeof(filepath), "%s/node3.jsonl", dir);
    fp = fopen(filepath, "w");
    if (fp) {
        /* E: VC=[2,1,1] - 收到 C 的消息 */
        fprintf(fp, "{\"node_id\":\"node-03\",\"vector_clock\":{\"node-0\":2,\"node-1\":1,\"node-2\":1},"
                "\"timestamp\":5000,\"level\":\"INFO\",\"message\":\"E: Logging result\"}\n");
        /* G: VC=[2,1,2] */
        fprintf(fp, "{\"node_id\":\"node-03\",\"vector_clock\":{\"node-0\":2,\"node-1\":1,\"node-2\":2},"
                "\"timestamp\":7000,\"level\":\"DEBUG\",\"message\":\"G: Audit updated\"}\n");
        fclose(fp);
    }
}

/* 清理测试数据 */
static void cleanup_test_logs(const char *dir)
{
    char filepath[512];

    snprintf(filepath, sizeof(filepath), "%s/node1.jsonl", dir);
    remove(filepath);

    snprintf(filepath, sizeof(filepath), "%s/node2.jsonl", dir);
    remove(filepath);

    snprintf(filepath, sizeof(filepath), "%s/node3.jsonl", dir);
    remove(filepath);

    rmdir(dir);
}

/* 测试 1: 日志加载 */
void test_load_logs(void)
{
    TEST("Load logs from directory");

    log_store_t store;
    store_init(&store);

    int loaded = store_load_dir(&store, "test_data");

    ASSERT(loaded == 7, "Should load 7 logs");
    ASSERT(store.count == 7, "Store should have 7 entries");

    store_cleanup(&store);
    PASS();
}

/* 测试 2: 因果排序 */
void test_causal_sort(void)
{
    TEST("Causal sort preserves happens-before");

    log_store_t store;
    store_init(&store);
    store_load_dir(&store, "test_data");

    /* 打乱顺序加载 (模拟网络乱序) */
    store_sort_causal(&store);

    /* 验证因果序: A 必在 B 前 */
    int idx_A = -1, idx_B = -1, idx_C = -1, idx_D = -1, idx_E = -1;

    for (int i = 0; i < store.count; i++) {
        const char *msg = store.entries[i].message;
        if (strstr(msg, "A:")) idx_A = i;
        if (strstr(msg, "B:")) idx_B = i;
        if (strstr(msg, "C:")) idx_C = i;
        if (strstr(msg, "D:")) idx_D = i;
        if (strstr(msg, "E:")) idx_E = i;
    }

    ASSERT(idx_A >= 0 && idx_B >= 0, "A and B should exist");
    ASSERT(idx_A < idx_B, "A should come before B (A -> B)");
    ASSERT(idx_B < idx_D, "B should come before D (B -> D)");
    ASSERT(idx_A < idx_D, "A should come before D (transitivity)");
    ASSERT(idx_C < idx_E, "C should come before E (C -> E)");

    store_cleanup(&store);
    PASS();
}

/* 测试 3: 时间排序 */
void test_time_sort(void)
{
    TEST("Time sort");

    log_store_t store;
    store_init(&store);
    store_load_dir(&store, "test_data");

    store_sort_by_time(&store);

    /* 验证时间戳递增 */
    for (int i = 1; i < store.count; i++) {
        ASSERT(store.entries[i].timestamp >= store.entries[i-1].timestamp,
               "Timestamps should be non-decreasing");
    }

    store_cleanup(&store);
    PASS();
}

/* 测试 4: 关键词查询 */
void test_query_keyword(void)
{
    TEST("Query by keyword");

    log_store_t store;
    store_init(&store);
    store_load_dir(&store, "test_data");
    store_sort_causal(&store);

    query_t query;
    memset(&query, 0, sizeof(query));
    query.keyword = "cache";

    log_entry_t *results[100];
    int count = store_query(&store, &query, results, 100);

    ASSERT(count == 1, "Should find 1 log with 'cache'");
    ASSERT(strstr(results[0]->message, "Cache miss") != NULL,
           "Should be the cache miss log");

    store_cleanup(&store);
    PASS();
}

/* 测试 5: 节点查询 */
void test_query_node(void)
{
    TEST("Query by node ID");

    log_store_t store;
    store_init(&store);
    store_load_dir(&store, "test_data");

    query_t query;
    memset(&query, 0, sizeof(query));
    query.node_id = "node-02";

    log_entry_t *results[100];
    int count = store_query(&store, &query, results, 100);

    ASSERT(count == 2, "Should find 2 logs from node-02");

    for (int i = 0; i < count; i++) {
        ASSERT(strcmp(results[i]->node_id, "node-02") == 0,
               "All results should be from node-02");
    }

    store_cleanup(&store);
    PASS();
}

/* 测试 6: 级别查询 */
void test_query_level(void)
{
    TEST("Query by level");

    log_store_t store;
    store_init(&store);
    store_load_dir(&store, "test_data");

    query_t query;
    memset(&query, 0, sizeof(query));
    query.level = "WARN";

    log_entry_t *results[100];
    int count = store_query(&store, &query, results, 100);

    ASSERT(count == 1, "Should find 1 WARN log");
    ASSERT(results[0]->level == LOG_WARN, "Level should be WARN");

    store_cleanup(&store);
    PASS();
}

/* 测试 7: 组合查询 */
void test_query_combined(void)
{
    TEST("Combined query (node + level)");

    log_store_t store;
    store_init(&store);
    store_load_dir(&store, "test_data");

    query_t query;
    memset(&query, 0, sizeof(query));
    query.node_id = "node-01";
    query.level = "INFO";

    log_entry_t *results[100];
    int count = store_query(&store, &query, results, 100);

    ASSERT(count == 3, "Should find 3 INFO logs from node-01");

    store_cleanup(&store);
    PASS();
}

/* 测试 8: 统计信息 */
void test_stats(void)
{
    TEST("Statistics");

    log_store_t store;
    store_init(&store);
    store_load_dir(&store, "test_data");

    int total = 0;
    int node_counts[32] = {0};
    int node_count = 0;

    store_get_stats(&store, &total, node_counts, &node_count);

    ASSERT(total == 7, "Total should be 7");
    ASSERT(node_count > 0, "Should have node counts");

    store_cleanup(&store);
    PASS();
}

/* 主函数 */
int main(void)
{
    printf("========================================\n");
    printf("  LogAgg Integration Tests\n");
    printf("========================================\n\n");

    /* 创建测试数据 */
    printf("Creating test data...\n");
    create_test_logs("test_data");

    printf("\n[Tests]\n");
    test_load_logs();
    test_causal_sort();
    test_time_sort();
    test_query_keyword();
    test_query_node();
    test_query_level();
    test_query_combined();
    test_stats();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    /* 清理测试数据 */
    cleanup_test_logs("test_data");

    return tests_failed > 0 ? 1 : 0;
}