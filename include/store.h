#ifndef STORE_H
#define STORE_H

#include "logagg.h"
#include <stdint.h>
#include <stdbool.h>

/* 日志池初始容量 */
#define STORE_INIT_CAPACITY 1024

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

/* 日志存储 */
typedef struct {
    log_entry_t *entries;       /* 日志数组 */
    int count;                  /* 当前日志数 */
    int capacity;               /* 数组容量 */
    bool sorted;                /* 是否已排序 */
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

#endif /* STORE_H */