//单元测试
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vector_clock.h"
#include "log_entry.h"

/* 测试计数器 */
static int tests_passed = 0;
static int tests_failed = 0;

/* 测试宏 */
#define TEST(name) \
    do { \
        printf("  TEST: %-50s", name); \
    } while (0)

#define PASS() \
    do { \
        printf("[PASS]\n"); \
        tests_passed++; \
    } while (0)

#define FAIL(msg) \
    do { \
        printf("[FAIL] %s\n", msg); \
        tests_failed++; \
    } while (0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            FAIL(msg); \
            return; \
        } \
    } while (0)

/* ==================== 向量时钟测试 ==================== */

void test_vc_init(void)
{
    TEST("vc_init");

    vector_clock_t vc;
    vc_init(&vc, 3);

    ASSERT(vc.node_count == 3, "node_count should be 3");
    ASSERT(vc.clocks[0] == 0, "clock[0] should be 0");
    ASSERT(vc.clocks[1] == 0, "clock[1] should be 0");
    ASSERT(vc.clocks[2] == 0, "clock[2] should be 0");

    PASS();
}

void test_vc_init_invalid(void)
{
    TEST("vc_init (invalid params)");

    /* NULL 指针不崩溃 */
    vc_init(NULL, 3);

    /* 节点数为 0 */
    vector_clock_t vc;
    vc_init(&vc, 0);
    ASSERT(vc.node_count == 0, "node_count should be 0");

    /* 节点数超过最大值 */
    vc_init(&vc, VC_MAX_NODES + 1);
    ASSERT(vc.node_count == 0, "node_count should be 0 for overflow");

    PASS();
}

void test_vc_increment(void)
{
    TEST("vc_increment");

    vector_clock_t vc;
    vc_init(&vc, 3);

    vc_increment(&vc, 0);
    ASSERT(vc.clocks[0] == 1, "clock[0] should be 1");

    vc_increment(&vc, 0);
    ASSERT(vc.clocks[0] == 2, "clock[0] should be 2");

    vc_increment(&vc, 2);
    ASSERT(vc.clocks[2] == 1, "clock[2] should be 1");
    ASSERT(vc.clocks[1] == 0, "clock[1] should still be 0");

    PASS();
}

void test_vc_increment_invalid(void)
{
    TEST("vc_increment (invalid params)");

    vector_clock_t vc;
    vc_init(&vc, 3);

    /* 无效节点 ID 不崩溃 */
    vc_increment(&vc, -1);
    vc_increment(&vc, 3);
    vc_increment(&vc, 100);

    /* NULL 指针不崩溃 */
    vc_increment(NULL, 0);

    PASS();
}

void test_vc_merge(void)
{
    TEST("vc_merge");

    vector_clock_t a, b;
    vc_init(&a, 3);
    vc_init(&b, 3);

    a.clocks[0] = 3;
    a.clocks[1] = 1;
    a.clocks[2] = 2;

    b.clocks[0] = 1;
    b.clocks[1] = 4;
    b.clocks[2] = 2;

    vc_merge(&a, &b);

    ASSERT(a.clocks[0] == 3, "merged clock[0] should be 3");
    ASSERT(a.clocks[1] == 4, "merged clock[1] should be 4");
    ASSERT(a.clocks[2] == 2, "merged clock[2] should be 2");

    PASS();
}

void test_vc_merge_different_sizes(void)
{
    TEST("vc_merge (different sizes)");

    vector_clock_t a, b;
    vc_init(&a, 3);
    vc_init(&b, 2);

    a.clocks[0] = 1;
    a.clocks[1] = 2;
    a.clocks[2] = 3;

    b.clocks[0] = 4;
    b.clocks[1] = 1;

    vc_merge(&a, &b);

    ASSERT(a.clocks[0] == 4, "merged clock[0] should be 4");
    ASSERT(a.clocks[1] == 2, "merged clock[1] should be 2");
    ASSERT(a.clocks[2] == 3, "merged clock[2] should be 3");

    PASS();
}

void test_vc_compare_equal(void)
{
    TEST("vc_compare (equal)");

    vector_clock_t a, b;
    vc_init(&a, 3);
    vc_init(&b, 3);

    a.clocks[0] = 1;
    a.clocks[1] = 2;
    a.clocks[2] = 3;

    b.clocks[0] = 1;
    b.clocks[1] = 2;
    b.clocks[2] = 3;

    ASSERT(vc_compare(&a, &b) == VC_EQUAL, "should be equal");

    PASS();
}

void test_vc_compare_happens_before(void)
{
    TEST("vc_compare (happens-before)");

    vector_clock_t a, b;
    vc_init(&a, 3);
    vc_init(&b, 3);

    /* a = [1, 0, 0], b = [1, 1, 0] => a happens-before b */
    a.clocks[0] = 1;
    b.clocks[0] = 1;
    b.clocks[1] = 1;

    ASSERT(vc_compare(&a, &b) == VC_HAPPENS_BEFORE, "a should happen before b");

    PASS();
}

void test_vc_compare_happens_after(void)
{
    TEST("vc_compare (happens-after)");

    vector_clock_t a, b;
    vc_init(&a, 3);
    vc_init(&b, 3);

    /* a = [2, 1, 1], b = [1, 1, 0] => a happens-after b */
    a.clocks[0] = 2;
    a.clocks[1] = 1;
    a.clocks[2] = 1;

    b.clocks[0] = 1;
    b.clocks[1] = 1;

    ASSERT(vc_compare(&a, &b) == VC_HAPPENS_AFTER, "a should happen after b");

    PASS();
}

void test_vc_compare_concurrent(void)
{
    TEST("vc_compare (concurrent)");

    vector_clock_t a, b;
    vc_init(&a, 3);
    vc_init(&b, 3);

    /* a = [2, 1, 0], b = [1, 2, 0] => concurrent */
    a.clocks[0] = 2;
    a.clocks[1] = 1;

    b.clocks[0] = 1;
    b.clocks[1] = 2;

    ASSERT(vc_compare(&a, &b) == VC_CONCURRENT, "should be concurrent");

    PASS();
}

void test_vc_copy(void)
{
    TEST("vc_copy");

    vector_clock_t src, dst;
    vc_init(&src, 3);
    vc_init(&dst, 3);

    src.clocks[0] = 5;
    src.clocks[1] = 3;
    src.clocks[2] = 7;

    vc_copy(&dst, &src);

    ASSERT(dst.clocks[0] == 5, "copied clock[0] should be 5");
    ASSERT(dst.clocks[1] == 3, "copied clock[1] should be 3");
    ASSERT(dst.clocks[2] == 7, "copied clock[2] should be 7");

    PASS();
}

void test_vc_to_string(void)
{
    TEST("vc_to_string");

    vector_clock_t vc;
    vc_init(&vc, 3);
    vc.clocks[0] = 1;
    vc.clocks[1] = 2;
    vc.clocks[2] = 3;

    char buf[64];
    int ret = vc_to_string(&vc, buf, sizeof(buf));

    ASSERT(ret > 0, "return value should be positive");
    ASSERT(strcmp(buf, "[1, 2, 3]") == 0, "string should be [1, 2, 3]");

    PASS();
}

void test_vc_to_json(void)
{
    TEST("vc_to_json");

    vector_clock_t vc;
    vc_init(&vc, 3);
    vc.clocks[0] = 5;
    vc.clocks[1] = 3;
    vc.clocks[2] = 7;

    char buf[128];
    int ret = vc_to_json(&vc, NULL, buf, sizeof(buf));

    ASSERT(ret > 0, "return value should be positive");
    ASSERT(strstr(buf, "\"node-0\":5") != NULL, "should contain node-0:5");
    ASSERT(strstr(buf, "\"node-1\":3") != NULL, "should contain node-1:3");
    ASSERT(strstr(buf, "\"node-2\":7") != NULL, "should contain node-2:7");

    PASS();
}

void test_vc_from_json(void)
{
    TEST("vc_from_json");

    vector_clock_t vc;
    const char *json = "{\"node-0\":5,\"node-1\":3,\"node-2\":7}";

    int ret = vc_from_json(&vc, json, NULL);

    ASSERT(ret == 0, "parse should succeed");
    ASSERT(vc.node_count == 3, "node_count should be 3");
    ASSERT(vc.clocks[0] == 5, "clock[0] should be 5");
    ASSERT(vc.clocks[1] == 3, "clock[1] should be 3");
    ASSERT(vc.clocks[2] == 7, "clock[2] should be 7");

    PASS();
}

void test_vc_json_roundtrip(void)
{
    TEST("vc_json roundtrip");

    vector_clock_t original, parsed;
    vc_init(&original, 3);
    original.clocks[0] = 10;
    original.clocks[1] = 20;
    original.clocks[2] = 30;

    char buf[128];
    vc_to_json(&original, NULL, buf, sizeof(buf));

    int ret = vc_from_json(&parsed, buf, NULL);

    ASSERT(ret == 0, "parse should succeed");
    ASSERT(vc_compare(&original, &parsed) == VC_EQUAL, "roundtrip should preserve values");

    PASS();
}

void test_vc_is_valid(void)
{
    TEST("vc_is_valid");

    vector_clock_t vc;
    vc_init(&vc, 3);
    ASSERT(vc_is_valid(&vc), "should be valid");

    vc_init(&vc, 0);
    ASSERT(!vc_is_valid(&vc), "0 nodes should be invalid");

    ASSERT(!vc_is_valid(NULL), "NULL should be invalid");

    PASS();
}

void test_vc_relation_str(void)
{
    TEST("vc_relation_str");

    ASSERT(strcmp(vc_relation_str(VC_EQUAL), "EQUAL") == 0, "EQUAL");
    ASSERT(strcmp(vc_relation_str(VC_HAPPENS_BEFORE), "HAPPENS_BEFORE") == 0, "HAPPENS_BEFORE");
    ASSERT(strcmp(vc_relation_str(VC_HAPPENS_AFTER), "HAPPENS_AFTER") == 0, "HAPPENS_AFTER");
    ASSERT(strcmp(vc_relation_str(VC_CONCURRENT), "CONCURRENT") == 0, "CONCURRENT");

    PASS();
}

/* ==================== 日志条目测试 ==================== */

void test_log_entry_init(void)
{
    TEST("log_entry_init");

    log_entry_t entry;
    vector_clock_t vc;
    vc_init(&vc, 3);
    vc.clocks[0] = 1;

    log_entry_init(&entry, "node-01", &vc, LOG_INFO, "Test message");

    ASSERT(strcmp(entry.node_id, "node-01") == 0, "node_id should be node-01");
    ASSERT(entry.vc.clocks[0] == 1, "vc should be copied");
    ASSERT(entry.level == LOG_INFO, "level should be INFO");
    ASSERT(strcmp(entry.message, "Test message") == 0, "message should match");
    ASSERT(entry.timestamp > 0, "timestamp should be set");

    PASS();
}

void test_log_level_from_str(void)
{
    TEST("log_level_from_str");

    ASSERT(log_level_from_str("DEBUG") == LOG_DEBUG, "DEBUG");
    ASSERT(log_level_from_str("INFO") == LOG_INFO, "INFO");
    ASSERT(log_level_from_str("WARN") == LOG_WARN, "WARN");
    ASSERT(log_level_from_str("ERROR") == LOG_ERROR, "ERROR");
    ASSERT(log_level_from_str("FATAL") == LOG_FATAL, "FATAL");
    ASSERT(log_level_from_str("unknown") == LOG_INFO, "unknown should default to INFO");
    ASSERT(log_level_from_str(NULL) == LOG_INFO, "NULL should default to INFO");

    PASS();
}

void test_log_entry_metadata(void)
{
    TEST("log_entry_metadata");

    log_entry_t entry;
    vector_clock_t vc;
    vc_init(&vc, 3);

    log_entry_init(&entry, "node-01", &vc, LOG_INFO, "Test");

    int ret = log_entry_add_metadata(&entry, "trace_id", "abc-123");
    ASSERT(ret == 0, "add_metadata should succeed");

    ret = log_entry_add_metadata(&entry, "request_id", "req-456");
    ASSERT(ret == 0, "add_metadata should succeed");

    const char *val = log_entry_get_metadata(&entry, "trace_id");
    ASSERT(val != NULL, "get_metadata should return non-NULL");
    ASSERT(strcmp(val, "abc-123") == 0, "trace_id should be abc-123");

    val = log_entry_get_metadata(&entry, "request_id");
    ASSERT(val != NULL, "get_metadata should return non-NULL");
    ASSERT(strcmp(val, "req-456") == 0, "request_id should be req-456");

    val = log_entry_get_metadata(&entry, "nonexistent");
    ASSERT(val == NULL, "nonexistent key should return NULL");

    PASS();
}

void test_log_entry_to_json(void)
{
    TEST("log_entry_to_json");

    log_entry_t entry;
    vector_clock_t vc;
    vc_init(&vc, 3);
    vc.clocks[0] = 1;
    vc.clocks[1] = 2;
    vc.clocks[2] = 3;

    log_entry_init(&entry, "node-01", &vc, LOG_INFO, "Test message");
    entry.timestamp = 1719648000000ULL;

    char buf[1024];
    int ret = log_entry_to_json(&entry, buf, sizeof(buf));

    ASSERT(ret > 0, "to_json should return positive");
    ASSERT(strstr(buf, "\"node_id\":\"node-01\"") != NULL, "should contain node_id");
    ASSERT(strstr(buf, "\"level\":\"INFO\"") != NULL, "should contain level");
    ASSERT(strstr(buf, "\"message\":\"Test message\"") != NULL, "should contain message");
    ASSERT(strstr(buf, "\"timestamp\":1719648000000") != NULL, "should contain timestamp");

    PASS();
}

void test_log_entry_from_json(void)
{
    TEST("log_entry_from_json");

    const char *json = "{\"node_id\":\"node-01\",\"vector_clock\":{\"node-0\":1,\"node-1\":2,\"node-2\":3},"
                       "\"timestamp\":1719648000000,\"level\":\"INFO\",\"message\":\"Test message\"}";

    log_entry_t entry;
    int ret = log_entry_from_json(&entry, json);

    ASSERT(ret == 0, "from_json should succeed");
    ASSERT(strcmp(entry.node_id, "node-01") == 0, "node_id should be node-01");
    ASSERT(entry.timestamp == 1719648000000ULL, "timestamp should match");
    ASSERT(entry.level == LOG_INFO, "level should be INFO");
    ASSERT(strcmp(entry.message, "Test message") == 0, "message should match");

    PASS();
}

void test_log_entry_compare(void)
{
    TEST("log_entry_compare");

    log_entry_t a, b;
    vector_clock_t vc_a, vc_b;

    /* a happens-before b */
    vc_init(&vc_a, 3);
    vc_a.clocks[0] = 1;

    vc_init(&vc_b, 3);
    vc_b.clocks[0] = 1;
    vc_b.clocks[1] = 1;

    log_entry_init(&a, "node-0", &vc_a, LOG_INFO, "A");
    log_entry_init(&b, "node-1", &vc_b, LOG_INFO, "B");

    ASSERT(log_entry_compare(&a, &b) < 0, "a should come before b");
    ASSERT(log_entry_compare(&b, &a) > 0, "b should come after a");

    PASS();
}

void test_timestamp_functions(void)
{
    TEST("timestamp functions");

    uint64_t ts = get_current_timestamp_ms();
    ASSERT(ts > 0, "timestamp should be positive");

    char buf[64];
    int ret = timestamp_to_string(1719648000000ULL, buf, sizeof(buf));
    ASSERT(ret > 0, "timestamp_to_string should return positive");
    ASSERT(strstr(buf, "2024") != NULL, "should contain year 2024");

    PASS();
}

/* ==================== 经典场景测试 ==================== */

void test_scenario_causal_order(void)
{
    TEST("scenario: causal ordering");

    /*
     * 场景：
     *   Node-0: [A]──────[C]────────[F]
     *   Node-1: ───[B]──────[D]──
     *   Node-2: ──────[E]──────────[G]
     *
     * 因果关系:
     *   A → B (Node-0 发消息给 Node-1)
     *   B → D (Node-1 内部)
     *   C → E (Node-0 发消息给 Node-2)
     */

    vector_clock_t vc_a, vc_b, vc_c, vc_d, vc_e, vc_f, vc_g;
    vc_init(&vc_a, 3); vc_init(&vc_b, 3); vc_init(&vc_c, 3);
    vc_init(&vc_d, 3); vc_init(&vc_e, 3); vc_init(&vc_f, 3);
    vc_init(&vc_g, 3);

    /* A: Node-0 本地事件 */
    vc_increment(&vc_a, 0);  /* [1,0,0] */

    /* B: Node-0 发消息给 Node-1，Node-1 接收 */
    vc_copy(&vc_b, &vc_a);
    vc_merge(&vc_b, &vc_a);
    vc_increment(&vc_b, 1);  /* [1,1,0] */

    /* C: Node-0 本地事件 */
    vc_copy(&vc_c, &vc_a);
    vc_increment(&vc_c, 0);  /* [2,1,0] (merge with B's knowledge) */
    vc_merge(&vc_c, &vc_b);
    vc_increment(&vc_c, 0);  /* 实际应该是 [2,1,0] + increment = [3,1,0] */
    /* 修正：C 是 A 之后的本地事件，不需要 merge B */
    vc_copy(&vc_c, &vc_a);
    vc_increment(&vc_c, 0);  /* [2,0,0] */

    /* D: Node-1 本地事件 */
    vc_copy(&vc_d, &vc_b);
    vc_increment(&vc_d, 1);  /* [1,2,0] */

    /* E: Node-0 发消息给 Node-2，Node-2 接收 */
    vc_copy(&vc_e, &vc_c);
    vc_merge(&vc_e, &vc_c);
    vc_increment(&vc_e, 2);  /* [2,0,1] */

    /* F: Node-0 本地事件 */
    vc_copy(&vc_f, &vc_c);
    vc_increment(&vc_f, 0);  /* [3,0,0] */

    /* G: Node-2 本地事件 */
    vc_copy(&vc_g, &vc_e);
    vc_increment(&vc_g, 2);  /* [2,0,2] */

    /* 验证因果关系 */
    ASSERT(vc_compare(&vc_a, &vc_b) == VC_HAPPENS_BEFORE, "A -> B");
    ASSERT(vc_compare(&vc_b, &vc_d) == VC_HAPPENS_BEFORE, "B -> D");
    ASSERT(vc_compare(&vc_a, &vc_d) == VC_HAPPENS_BEFORE, "A -> D (transitivity)");
    ASSERT(vc_compare(&vc_c, &vc_e) == VC_HAPPENS_BEFORE, "C -> E");
    ASSERT(vc_compare(&vc_a, &vc_f) == VC_HAPPENS_BEFORE, "A -> F");

    /* 验证并发关系 */
    ASSERT(vc_compare(&vc_d, &vc_f) == VC_CONCURRENT, "D || F (concurrent)");
    ASSERT(vc_compare(&vc_d, &vc_g) == VC_CONCURRENT, "D || G (concurrent)");

    PASS();
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("========================================\n");
    printf("  LogAgg Unit Tests - Day 1\n");
    printf("========================================\n\n");

    printf("[Vector Clock Tests]\n");
    test_vc_init();
    test_vc_init_invalid();
    test_vc_increment();
    test_vc_increment_invalid();
    test_vc_merge();
    test_vc_merge_different_sizes();
    test_vc_compare_equal();
    test_vc_compare_happens_before();
    test_vc_compare_happens_after();
    test_vc_compare_concurrent();
    test_vc_copy();
    test_vc_to_string();
    test_vc_to_json();
    test_vc_from_json();
    test_vc_json_roundtrip();
    test_vc_is_valid();
    test_vc_relation_str();

    printf("\n[Log Entry Tests]\n");
    test_log_entry_init();
    test_log_level_from_str();
    test_log_entry_metadata();
    test_log_entry_to_json();
    test_log_entry_from_json();
    test_log_entry_compare();
    test_timestamp_functions();

    printf("\n[Scenario Tests]\n");
    test_scenario_causal_order();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}