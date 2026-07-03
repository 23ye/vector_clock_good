#include "store.h"
#include "lz4.h"
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

    /* 基于节点ID和向量时钟(Vector Clock)做全局日志去重检查 */
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->entries[i].node_id, entry->node_id) == 0) {
            if (vc_compare(&store->entries[i].vc, &entry->vc) == VC_EQUAL) {
                return 0; 
            }
        }
    }

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

    size_t raw_size = 0;
    int loaded = 0;

    // 尝试进行透明解压读取
    char *decompressed_data = store_load_compressed(filepath, &raw_size);

    if (decompressed_data) {
        char *line_ptr = decompressed_data;
        char *next_line = NULL;

        // 模拟 fgets 在内存缓冲区中安全切割每一行文本
        while (line_ptr && *line_ptr != '\0') {
            next_line = strchr(line_ptr, '\n');
            if (next_line) {
                *next_line = '\0'; // 临时截断换行符，变成标准 C 字符串
            }

            // 去掉 Windows 特有的 \r 换行符
            size_t len = strlen(line_ptr);
            if (len > 0 && line_ptr[len - 1] == '\r') {
                line_ptr[len - 1] = '\0';
            }

            // 跳过空行
            if (line_ptr[0] != '\0') {
                log_entry_t entry;
                // 调用你原本的解析函数
                if (log_entry_from_json(&entry, line_ptr) == 0) {
                    if (store_append(store, &entry) == 0) { // 内部带去重
                        loaded++;
                    }
                }
            }

            if (next_line) {
                line_ptr = next_line + 1; // 滚到下一行起始位置
            } else {
                break;
            }
        }

        free(decompressed_data); // 记得释放 store_load_compressed 内部 malloc 的解压池
        return loaded;
    }

    // 如果不是 LZ4 压缩文件，下降降级到原有的普通纯文本 JSONL 流
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        /* 文件不存在不是错误 */
        return 0;
    }

    char line[LOGAGG_MAX_ENTRY_SIZE];


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

/* 因果依赖关系的最小生成边判断：
 * 为了防止图里的线密密麻麻（因为 A->B, B->C 会导致 A->C 产生冗余边），
 * 我们可以只连接“直接”偏序（即中间没有其他事件隔离）的两个日志。
 */
int store_export_dot(const log_store_t *store, const char *filepath) {
    if (!store || !filepath || store->count == 0) return -1;

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s for writing\n", filepath);
        return -1;
    }

    // 1. 写入 DOT 图头部配置
    fprintf(fp, "digraph G {\n");
    fprintf(fp, "    rankdir=LR;\n"); // 从左到右布局
    fprintf(fp, "    node [shape=box, fontname=\"Courier\", fontsize=10];\n");
    fprintf(fp, "    edge [color=\"#2b579a\", arrowhead=normal, arrowsize=0.8];\n\n");

    // 2. 按节点分组绘制节点（使用子图 cluster 区分机器环境）
    // 为了防止重名，我们将日志索引作为图节点的唯一 ID：event_0, event_1...
    // int total_nodes = 0;
    // int node_processed[32] = {0}; // 假设最大32个节点

    for (int i = 0; i < store->count; i++) {
        const char *curr_node = store->entries[i].node_id;
        
        // 简单提取节点名字，避免重复为同一个 cluster 重写头部
        // 查找是否已经为此节点开辟过 subgraph
        bool first_seen = true;
        for(int k=0; k<i; k++) {
            if(strcmp(store->entries[k].node_id, curr_node) == 0) {
                first_seen = false;
                break;
            }
        }

        if (first_seen) {
            if (i > 0) fprintf(fp, "    }\n"); // 关闭上一个 cluster
            fprintf(fp, "    subgraph \"cluster_%s\" {\n", curr_node);
            fprintf(fp, "        label=\"Node: %s\";\n", curr_node);
            fprintf(fp, "        color=gray; style=dashed;\n");
        }

        // 把日志的关键 Message 缩短（防止图块过大）
        char msg_summary[32];
        strncpy(msg_summary, store->entries[i].message, 28);
        msg_summary[28] = '\0';
        if (strlen(store->entries[i].message) > 28) strcat(msg_summary, "...");

        // 打印每个日志点节点
        fprintf(fp, "        event_%d [label=\"[%s]\\n%s\"];\n", 
                i, LOG_LEVEL_STR(store->entries[i].level), msg_summary);
    }
    if (store->count > 0) fprintf(fp, "    }\n\n"); // 关闭最后一个 cluster

    // 3. 计算因果边 (两两进行向量时钟偏序比对)
    fprintf(fp, "    // Causal Dependency Edges\n");
    for (int i = 0; i < store->count; i++) {
        for (int j = 0; j < store->count; j++) {
            if (i == j) continue;
            
            // 运用之前实现的 vc_compare 算法
            // 如果日志 i Happens-Before 日志 j
            if (vc_compare(&store->entries[i].vc, &store->entries[j].vc) == VC_HAPPENS_BEFORE) {
                
                // 【可选优化：规避传递冗余边】
                // 检查是否存在一个中间媒介 k，使得 i -> k 且 k -> j。如果有，则 i -> j 是一条冗余长线，不打印。
                bool is_direct = true;
                for (int k = 0; k < store->count; k++) {
                    if (k == i || k == j) continue;
                    if (vc_compare(&store->entries[i].vc, &store->entries[k].vc) == VC_HAPPENS_BEFORE &&
                        vc_compare(&store->entries[k].vc, &store->entries[j].vc) == VC_HAPPENS_BEFORE) {
                        is_direct = false;
                        break;
                    }
                }

                if (is_direct) {
                    // 如果跨节点，可以用虚线或者其他颜色高亮显示
                    if (strcmp(store->entries[i].node_id, store->entries[j].node_id) != 0) {
                        fprintf(fp, "    event_%d -> event_%d [style=dashed, color=\"#e51400\", label=\"RPC\"];\n", i, j);
                    } else {
                        fprintf(fp, "    event_%d -> event_%d;\n", i, j);
                    }
                }
            }
        }
    }

    fprintf(fp, "}\n");
    fclose(fp);
    return 0;
}

/* * 压缩落盘：将内存中的大字符串（如拼接好的所有JSON日志）压缩写入文件
 */
int store_save_compressed(const char *filepath, const char *raw_data, size_t raw_size) {
    if (!filepath || !raw_data || raw_size == 0) return -1;

    // 1. 计算 LZ4 压缩所需的最大安全缓冲区大小
    int max_dst_size = LZ4_compressBound((int)raw_size);
    char *compressed_buf = malloc(max_dst_size);
    if (!compressed_buf) return -1;

    // 2. 执行压缩
    int compressed_size = LZ4_compress_default(raw_data, compressed_buf, (int)raw_size, max_dst_size);
    if (compressed_size <= 0) {
        free(compressed_buf);
        return -1;
    }

    // 3. 写入文件 (格式：[RawSize (4B)] + [CompSize (4B)] + [Data])
    FILE *fp = fopen(filepath, "wb"); // 必须是 wb 模式
    if (!fp) {
        free(compressed_buf);
        return -1;
    }

    uint32_t header_raw = (uint32_t)raw_size;
    uint32_t header_comp = (uint32_t)compressed_size;

    fwrite(&header_raw, sizeof(uint32_t), 1, fp);
    fwrite(&header_comp, sizeof(uint32_t), 1, fp);
    fwrite(compressed_buf, 1, compressed_size, fp);

    fclose(fp);
    free(compressed_buf);
    return 0;
}

/* * 透明解压：读取压缩文件，在内存中还原出完整的原始字符串
 * @return 还原后的字符串指针 (使用后需 free)，失败返回 NULL
 */
char* store_load_compressed(const char *filepath, size_t *out_raw_size) {
    if (!filepath || !out_raw_size) return NULL;

    FILE *fp = fopen(filepath, "rb"); // 必须是 rb 模式
    if (!fp) return NULL;

    // 1. 读取 8 字节的文件头
    uint32_t raw_size = 0;
    uint32_t compressed_size = 0;
    if (fread(&raw_size, sizeof(uint32_t), 1, fp) != 1 ||
        fread(&compressed_size, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    // 2. 申请对应的内存缓冲区
    char *compressed_buf = malloc(compressed_size);
    char *raw_buf = malloc(raw_size + 1); // +1 用于格式化为 C 字符串 \0
    if (!compressed_buf || !raw_buf) {
        fclose(fp);
        free(compressed_buf);
        free(raw_buf);
        return NULL;
    }

    // 3. 读取压缩的二进制流
    if (fread(compressed_buf, 1, compressed_size, fp) != compressed_size) {
        fclose(fp);
        free(compressed_buf);
        free(raw_buf);
        return NULL;
    }
    fclose(fp);

    // 4. 调用 LZ4 解压还原
    int decompress_result = LZ4_decompress_safe(compressed_buf, raw_buf, (int)compressed_size, (int)raw_size);
    free(compressed_buf);

    if (decompress_result < 0) {
        free(raw_buf);
        return NULL; // 解压失败
    }

    raw_buf[raw_size] = '\0'; // 闭合字符串
    *out_raw_size = raw_size;
    return raw_buf;
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