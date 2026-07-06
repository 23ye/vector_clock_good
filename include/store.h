#ifndef STORE_H
#define STORE_H

#include "logagg.h"
#include <stdint.h>
#include <stdbool.h>

/* 日志池初始容量 */
#define STORE_INIT_CAPACITY 1024
#define MAX_TERM_LEN 64
#define INDEX_INIT_CAPACITY 128
#define DEDUP_HASH_SIZE 65536

/* 查询条件 */
typedef struct {
    char *keyword;              /* 关键词过滤 */
    char *node_id;              /* 节点 ID 过滤 */
    char *level;                /* 日志级别过滤 */
    uint64_t time_start;        /* 时间范围起始 */
    uint64_t time_end;          /* 时间范围结束 */
} query_t;

/* 排序方式 */
typedef enum {
    SORT_CAUSAL,    /* 因果排序 (向量时钟) */
    SORT_TIME,      /* 时间排序 */
    SORT_NODE       /* 按节点排序 */
} sort_mode_t;

/* 倒排表：存储包含某个单词的所有日志项的内部索引 (数组下标) */
typedef struct {
    int *entry_indices;  /* 动态数组，存储 store->entries 的下标 */
    int count;           /* 当前记录的日志数量 */
    int capacity;        /* 动态数组容量 */
} posting_list_t;

/* 词典节点：关联单词与其倒排表 */
typedef struct {
    char term[MAX_TERM_LEN];
    posting_list_t posting;
} index_entry_t;

/* 倒排索引主结构 */
typedef struct {
    index_entry_t *dict; /* 词典数组 */
    int count;           /* 词典中唯一单词的数量 */
    int capacity;        /* 词典容量 */
} inverted_index_t;

// 哈希表节点结构
typedef struct hash_node {
    char node_id[64];           // 对应 log_entry_t 中的 node_id
    vector_clock_t vc;          // 对应 log_entry_t 中的 vc (假设你的类型名叫这个)
    struct hash_node *next;     // 冲突链表指针
} hash_node_t;

// 哈希表管理器
typedef struct {
    hash_node_t **buckets;
} dedup_hash_t;

/* 日志存储 */
typedef struct {
    log_entry_t *entries;       /* 日志数组 */
    int count;                  /* 当前日志数 */
    int capacity;               /* 数组容量 */
    bool sorted;                /* 是否已排序 */
    inverted_index_t index;     /* 倒排索引 */
    dedup_hash_t *dedup_hash;   /* 哈希表 */
} log_store_t;

/*
 * 初始化日志存储
 * @param store 存储指针
 * @return 0 成功，-1 失败
 */
int store_init(log_store_t *store);

/*
 * 清理日志存储
 * @param store 存储指针
 */
void store_cleanup(log_store_t *store);

/*
 * 添加日志条目
 * @param store 存储指针
 * @param entry 日志条目
 * @return 0 成功，-1 失败
 */
int store_append(log_store_t *store, const log_entry_t *entry);

/*
 * 从 JSONL 文件加载日志
 * @param store    存储指针
 * @param filepath 文件路径
 * @return 成功加载的日志数，-1 失败
 */
int store_load_file(log_store_t *store, const char *filepath);

/*
 * 从目录加载所有 JSONL 文件
 * @param store 存储指针
 * @param dir   目录路径
 * @return 成功加载的日志数，-1 失败
 */
int store_load_dir(log_store_t *store, const char *dir);

/*
 * 因果排序
 * @param store 存储指针
 */
void store_sort_causal(log_store_t *store);

/*
 * 按时间排序
 * @param store 存储指针
 */
void store_sort_by_time(log_store_t *store);

/*
 * 按节点排序
 * @param store 存储指针
 */
void store_sort_by_node(log_store_t *store);

/*
 * 查询日志
 * @param store     存储指针
 * @param query     查询条件
 * @param results   结果数组 (输出)
 * @param max_count 最大结果数
 * @return 匹配的日志数
 */
int store_query(const log_store_t *store, const query_t *query,
                log_entry_t **results, int max_count);

/*
 * 打印日志条目
 * @param entry 日志条目
 * @param index 索引 (用于显示)
 */
void store_print_entry(const log_entry_t *entry, int index);

/*
 * 打印日志列表
 * @param store  存储指针
 * @param start  起始索引
 * @param count  打印数量
 */
void store_print_range(const log_store_t *store, int start, int count);

/*
 * 获取存储统计信息
 * @param store       存储指针
 * @param total       总日志数 (输出)
 * @param node_counts 各节点日志数 (输出)
 * @param node_count  节点数 (输出)
 */
void store_get_stats(const log_store_t *store, int *total,
                     int *node_counts, int *node_count);

/*
 * 检查日志是否匹配查询条件
 * @param entry 日志条目
 * @param query 查询条件
 * @return true 匹配，false 不匹配
 */
bool store_match_query(const log_entry_t *entry, const query_t *query);

/* * 为所有加载进内存的 message 字段建立倒排索引词典 
 * @param store 存储指针
 */
void store_build_index(log_store_t *store);

/* * 倒排索引高效 And/Or 组合查询核心接口
 * @param store        存储指针
 * @param query_str    组合关键词字符串 (如 "connect timeout")
 * @param is_and_mode  true 为 AND 模式，false 为 OR 模式
 * @param results      输出的结果数组
 * @param max_count    最大结果数限制
 * @return 匹配命中的日志条数
 */
int store_query_by_index(const log_store_t *store, const char *query_str, bool is_and_mode, 
                         log_entry_t **results, int max_count);

/* * 将当前日志库中的因果依赖链导出为 Graphviz DOT 文件
 * @param store     存储指针
 * @param filepath  导出的 .dot 文件路径（例如 "causal_graph.dot"）
 * @return 0 成功，-1 失败
 */
int store_export_dot(const log_store_t *store, const char *filepath);

/* * 退出前释放倒排索引词典及 Posting List 占用的动态内存，防止内存泄漏
 * @param store 存储指针
 */
void store_index_cleanup(log_store_t *store);

/* LZ4 透明压缩与解压模块声明                                   */
int store_save_compressed(const char *filepath, const char *raw_data, size_t raw_size);
char* store_load_compressed(const char *filepath, size_t *out_raw_size);

/* 声明哈希表的管理函数 */
dedup_hash_t* dedup_hash_create(void);
void dedup_hash_destroy(dedup_hash_t *h);
bool dedup_hash_check_and_insert(dedup_hash_t *h, const char *node_id, const vector_clock_t *vc);

#endif /* STORE_H */