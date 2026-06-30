// 日志条目格式定义
#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include "vector_clock.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* 日志级别 */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} log_level_t;

/* 日志级别字符串 */
#define LOG_LEVEL_STR(level) \
    ((level) == LOG_DEBUG ? "DEBUG" : \
     (level) == LOG_INFO  ? "INFO"  : \
     (level) == LOG_WARN  ? "WARN"  : \
     (level) == LOG_ERROR ? "ERROR" : \
     (level) == LOG_FATAL ? "FATAL" : "UNKNOWN")

/* 解析日志级别字符串 */
log_level_t log_level_from_str(const char *str);

/* 节点 ID 最大长度 */
#define NODE_ID_MAX_LEN 31

/* 日志消息最大长度 */
#define LOG_MESSAGE_MAX_LEN 1023

/* 元数据键值对最大数量 */
#define METADATA_MAX_COUNT 8

/* 元数据键值对 */
typedef struct {
    char key[64];
    char value[256];
} metadata_pair_t;

/* 日志条目结构 */
typedef struct {
    /* 必需字段 */
    char node_id[NODE_ID_MAX_LEN + 1];  /* 节点标识符 */
    vector_clock_t vc;                   /* 向量时钟 */
    uint64_t timestamp;                  /* Unix 时间戳 (毫秒) */
    log_level_t level;                   /* 日志级别 */
    char message[LOG_MESSAGE_MAX_LEN + 1]; /* 日志消息 */

    /* 可选字段 */
    metadata_pair_t metadata[METADATA_MAX_COUNT];
    int metadata_count;

    /* 内部字段 (用于排序) */
    uint64_t seq;  /* 同节点内递增序号 */
} log_entry_t;

/*
 * 初始化日志条目
 * @param entry    日志条目指针
 * @param node_id  节点标识符
 * @param vc       向量时钟
 * @param level    日志级别
 * @param message  日志消息
 */
void log_entry_init(log_entry_t *entry, const char *node_id,
                    const vector_clock_t *vc, log_level_t level,
                    const char *message);

/*
 * 设置日志时间戳
 * @param entry     日志条目指针
 * @param timestamp 时间戳 (毫秒)
 */
void log_entry_set_timestamp(log_entry_t *entry, uint64_t timestamp);

/*
 * 添加元数据
 * @param entry 日志条目指针
 * @param key   键
 * @param value 值
 * @return 0 成功，-1 失败 (元数据已满)
 */
int log_entry_add_metadata(log_entry_t *entry, const char *key, const char *value);

/*
 * 获取元数据值
 * @param entry 日志条目指针
 * @param key   键
 * @return 值的指针，未找到返回 NULL
 */
const char *log_entry_get_metadata(const log_entry_t *entry, const char *key);

/*
 * 将日志条目序列化为 JSON 字符串
 * @param entry  日志条目指针
 * @param buf    输出缓冲区
 * @param buflen 缓冲区大小
 * @return 写入的字符数，失败返回 -1
 */
int log_entry_to_json(const log_entry_t *entry, char *buf, size_t buflen);

/*
 * 从 JSON 字符串解析日志条目
 * @param entry 输出日志条目
 * @param json  JSON 字符串
 * @return 0 成功，-1 失败
 */
int log_entry_from_json(log_entry_t *entry, const char *json);

/*
 * 比较两个日志条目 (用于因果排序)
 * @param a 日志条目 a
 * @param b 日志条目 b
 * @return <0 如果 a 应在 b 前，>0 如果 a 应在 b 后，0 如果相等
 */
int log_entry_compare(const void *a, const void *b);

/*
 * 比较两个日志条目 (仅按时间戳)
 * @param a 日志条目 a
 * @param b 日志条目 b
 * @return <0 如果 a 的时间戳更早，>0 如果更晚，0 如果相等
 */
int log_entry_compare_by_time(const void *a, const void *b);

/*
 * 获取当前时间戳 (毫秒)
 * @return 毫秒级 Unix 时间戳
 */
uint64_t get_current_timestamp_ms(void);

/*
 * 时间戳转换为可读字符串
 * @param timestamp 毫秒时间戳
 * @param buf       输出缓冲区
 * @param buflen    缓冲区大小
 * @return 写入的字符数
 */
int timestamp_to_string(uint64_t timestamp, char *buf, size_t buflen);

#endif /* LOG_ENTRY_H */