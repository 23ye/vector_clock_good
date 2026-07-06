#include "store.h"
#include "i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* 打印使用说明 */
static void print_usage(const char *prog_name)
{
    printf("LogAgg Query\n");
    printf("\n");
    printf(I18N_QUERY_USAGE ":\n");
    printf("  %s -d <dir> [options]\n", prog_name);
    printf("\n");
    printf(I18N_QUERY_REQUIRED ":\n");
    printf("  -d <dir>        " I18N_SERVER_STORAGE "\n");
    printf("\n");
    printf(I18N_QUERY_OPTIONS ":\n");
    printf("  -k <keyword>    " I18N_QUERY_KEYWORD "\n");
    printf("  -n <node_id>    " I18N_QUERY_NODE "\n");
    printf("  -l <level>      " I18N_QUERY_LEVEL " (DEBUG/INFO/WARN/ERROR/FATAL)\n");
    printf("  -s <timestamp>  Start time (Unix ms)\n");
    printf("  -e <timestamp>  End time (Unix ms)\n");
    printf("  -c <count>      Max results (default: 100)\n");
    printf("  --sort <mode>   " I18N_STORE_CAUSAL_SORT "/" I18N_STORE_TIME_SORT "/" I18N_STORE_NODE_SORT "\n");
    printf("  --stats         " I18N_QUERY_STATISTICS "\n");
    printf("  -h              " I18N_QUERY_USAGE "\n");
    printf("\n");
    printf(I18N_QUERY_EXAMPLES ":\n");
    printf("  %s -d ./logs\n", prog_name);
    printf("  %s -d ./logs -k \"error\"\n", prog_name);
    printf("  %s -d ./logs -n node-01 -l ERROR\n", prog_name);
    printf("  %s -d ./logs --sort time -c 50\n", prog_name);
    printf("  %s -d ./logs --stats\n", prog_name);
    printf("\n");
    printf("  --or            使用关键词 [OR] 组合查询 (默认是 AND 组合)\n");
    printf("\n");
    printf("  --dot <file>    将日志因果依赖链导出为 Graphviz DOT 拓扑图\n");
    printf("\n");
}

/* 打印统计信息 */
static void print_stats(const log_store_t *store)
{
    int total = 0;
    int node_counts[32] = {0};
    int node_count = 0;

    store_get_stats(store, &total, node_counts, &node_count);

    printf("\n========== " I18N_QUERY_STATISTICS " ==========\n");
    printf("  " I18N_STORE_TOTAL ": %d\n", total);
    printf("\n  " I18N_QUERY_LOGS_BY_NODE ":\n");

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
    bool is_and_mode = true; /* 默认为 AND 组合 */
    char *dot_output_path = NULL;

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -d " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            strncpy(storage_dir, argv[++i], sizeof(storage_dir) - 1);
        } else if (strcmp(argv[i], "-k") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -k " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            query.keyword = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -n " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            query.node_id = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -l " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            query.level = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -s " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            query.time_start = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -e " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            query.time_end = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -c " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            max_results = atoi(argv[++i]);
            if (max_results <= 0) max_results = 100;
        } else if (strcmp(argv[i], "--sort") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": --sort " I18N_ERR_MISSING_PARAM "\n");
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
                fprintf(stderr, I18N_ERROR ": " I18N_ERR_INVALID_PARAM " '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--stats") == 0) {
            stats_only = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--or") == 0) { 
            is_and_mode = false;
        }else if (strcmp(argv[i], "--dot") == 0) {
            if (i + 1 < argc) dot_output_path = argv[++i];
        }else {
            fprintf(stderr, I18N_ERROR ": " I18N_ERR_INVALID_PARAM " '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* 检查必需参数 */
    if (storage_dir[0] == '\0') {
        fprintf(stderr, I18N_ERROR ": " I18N_SERVER_STORAGE " (-d) " I18N_ERR_MISSING_PARAM "\n");
        print_usage(argv[0]);
        return 1;
    }

    /* 初始化存储 */
    log_store_t store;
    if (store_init(&store) != 0) {
        return 1;
    }

    /* 加载日志 */
    printf(I18N_STORE_LOADING " %s...\n", storage_dir);
    int loaded = store_load_dir(&store, storage_dir);
    if (loaded < 0) {
        return 1;
    }
    printf(I18N_STORE_LOADED " %d " I18N_SERVER_LOGS "\n", loaded);

    // if (store_load_dir(&store, storage_dir) < 0) {
    //     return 1;
    // }

    if (loaded == 0) {
        printf(I18N_STORE_NO_LOGS ".\n");
        store_cleanup(&store);
        return 0;
    }

    store_build_index(&store);/* 为所有加载进内存的 message 字段建立倒排索引词典 */

    /* 排序 */
    printf(I18N_STORE_SORTING " (%s)...\n",
           sort_mode == SORT_CAUSAL ? I18N_STORE_CAUSAL_SORT :
           sort_mode == SORT_TIME ? I18N_STORE_TIME_SORT : I18N_STORE_NODE_SORT);

    switch (sort_mode) {
        case SORT_CAUSAL:
            store_sort_causal(&store);
            break;
        case SORT_TIME:
            store_sort_by_time(&store);
            break;
        case SORT_NODE:
            store_sort_by_node(&store);
            break;
    }

    /* 统计模式 */
    if (stats_only) {
        print_stats(&store);
        store_cleanup(&store);
        return 0;
    }

    /* 查询 */
    log_entry_t **results = malloc(store.count * sizeof(log_entry_t *));
    if (!results) {
        fprintf(stderr, I18N_ERROR ": " I18N_ERR_ALLOC_MEMORY "\n");
        store_index_cleanup(&store);
        store_cleanup(&store);
        return 1;
    }

    //int result_count = store_query(&store, &query, results, max_results);
    int result_count = 0;

    /* 判断是否走倒排索引组合查询 */
    if (query.keyword && strlen(query.keyword) > 0) {
        printf("[Test] Starting inverted index keyword query: '%s'\n", query.keyword);

        // 1. 初始化高精度计时器
        LARGE_INTEGER frequency;
        LARGE_INTEGER start_time, end_time;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start_time); // 记录开始时间
        // 使用倒排索引流进行 And/Or 高效检索
        result_count = store_query_by_index(&store, query.keyword, is_and_mode, results, store.count);
        QueryPerformanceCounter(&end_time); // 💾 记录结束时间

        // 3. 计算耗时（单位：毫秒 ms）
        double elapsed_ms = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 / frequency.QuadPart;

        printf("\n========================================\n");
        printf(" Performance benchmark test results \n");
        printf("========================================\n");
        printf("  Test dataset size          : %d \n", store.count);
        printf("  Number of hits in search   : %d \n", result_count);
        printf("  Core query latency         : %.3f ms\n", elapsed_ms);
    
        // 4. 判定是否达标
        if (elapsed_ms < 100.0) {
            printf("  PASSED \n");
        } else {
            printf("   FAILED \n");
        }
        printf("========================================\n");
    } else {
        // 原有的普通无索引条件查询
        result_count = store_query(&store, &query, results, store.count);
    }

    /* 在最终输出和打印前，强行做数量截断 (Truncation) */
    int display_count = result_count;
    if (display_count > max_results) {
        display_count = max_results; // 👈 在这里将结果数截断到你传入的 -c 20 条
    }

    /* 打印结果 */
    if (result_count > 0) {
        printf("\n" I18N_QUERY_RESULTS ":\n");
        printf("  " I18N_QUERY_KEYWORD ": %s\n", query.keyword ? query.keyword : I18N_QUERY_ANY);
        printf("  " I18N_QUERY_NODE ":    %s\n", query.node_id ? query.node_id : I18N_QUERY_ANY);
        printf("  " I18N_QUERY_LEVEL ":   %s\n", query.level ? query.level : I18N_QUERY_ANY);
        printf("  " I18N_STORE_TOTAL ":   %d\n\n", result_count);

        printf("%-5s | %-23s | %-5s | %-8s | %-15s | %s\n",
               "Index", "Timestamp", "Level", "Node", "Vector Clock", "Message");
        printf("------+-------------------------+-------+----------+-----------------+--------\n");

        for (int i = 0; i < display_count; i++) {
            store_print_entry(results[i], i);
        }
    } else {
        printf("\n" I18N_QUERY_NO_RESULTS ".\n");
    }

    if (dot_output_path) {
        if (store_export_dot(&store, dot_output_path) == 0) {
            printf("\n[Success] The causal dependency network has been successfully exported to: %s\n", dot_output_path);
            printf("Hint: You can use the command 'dot -Tpng %s -o causal.png' to generate an image.\n", dot_output_path);
        }
    }

    /* 清理 */
    free(results);
    store_index_cleanup(&store);
    store_cleanup(&store);

    return 0;
}