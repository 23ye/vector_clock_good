#include "store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/* 初始化日志存储 */
int store_init(log_store_t *store)
{
    if (!store) return -1;

    store->entries = malloc(STORE_INIT_CAPACITY * sizeof(log_entry_t));
    if (!store->entries) {
        fprintf(stderr, "Error: Failed to allocate log store\n");
        return -1;
    }

    store->count = 0;
    store->capacity = STORE_INIT_CAPACITY;
    store->sorted = false;

    return 0;
}

/* 清理日志存储 */
void store_cleanup(log_store_t *store)
{
    if (!store) return;

    if (store->entries) {
        free(store->entries);
        store->entries = NULL;
    }

    store->count = 0;
    store->capacity = 0;
}

/* 扩容 */
static int store_expand(log_store_t *store)
{
    int new_capacity = store->capacity * 2;
    log_entry_t *new_entries = realloc(store->entries,
                                       new_capacity * sizeof(log_entry_t));
    if (!new_entries) {
        fprintf(stderr, "Error: Failed to expand log store\n");
        return -1;
    }

    store->entries = new_entries;
    store->capacity = new_capacity;
    return 0;
}

/* 添加日志条目 */
int store_append(log_store_t *store, const log_entry_t *entry)
{
    if (!store || !entry) return -1;

    /* 检查是否需要扩容 */
    if (store->count >= store->capacity) {
        if (store_expand(store) != 0) {
            return -1;
        }
    }

    /* 复制日志条目 */
    memcpy(&store->entries[store->count], entry, sizeof(log_entry_t));
    store->count++;
    store->sorted = false;

    return 0;
}

/* 从 JSONL 文件加载日志 */
int store_load_file(log_store_t *store, const char *filepath)
{
    if (!store || !filepath) return -1;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        /* 文件不存在不是错误 */
        return 0;
    }

    char line[LOGAGG_MAX_ENTRY_SIZE];
    int loaded = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* 跳过空行 */
        if (line[0] == '\0' || line[0] == '\n') continue;

        /* 去掉末尾换行符 */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* 解析 JSON */
        log_entry_t entry;
        if (log_entry_from_json(&entry, line) == 0) {
            if (store_append(store, &entry) == 0) {
                loaded++;
            }
        }
    }

    fclose(fp);
    return loaded;
}

/* 从目录加载所有 JSONL 文件 */
int store_load_dir(log_store_t *store, const char *dir)
{
    if (!store || !dir) return -1;

    int total_loaded = 0;

#ifdef _WIN32
    /* Windows 目录遍历 */
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\*.jsonl", dir);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(pattern, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        /* 目录不存在或为空 */
        return 0;
    }

    do {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s\\%s", dir, find_data.cFileName);

        int loaded = store_load_file(store, filepath);
        if (loaded > 0) {
            printf("Loaded %d logs from %s\n", loaded, find_data.cFileName);
            total_loaded += loaded;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
#else
    /* Linux/Mac 目录遍历 */
    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        /* 只处理 .jsonl 文件 */
        const char *name = entry->d_name;
        size_t name_len = strlen(name);
        if (name_len < 6 || strcmp(name + name_len - 6, ".jsonl") != 0) {
            continue;
        }

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir, name);

        int loaded = store_load_file(store, filepath);
        if (loaded > 0) {
            printf("Loaded %d logs from %s\n", loaded, name);
            total_loaded += loaded;
        }
    }

    closedir(d);
#endif

    return total_loaded;
}

/* 比较函数：因果排序 */
static int compare_causal(const void *a, const void *b)
{
    return log_entry_compare(a, b);
}

/* 比较函数：时间排序 */
static int compare_time(const void *a, const void *b)
{
    return log_entry_compare_by_time(a, b);
}

/* 比较函数：节点排序 */
static int compare_node(const void *a, const void *b)
{
    const log_entry_t *ea = (const log_entry_t *)a;
    const log_entry_t *eb = (const log_entry_t *)b;

    int cmp = strcmp(ea->node_id, eb->node_id);
    if (cmp != 0) return cmp;

    /* 同节点按时间排序 */
    if (ea->timestamp != eb->timestamp) {
        return (ea->timestamp < eb->timestamp) ? -1 : 1;
    }

    return 0;
}

/* 因果排序 */
void store_sort_causal(log_store_t *store)
{
    if (!store || store->count <= 1) return;

    qsort(store->entries, store->count, sizeof(log_entry_t), compare_causal);
    store->sorted = true;
}

/* 按时间排序 */
void store_sort_by_time(log_store_t *store)
{
    if (!store || store->count <= 1) return;

    qsort(store->entries, store->count, sizeof(log_entry_t), compare_time);
    store->sorted = true;
}

/* 按节点排序 */
void store_sort_by_node(log_store_t *store)
{
    if (!store || store->count <= 1) return;

    qsort(store->entries, store->count, sizeof(log_entry_t), compare_node);
    store->sorted = true;
}

/* 字符串包含检查 (不区分大小写) */
static bool str_contains(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;

    size_t needle_len = strlen(needle);
    if (needle_len == 0) return true;

    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, needle_len) == 0) {
            return true;
        }
    }

    return false;
}

/* 检查日志是否匹配查询条件 */
bool store_match_query(const log_entry_t *entry, const query_t *query)
{
    if (!entry) return false;
    if (!query) return true;  /* 无查询条件，全部匹配 */

    /* 关键词过滤 */
    if (query->keyword && query->keyword[0] != '\0') {
        if (!str_contains(entry->message, query->keyword)) {
            return false;
        }
    }

    /* 节点 ID 过滤 */
    if (query->node_id && query->node_id[0] != '\0') {
        if (strcmp(entry->node_id, query->node_id) != 0) {
            return false;
        }
    }

    /* 日志级别过滤 */
    if (query->level && query->level[0] != '\0') {
        if (strcasecmp(LOG_LEVEL_STR(entry->level), query->level) != 0) {
            return false;
        }
    }

    /* 时间范围过滤 */
    if (query->time_start > 0 && entry->timestamp < query->time_start) {
        return false;
    }
    if (query->time_end > 0 && entry->timestamp > query->time_end) {
        return false;
    }

    return true;
}

/* 查询日志 */
int store_query(const log_store_t *store, const query_t *query,
                log_entry_t **results, int max_count)
{
    if (!store || !results || max_count <= 0) return 0;

    int count = 0;

    for (int i = 0; i < store->count && count < max_count; i++) {
        if (store_match_query(&store->entries[i], query)) {
            results[count++] = &store->entries[i];
        }
    }

    return count;
}

/* 打印日志条目 */
void store_print_entry(const log_entry_t *entry, int index)
{
    if (!entry) return;

    char time_str[32];
    timestamp_to_string(entry->timestamp, time_str, sizeof(time_str));

    char vc_str[128];
    vc_to_string(&entry->vc, vc_str, sizeof(vc_str));

    printf("[%d] %s | %-5s | %-8s | VC=%s | %s\n",
           index,
           time_str,
           LOG_LEVEL_STR(entry->level),
           entry->node_id,
           vc_str,
           entry->message);
}

/* 打印日志列表 */
void store_print_range(const log_store_t *store, int start, int count)
{
    if (!store) return;

    if (start < 0) start = 0;
    if (start + count > store->count) {
        count = store->count - start;
    }

    printf("\n%-5s | %-23s | %-5s | %-8s | %-15s | %s\n",
           "Index", "Timestamp", "Level", "Node", "Vector Clock", "Message");
    printf("------+-------------------------+-------+----------+-----------------+--------\n");

    for (int i = 0; i < count; i++) {
        store_print_entry(&store->entries[start + i], start + i);
    }

    printf("\nTotal: %d logs\n", store->count);
}

/* 获取存储统计信息 */
void store_get_stats(const log_store_t *store, int *total,
                     int *node_counts, int *node_count)
{
    if (!store) return;

    if (total) *total = store->count;

    /* 统计各节点日志数 */
    int counts[32] = {0};
    int max_node = 0;

    for (int i = 0; i < store->count; i++) {
        /* 简单的节点 ID 到索引映射 */
        const char *id = store->entries[i].node_id;
        int idx = 0;

        /* 尝试从 "node-X" 提取索引 */
        if (strncmp(id, "node-", 5) == 0) {
            idx = atoi(id + 5);
        } else {
            /* 使用哈希 */
            for (const char *p = id; *p; p++) {
                idx = (idx * 31 + *p) % 32;
            }
        }

        if (idx >= 0 && idx < 32) {
            counts[idx]++;
            if (idx > max_node) max_node = idx;
        }
    }

    if (node_counts) {
        memcpy(node_counts, counts, sizeof(counts));
    }
    if (node_count) {
        *node_count = max_node + 1;
    }
}