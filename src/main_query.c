#include "store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 打印使用说明 */
static void print_usage(const char *prog_name)
{
    printf("LogAgg Query - Log Query Tool\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s -d <dir> [options]\n", prog_name);
    printf("\n");
    printf("Required:\n");
    printf("  -d <dir>        Log storage directory\n");
    printf("\n");
    printf("Options:\n");
    printf("  -k <keyword>    Filter by keyword\n");
    printf("  -n <node_id>    Filter by node ID\n");
    printf("  -l <level>      Filter by level (DEBUG/INFO/WARN/ERROR/FATAL)\n");
    printf("  -s <timestamp>  Start time (Unix ms)\n");
    printf("  -e <timestamp>  End time (Unix ms)\n");
    printf("  -c <count>      Max results to show (default: 100)\n");
    printf("  --sort <mode>   Sort mode: causal, time, node (default: causal)\n");
    printf("  --stats         Show statistics only\n");
    printf("  -h              Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -d ./logs\n", prog_name);
    printf("  %s -d ./logs -k \"error\"\n", prog_name);
    printf("  %s -d ./logs -n node-01 -l ERROR\n", prog_name);
    printf("  %s -d ./logs --sort time -c 50\n", prog_name);
    printf("  %s -d ./logs --stats\n", prog_name);
}

/* 打印统计信息 */
static void print_stats(const log_store_t *store)
{
    int total = 0;
    int node_counts[32] = {0};
    int node_count = 0;

    store_get_stats(store, &total, node_counts, &node_count);

    printf("\n========== Log Statistics ==========\n");
    printf("  Total Logs: %d\n", total);
    printf("\n  Logs by Node:\n");

    for (int i = 0; i < node_count && i < 32; i++) {
        if (node_counts[i] > 0) {
            printf("    node-%02d: %d\n", i, node_counts[i]);
        }
    }

    printf("====================================\n");
}

int main(int argc, char *argv[])
{
    char storage_dir[256] = {0};
    query_t query;
    memset(&query, 0, sizeof(query));

    int max_results = 100;
    sort_mode_t sort_mode = SORT_CAUSAL;
    bool stats_only = false;

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -d requires an argument\n");
                return 1;
            }
            strncpy(storage_dir, argv[++i], sizeof(storage_dir) - 1);
        } else if (strcmp(argv[i], "-k") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -k requires an argument\n");
                return 1;
            }
            query.keyword = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -n requires an argument\n");
                return 1;
            }
            query.node_id = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -l requires an argument\n");
                return 1;
            }
            query.level = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -s requires an argument\n");
                return 1;
            }
            query.time_start = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -e requires an argument\n");
                return 1;
            }
            query.time_end = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -c requires an argument\n");
                return 1;
            }
            max_results = atoi(argv[++i]);
            if (max_results <= 0) max_results = 100;
        } else if (strcmp(argv[i], "--sort") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --sort requires an argument\n");
                return 1;
            }
            i++;
            if (strcmp(argv[i], "causal") == 0) {
                sort_mode = SORT_CAUSAL;
            } else if (strcmp(argv[i], "time") == 0) {
                sort_mode = SORT_TIME;
            } else if (strcmp(argv[i], "node") == 0) {
                sort_mode = SORT_NODE;
            } else {
                fprintf(stderr, "Error: Unknown sort mode '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--stats") == 0) {
            stats_only = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* 检查必需参数 */
    if (storage_dir[0] == '\0') {
        fprintf(stderr, "Error: Storage directory (-d) is required\n");
        print_usage(argv[0]);
        return 1;
    }

    /* 初始化存储 */
    log_store_t store;
    if (store_init(&store) != 0) {
        return 1;
    }

    /* 加载日志 */
    printf("Loading logs from %s...\n", storage_dir);
    int loaded = store_load_dir(&store, storage_dir);
    printf("Loaded %d logs\n", loaded);

    if (loaded == 0) {
        printf("No logs found.\n");
        store_cleanup(&store);
        return 0;
    }

    /* 排序 */
    printf("Sorting logs (%s)...\n",
           sort_mode == SORT_CAUSAL ? "causal" :
           sort_mode == SORT_TIME ? "time" : "node");

    switch (sort_mode) {
        case SORT_CAUSAL:
            store_sort_causal(&store);
            break;
        case SORT_TIME:
            store_sort_by_time(&store);
            break;
        case SORT_NODE:
            /* store_sort_by_node not exposed in header, use time */
            store_sort_by_time(&store);
            break;
    }

    /* 统计模式 */
    if (stats_only) {
        print_stats(&store);
        store_cleanup(&store);
        return 0;
    }

    /* 查询 */
    log_entry_t **results = malloc(max_results * sizeof(log_entry_t *));
    if (!results) {
        fprintf(stderr, "Error: Failed to allocate results array\n");
        store_cleanup(&store);
        return 1;
    }

    int result_count = store_query(&store, &query, results, max_results);

    /* 打印结果 */
    if (result_count > 0) {
        printf("\nQuery Results:\n");
        printf("  Keyword: %s\n", query.keyword ? query.keyword : "(any)");
        printf("  Node:    %s\n", query.node_id ? query.node_id : "(any)");
        printf("  Level:   %s\n", query.level ? query.level : "(any)");
        printf("  Results: %d\n\n", result_count);

        printf("%-5s | %-23s | %-5s | %-8s | %-15s | %s\n",
               "Index", "Timestamp", "Level", "Node", "Vector Clock", "Message");
        printf("------+-------------------------+-------+----------+-----------------+--------\n");

        for (int i = 0; i < result_count; i++) {
            store_print_entry(results[i], i);
        }
    } else {
        printf("\nNo matching logs found.\n");
    }

    /* 清理 */
    free(results);
    store_cleanup(&store);

    return 0;
}