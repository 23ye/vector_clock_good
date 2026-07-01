//向量时钟实现
#include "vector_clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

/* 初始化向量时钟 */
void vc_init(vector_clock_t *vc, int node_count)
{
    if (!vc) {
        return;
    }

    /* 无效参数时设置为 0 */
    if (node_count <= 0 || node_count > VC_MAX_NODES) {
        vc->node_count = 0;
        memset(vc->clocks, 0, sizeof(vc->clocks));
        return;
    }

    vc->node_count = node_count;
    memset(vc->clocks, 0, sizeof(uint64_t) * node_count);
}

/* 增加指定节点的时钟值 */
void vc_increment(vector_clock_t *vc, int node_id)
{
    if (!vc || node_id < 0 || node_id >= vc->node_count) {
        return;
    }

    vc->clocks[node_id]++;
}

/* 合并两个向量时钟 (取每个维度的最大值) */
void vc_merge(vector_clock_t *dst, const vector_clock_t *src)
{
    if (!dst || !src) {
        return;
    }

    /* 使用较小的节点数 */
    int count = (dst->node_count < src->node_count) ?
                dst->node_count : src->node_count;

    for (int i = 0; i < count; i++) {
        if (src->clocks[i] > dst->clocks[i]) {
            dst->clocks[i] = src->clocks[i];
        }
    }
}

/* 比较两个向量时钟的因果关系 */
vc_relation_t vc_compare(const vector_clock_t *a, const vector_clock_t *b)
{
    if (!a || !b) {
        return VC_CONCURRENT;
    }

    /* 使用较小的节点数进行比较 */
    int count = (a->node_count < b->node_count) ?
                a->node_count : b->node_count;

    /*
     * 逻辑说明：
     * - a_le_b 表示 a[i] <= b[i] 对所有 i 成立
     * - b_le_a 表示 b[i] <= a[i] 对所有 i 成立
     *
     * 初始假设两者都成立，然后寻找反例：
     * - 如果 a[i] > b[i]，则 a <= b 不成立，a_le_b = false
     * - 如果 b[i] > a[i]，则 b <= a 不成立，b_le_a = false
     */
    bool a_le_b = true;  /* a[i] <= b[i] 对所有 i */
    bool b_le_a = true;  /* b[i] <= a[i] 对所有 i */

    for (int i = 0; i < count; i++) {
        if (a->clocks[i] > b->clocks[i]) {
            /* a[i] > b[i]，违反 a <= b */
            a_le_b = false;
        }
        if (b->clocks[i] > a->clocks[i]) {
            /* b[i] > a[i]，违反 b <= a */
            b_le_a = false;
        }
    }

    /* 处理节点数不同的情况 */
    if (a->node_count > count) {
        /* a 有额外的节点，检查这些节点是否都为 0 */
        for (int i = count; i < a->node_count; i++) {
            if (a->clocks[i] > 0) {
                a_le_b = false;
                break;
            }
        }
    }
    if (b->node_count > count) {
        /* b 有额外的节点，检查这些节点是否都为 0 */
        for (int i = count; i < b->node_count; i++) {
            if (b->clocks[i] > 0) {
                b_le_a = false;
                break;
            }
        }
    }

    /* 判断关系 */
    if (a_le_b && b_le_a) {
        return VC_EQUAL;
    } else if (b_le_a) {
        /* b <= a 且 a <= b 不成立，说明 a > b */
        return VC_HAPPENS_AFTER;
    } else if (a_le_b) {
        /* a <= b 且 b <= a 不成立，说明 a < b */
        return VC_HAPPENS_BEFORE;
    } else {
        return VC_CONCURRENT;
    }
}

/* 复制向量时钟 */
void vc_copy(vector_clock_t *dst, const vector_clock_t *src)
{
    if (!dst || !src) {
        return;
    }

    dst->node_count = src->node_count;
    memcpy(dst->clocks, src->clocks, sizeof(uint64_t) * src->node_count);
}

/* 将向量时钟序列化为 JSON 字符串 */
int vc_to_json(const vector_clock_t *vc, const char **node_names,
               char *buf, size_t buflen)
{
    if (!vc || !buf || buflen == 0) {
        return -1;
    }

    int offset = 0;
    int ret;

    ret = snprintf(buf + offset, buflen - offset, "{");
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    for (int i = 0; i < vc->node_count; i++) {
        if (i > 0) {
            ret = snprintf(buf + offset, buflen - offset, ",");
            if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
            offset += ret;
        }

        /* 节点名称 */
        if (node_names && node_names[i]) {
            ret = snprintf(buf + offset, buflen - offset,
                          "\"%s\":%" PRIu64, node_names[i],
                          vc->clocks[i]);
        } else {
            ret = snprintf(buf + offset, buflen - offset,
                          "\"node-%d\":%" PRIu64, i,
                          vc->clocks[i]);
        }

        if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
        offset += ret;
    }

    ret = snprintf(buf + offset, buflen - offset, "}");
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    return offset;
}

/* 辅助函数：跳过空白字符 */
static const char *skip_whitespace(const char *s)
{
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/* 辅助函数：解析 JSON 字符串 */
static const char *parse_json_string(const char *s, char *buf, size_t buflen)
{
    s = skip_whitespace(s);
    if (*s != '"') return NULL;
    s++;

    size_t i = 0;
    while (*s && *s != '"' && i < buflen - 1) {
        if (*s == '\\') {
            s++;
            if (*s == '"') buf[i++] = '"';
            else if (*s == '\\') buf[i++] = '\\';
            else if (*s == 'n') buf[i++] = '\n';
            else if (*s == 't') buf[i++] = '\t';
            else return NULL;
        } else {
            buf[i++] = *s;
        }
        s++;
    }

    if (*s != '"') return NULL;
    buf[i] = '\0';
    return s + 1;
}

/* 辅助函数：解析 JSON 数字 */
static const char *parse_json_number(const char *s, uint64_t *value)
{
    s = skip_whitespace(s);
    *value = 0;

    if (!isdigit((unsigned char)*s)) return NULL;

    while (isdigit((unsigned char)*s)) {
        *value = *value * 10 + (*s - '0');
        s++;
    }

    return s;
}

/* 从 JSON 字符串解析向量时钟 */
int vc_from_json(vector_clock_t *vc, const char *json, const char **node_names)
{
    if (!vc || !json) {
        return -1;
    }

    const char *s = skip_whitespace(json);

    /* 跳过 '{' */
    if (*s != '{') return -1;
    s = skip_whitespace(s + 1);

    /* 空对象 */
    if (*s == '}') {
        vc->node_count = 0;
        return 0;
    }

    /* 计算节点数并解析 */
    int count = 0;
    char keys[VC_MAX_NODES][64];
    uint64_t values[VC_MAX_NODES];

    while (*s && *s != '}') {
        if (count >= VC_MAX_NODES) return -1;

        /* 解析键 */
        s = parse_json_string(s, keys[count], sizeof(keys[count]));
        if (!s) return -1;

        /* 跳过 ':' */
        s = skip_whitespace(s);
        if (*s != ':') return -1;
        s = skip_whitespace(s + 1);

        /* 解析值 */
        s = parse_json_number(s, &values[count]);
        if (!s) return -1;

        count++;

        /* 跳过 ',' */
        s = skip_whitespace(s);
        if (*s == ',') {
            s = skip_whitespace(s + 1);
        }
    }

    if (*s != '}') return -1;

    /* 构建向量时钟 */
    vc->node_count = count;

    if (node_names) {
        /* 使用提供的节点名称映射 */
        for (int i = 0; i < count; i++) {
            vc->clocks[i] = 0;
            for (int j = 0; j < count; j++) {
                if (node_names[j] && strcmp(keys[i], node_names[j]) == 0) {
                    vc->clocks[j] = values[i];
                    break;
                }
            }
        }
    } else {
        /* 使用 "node-i" 格式解析 */
        for (int i = 0; i < count; i++) {
            char expected[32];
            snprintf(expected, sizeof(expected), "node-%d", i);
            if (strcmp(keys[i], expected) == 0) {
                vc->clocks[i] = values[i];
            } else {
                /* 尝试按数字索引 */
                if (strncmp(keys[i], "node-", 5) == 0) {
                    int idx = atoi(keys[i] + 5);
                    if (idx >= 0 && idx < count) {
                        vc->clocks[idx] = values[i];
                    }
                }
            }
        }
    }

    return 0;
}

/* 获取向量时钟的字符串表示 */
int vc_to_string(const vector_clock_t *vc, char *buf, size_t buflen)
{
    if (!vc || !buf || buflen == 0) {
        return -1;
    }

    int offset = 0;
    int ret;

    ret = snprintf(buf + offset, buflen - offset, "[");
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    for (int i = 0; i < vc->node_count; i++) {
        if (i > 0) {
            ret = snprintf(buf + offset, buflen - offset, ", ");
            if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
            offset += ret;
        }

        ret = snprintf(buf + offset, buflen - offset, "%" PRIu64,
                      vc->clocks[i]);
        if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
        offset += ret;
    }

    ret = snprintf(buf + offset, buflen - offset, "]");
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    return offset;
}

/* 检查向量时钟是否有效 */
bool vc_is_valid(const vector_clock_t *vc)
{
    if (!vc) return false;
    if (vc->node_count <= 0 || vc->node_count > VC_MAX_NODES) return false;
    return true;
}

/* 获取关系描述字符串 */
const char *vc_relation_str(vc_relation_t rel)
{
    switch (rel) {
        case VC_EQUAL:          return "EQUAL";
        case VC_HAPPENS_BEFORE: return "HAPPENS_BEFORE";
        case VC_HAPPENS_AFTER:  return "HAPPENS_AFTER";
        case VC_CONCURRENT:     return "CONCURRENT";
        default:                return "UNKNOWN";
    }
}