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

/* 清理倒排索引动态内存 */
void store_index_cleanup(log_store_t *store) {
    inverted_index_t *idx = &store->index;
    if (idx->dict) {
        for (int i = 0; i < idx->count; i++) {
            if (idx->dict[i].posting.entry_indices) {
                free(idx->dict[i].posting.entry_indices);
            }
        }
        free(idx->dict);
        idx->dict = NULL;
    }
    idx->count = 0;
    idx->capacity = 0;
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

/* 将字符串转换为纯小写，并去除末尾标点 */
static void normalize_term(char *dst, const char *src, size_t max_len) {
    size_t i = 0;
    while (*src && i < max_len - 1) {
        if (isalnum((unsigned char)*src)) { /* 只保留字母和数字 */
            dst[i++] = tolower((unsigned char)*src);
        }
        src++;
    }
    dst[i] = '\0';
}

/* 向某个倒排表中追加一条日志的索引 */
static void posting_list_add(posting_list_t *pl, int entry_idx) {
    if (pl->count >= pl->capacity) {
        pl->capacity = pl->capacity == 0 ? 8 : pl->capacity * 2;
        pl->entry_indices = realloc(pl->entry_indices, pl->capacity * sizeof(int));
    }
    // 保持 posting list 有序（由于日志是顺序处理的，天然递增）
    pl->entry_indices[pl->count++] = entry_idx;
}

/* 遍历日志并分词 */
void store_build_index(log_store_t *store) {
    inverted_index_t *idx = &store->index;
    
    // 初始化词典
    idx->count = 0;
    idx->capacity = INDEX_INIT_CAPACITY;
    idx->dict = malloc(idx->capacity * sizeof(index_entry_t));

    for (int i = 0; i < store->count; i++) {
        // 复制一份 message 用于 strtok 分词
        char msg_copy[LOG_MESSAGE_MAX_LEN + 1];
        strncpy(msg_copy, store->entries[i].message, LOG_MESSAGE_MAX_LEN);
        msg_copy[LOG_MESSAGE_MAX_LEN] = '\0';

        char *token = strtok(msg_copy, " \t\r\n,.!?:;()[]{}--_\"'");
        while (token != NULL) {
            char clean_term[MAX_TERM_LEN];
            normalize_term(clean_term, token, MAX_TERM_LEN);

            if (strlen(clean_term) > 0) {
                // 在词典中查找该单词是否已存在
                int found_pos = -1;
                for (int d = 0; d < idx->count; d++) {
                    if (strcmp(idx->dict[d].term, clean_term) == 0) {
                        found_pos = d;
                        break;
                    }
                }

                // 如果是新单词，加入词典
                if (found_pos == -1) {
                    if (idx->count >= idx->capacity) {
                        idx->capacity *= 2;
                        idx->dict = realloc(idx->dict, idx->capacity * sizeof(index_entry_t));
                    }
                    found_pos = idx->count;
                    strncpy(idx->dict[found_pos].term, clean_term, MAX_TERM_LEN);
                    idx->dict[found_pos].posting.entry_indices = NULL;
                    idx->dict[found_pos].posting.count = 0;
                    idx->dict[found_pos].posting.capacity = 0;
                    idx->count++;
                }

                // 避免同一条日志因同一个词被重复记录到同一个 Posting List 中
                posting_list_t *pl = &idx->dict[found_pos].posting;
                if (pl->count == 0 || pl->entry_indices[pl->count - 1] != i) {
                    posting_list_add(pl, i);
                }
            }
            token = strtok(NULL, " \t\r\n,.!?:;()[]{}--_\"'");
        }
    }
}

/* AND 求交集, 将两个倒排表求交集，结果存入 res */
static void intersect_posting_lists(const posting_list_t *a, const posting_list_t *b, posting_list_t *res) {
    res->count = 0;
    int i = 0, j = 0;
    while (i < a->count && j < b->count) {
        if (a->entry_indices[i] == b->entry_indices[j]) {
            posting_list_add(res, a->entry_indices[i]);
            i++; j++;
        } else if (a->entry_indices[i] < b->entry_indices[j]) {
            i++;
        } else {
            j++;
        }
    }
}

/* OR 求并集, 将两个倒排表求并集，结果存入 res */
static void union_posting_lists(const posting_list_t *a, const posting_list_t *b, posting_list_t *res) {
    res->count = 0;
    int i = 0, j = 0;
    while (i < a->count && j < b->count) {
        if (a->entry_indices[i] == b->entry_indices[j]) {
            posting_list_add(res, a->entry_indices[i]);
            i++; j++;
        } else if (a->entry_indices[i] < b->entry_indices[j]) {
            posting_list_add(res, a->entry_indices[i]);
            i++;
        } else {
            posting_list_add(res, b->entry_indices[j]);
            j++;
        }
    }
    // 处理残余元素
    while (i < a->count) posting_list_add(res, a->entry_indices[i++]);
    while (j < b->count) posting_list_add(res, b->entry_indices[j++]);
}

/* 根据单词获取词典中的倒排表指针 */
static const posting_list_t* get_posting_list(const log_store_t *store, const char *term) {
    char clean_term[MAX_TERM_LEN];
    normalize_term(clean_term, term, MAX_TERM_LEN);
    
    for (int i = 0; i < store->index.count; i++) {
        if (strcmp(store->index.dict[i].term, clean_term) == 0) {
            return &store->index.dict[i].posting;
        }
    }
    return NULL;
}

/* * 组合查询核心接口 
 * @param query_str: 输入的查询组合关键词，例如 "login failed"
 * @param is_and_mode: true 代表 AND 组合，false 代表 OR 组合
 */
int store_query_by_index(const log_store_t *store, const char *query_str, bool is_and_mode, 
                         log_entry_t **results, int max_count) {
    if (!query_str || strlen(query_str) == 0) return 0;

    // 分词解析查询条件中的各个 Term
    char q_copy[256];
    strncpy(q_copy, query_str, 255);
    q_copy[255] = '\0';

    posting_list_t final_pl = { NULL, 0, 0 };
    bool is_first_term = true;

    char *token = strtok(q_copy, " ");
    while (token != NULL) {
        const posting_list_t *current_term_pl = get_posting_list(store, token);
        
        // 如果当前单词在整个日志库里都没出现过
        posting_list_t empty_pl = { NULL, 0, 0 };
        if (!current_term_pl) {
            current_term_pl = &empty_pl;
        }

        if (is_first_term) {
            // 第一个词，直接复制其倒排表内容到临时结果中
            for (int k = 0; k < current_term_pl->count; k++) {
                posting_list_add(&final_pl, current_term_pl->entry_indices[k]);
            }
            is_first_term = false;
        } else {
            // 后续词，进行双指针高效交集或并集运算
            posting_list_t temp_res = { NULL, 0, 0 };
            if (is_and_mode) {
                intersect_posting_lists(&final_pl, current_term_pl, &temp_res);
            } else {
                union_posting_lists(&final_pl, current_term_pl, &temp_res);
            }
            // 释放上一次的临时内存，更新为最新的计算结果
            if (final_pl.entry_indices) free(final_pl.entry_indices);
            final_pl = temp_res;
        }
        token = strtok(NULL, " ");
    }

    // 将最终计算出的 Posting List 下标映射回真实的日志实体指针数组
    int match_count = 0;
    for (int m = 0; m < final_pl.count && match_count < max_count; m++) {
        int origin_idx = final_pl.entry_indices[m];
        results[match_count++] = &store->entries[origin_idx];
    }

    if (final_pl.entry_indices) free(final_pl.entry_indices);
    return match_count;
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