// 向量时钟 API 定义
#ifndef VECTOR_CLOCK_H
#define VECTOR_CLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 向量时钟最大支持节点数 */
#define VC_MAX_NODES 32

/* 向量时钟比较结果 */
typedef enum {
    VC_EQUAL,            /* 完全相同 */
    VC_HAPPENS_BEFORE,   /* a happens-before b (a < b) */
    VC_HAPPENS_AFTER,    /* a happens-after b (a > b) */
    VC_CONCURRENT        /* a 和 b 并发 (无法比较) */
} vc_relation_t;

/* 向量时钟结构 */
typedef struct {
    int node_count;                  /* 节点总数 */
    uint64_t clocks[VC_MAX_NODES];  /* 每个节点的逻辑时钟 */
} vector_clock_t;

/*
 * 初始化向量时钟
 * @param vc         向量时钟指针
 * @param node_count 节点总数 (最大 VC_MAX_NODES)
 */
void vc_init(vector_clock_t *vc, int node_count);

/*
 * 增加指定节点的时钟值
 * @param vc      向量时钟指针
 * @param node_id 节点 ID (0 到 node_count-1)
 */
void vc_increment(vector_clock_t *vc, int node_id);

/*
 * 合并两个向量时钟 (取每个维度的最大值)
 * @param dst 目标向量时钟
 * @param src 源向量时钟
 */
void vc_merge(vector_clock_t *dst, const vector_clock_t *src);

/*
 * 比较两个向量时钟的因果关系
 * @param a 向量时钟 a
 * @param b 向量时钟 b
 * @return vc_relation_t 关系类型
 */
vc_relation_t vc_compare(const vector_clock_t *a, const vector_clock_t *b);

/*
 * 复制向量时钟
 * @param dst 目标
 * @param src 源
 */
void vc_copy(vector_clock_t *dst, const vector_clock_t *src);

/*
 * 将向量时钟序列化为 JSON 字符串
 * 格式: {"node-0":1,"node-1":3,"node-2":2}
 * @param vc     向量时钟指针
 * @param node_names 节点名称数组 (可选，为 NULL 时使用 "node-i" 格式)
 * @param buf    输出缓冲区
 * @param buflen 缓冲区大小
 * @return 写入的字符数，失败返回 -1
 */
int vc_to_json(const vector_clock_t *vc, const char **node_names,
               char *buf, size_t buflen);

/*
 * 从 JSON 字符串解析向量时钟
 * @param vc       输出向量时钟
 * @param json     JSON 字符串
 * @param node_names 节点名称数组 (可选)
 * @return 0 成功，-1 失败
 */
int vc_from_json(vector_clock_t *vc, const char *json, const char **node_names);

/*
 * 获取向量时钟的字符串表示 (用于调试)
 * @param vc     向量时钟指针
 * @param buf    输出缓冲区
 * @param buflen 缓冲区大小
 * @return 写入的字符数
 */
int vc_to_string(const vector_clock_t *vc, char *buf, size_t buflen);

/*
 * 检查向量时钟是否有效
 * @param vc 向量时钟指针
 * @return true 有效，false 无效
 */
bool vc_is_valid(const vector_clock_t *vc);

/*
 * 获取两个向量时钟关系的描述字符串
 * @param rel 关系类型
 * @return 描述字符串
 */
const char *vc_relation_str(vc_relation_t rel);

#endif /* VECTOR_CLOCK_H */