//日志条目实现
#include "log_entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* 解析日志级别字符串 */
log_level_t log_level_from_str(const char *str)
{
    if (!str) return LOG_INFO;

    if (strcasecmp(str, "DEBUG") == 0) return LOG_DEBUG;
    if (strcasecmp(str, "INFO") == 0)  return LOG_INFO;
    if (strcasecmp(str, "WARN") == 0)  return LOG_WARN;
    if (strcasecmp(str, "ERROR") == 0) return LOG_ERROR;
    if (strcasecmp(str, "FATAL") == 0) return LOG_FATAL;

    return LOG_INFO;
}

/* 初始化日志条目 */
void log_entry_init(log_entry_t *entry, const char *node_id,
                    const vector_clock_t *vc, log_level_t level,
                    const char *message)
{
    if (!entry) return;

    memset(entry, 0, sizeof(log_entry_t));

    /* 设置节点 ID */
    if (node_id) {
        strncpy(entry->node_id, node_id, NODE_ID_MAX_LEN);
        entry->node_id[NODE_ID_MAX_LEN] = '\0';
    }

    /* 设置向量时钟 */
    if (vc) {
        vc_copy(&entry->vc, vc);
    }

    /* 设置时间戳 */
    entry->timestamp = get_current_timestamp_ms();

    /* 设置级别 */
    entry->level = level;

    /* 设置消息 */
    if (message) {
        strncpy(entry->message, message, LOG_MESSAGE_MAX_LEN);
        entry->message[LOG_MESSAGE_MAX_LEN] = '\0';
    }

    /* 初始化元数据 */
    entry->metadata_count = 0;
    entry->seq = 0;
}

/* 设置日志时间戳 */
void log_entry_set_timestamp(log_entry_t *entry, uint64_t timestamp)
{
    if (entry) {
        entry->timestamp = timestamp;
    }
}

/* 添加元数据 */
int log_entry_add_metadata(log_entry_t *entry, const char *key, const char *value)
{
    if (!entry || !key || !value) return -1;
    if (entry->metadata_count >= METADATA_MAX_COUNT) return -1;

    metadata_pair_t *pair = &entry->metadata[entry->metadata_count];
    strncpy(pair->key, key, sizeof(pair->key) - 1);
    pair->key[sizeof(pair->key) - 1] = '\0';
    strncpy(pair->value, value, sizeof(pair->value) - 1);
    pair->value[sizeof(pair->value) - 1] = '\0';

    entry->metadata_count++;
    return 0;
}

/* 获取元数据值 */
const char *log_entry_get_metadata(const log_entry_t *entry, const char *key)
{
    if (!entry || !key) return NULL;

    for (int i = 0; i < entry->metadata_count; i++) {
        if (strcmp(entry->metadata[i].key, key) == 0) {
            return entry->metadata[i].value;
        }
    }

    return NULL;
}

/* 辅助函数：转义 JSON 字符串 */
static int json_escape(const char *src, char *dst, size_t dstlen)
{
    int offset = 0;

    for (const char *p = src; *p && offset < (int)dstlen - 2; p++) {
        switch (*p) {
            case '"':
                if (offset + 2 < (int)dstlen) {
                    dst[offset++] = '\\';
                    dst[offset++] = '"';
                }
                break;
            case '\\':
                if (offset + 2 < (int)dstlen) {
                    dst[offset++] = '\\';
                    dst[offset++] = '\\';
                }
                break;
            case '\n':
                if (offset + 2 < (int)dstlen) {
                    dst[offset++] = '\\';
                    dst[offset++] = 'n';
                }
                break;
            case '\t':
                if (offset + 2 < (int)dstlen) {
                    dst[offset++] = '\\';
                    dst[offset++] = 't';
                }
                break;
            default:
                if (isprint((unsigned char)*p)) {
                    dst[offset++] = *p;
                }
                break;
        }
    }

    dst[offset] = '\0';
    return offset;
}

/* 将日志条目序列化为 JSON 字符串 */
int log_entry_to_json(const log_entry_t *entry, char *buf, size_t buflen)
{
    if (!entry || !buf || buflen == 0) return -1;

    int offset = 0;
    int ret;
    char escaped[LOG_MESSAGE_MAX_LEN * 2 + 1];

    /* 开始 JSON 对象 */
    ret = snprintf(buf + offset, buflen - offset, "{");
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    /* node_id */
    json_escape(entry->node_id, escaped, sizeof(escaped));
    ret = snprintf(buf + offset, buflen - offset,
                  "\"node_id\":\"%s\"", escaped);
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    /* vector_clock */
    ret = snprintf(buf + offset, buflen - offset, ",\"vector_clock\":");
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    ret = vc_to_json(&entry->vc, NULL, buf + offset, buflen - offset);
    if (ret < 0) return -1;
    offset += ret;

    /* timestamp */
    ret = snprintf(buf + offset, buflen - offset,
                  ",\"timestamp\":%" PRIu64, entry->timestamp);
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    /* level */
    ret = snprintf(buf + offset, buflen - offset,
                  ",\"level\":\"%s\"", LOG_LEVEL_STR(entry->level));
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    /* message */
    json_escape(entry->message, escaped, sizeof(escaped));
    ret = snprintf(buf + offset, buflen - offset,
                  ",\"message\":\"%s\"", escaped);
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    /* metadata (如果有) */
    if (entry->metadata_count > 0) {
        ret = snprintf(buf + offset, buflen - offset, ",\"metadata\":{");
        if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
        offset += ret;

        for (int i = 0; i < entry->metadata_count; i++) {
            if (i > 0) {
                ret = snprintf(buf + offset, buflen - offset, ",");
                if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
                offset += ret;
            }

            json_escape(entry->metadata[i].key, escaped, sizeof(escaped));
            ret = snprintf(buf + offset, buflen - offset, "\"%s\":", escaped);
            if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
            offset += ret;

            json_escape(entry->metadata[i].value, escaped, sizeof(escaped));
            ret = snprintf(buf + offset, buflen - offset, "\"%s\"", escaped);
            if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
            offset += ret;
        }

        ret = snprintf(buf + offset, buflen - offset, "}");
        if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
        offset += ret;
    }

    /* 结束 JSON 对象 */
    ret = snprintf(buf + offset, buflen - offset, "}");
    if (ret < 0 || (size_t)ret >= buflen - offset) return -1;
    offset += ret;

    return offset;
}

/* 辅助函数：跳过空白字符 */
static const char *skip_ws(const char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* 辅助函数：解析 JSON 字符串 */
static const char *parse_str(const char *s, char *buf, size_t buflen)
{
    s = skip_ws(s);
    if (*s != '"') return NULL;
    s++;

    size_t i = 0;
    while (*s && *s != '"' && i < buflen - 1) {
        if (*s == '\\') {
            s++;
            switch (*s) {
                case '"':  buf[i++] = '"'; break;
                case '\\': buf[i++] = '\\'; break;
                case 'n':  buf[i++] = '\n'; break;
                case 't':  buf[i++] = '\t'; break;
                case 'r':  buf[i++] = '\r'; break;
                default:   buf[i++] = *s; break;
            }
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
static const char *parse_num(const char *s, uint64_t *value)
{
    s = skip_ws(s);
    *value = 0;

    while (isdigit((unsigned char)*s)) {
        *value = *value * 10 + (*s - '0');
        s++;
    }

    return s;
}

/* 从 JSON 字符串解析日志条目 */
int log_entry_from_json(log_entry_t *entry, const char *json)
{
    if (!entry || !json) return -1;

    memset(entry, 0, sizeof(log_entry_t));

    const char *s = skip_ws(json);
    if (*s != '{') return -1;
    s = skip_ws(s + 1);

    char key[64];
    char value[LOG_MESSAGE_MAX_LEN * 2];

    while (*s && *s != '}') {
        /* 解析键 */
        s = parse_str(s, key, sizeof(key));
        if (!s) return -1;

        /* 跳过 ':' */
        s = skip_ws(s);
        if (*s != ':') return -1;
        s = skip_ws(s + 1);

        /* 根据键解析值 */
        if (strcmp(key, "node_id") == 0) {
            s = parse_str(s, entry->node_id, sizeof(entry->node_id));
            if (!s) return -1;
        } else if (strcmp(key, "vector_clock") == 0) {
            /* 找到 JSON 对象的结束位置 */
            const char *start = s;
            int depth = 0;
            if (*s == '{') {
                do {
                    if (*s == '{') depth++;
                    else if (*s == '}') depth--;
                    s++;
                } while (*s && depth > 0);
            }
            /* 临时截取并解析 */
            size_t len = s - start;
            char *vc_str = malloc(len + 1);
            if (vc_str) {
                strncpy(vc_str, start, len);
                vc_str[len] = '\0';
                vc_from_json(&entry->vc, vc_str, NULL);
                free(vc_str);
            }
        } else if (strcmp(key, "timestamp") == 0) {
            s = parse_num(s, &entry->timestamp);
            if (!s) return -1;
        } else if (strcmp(key, "level") == 0) {
            s = parse_str(s, value, sizeof(value));
            if (!s) return -1;
            entry->level = log_level_from_str(value);
        } else if (strcmp(key, "message") == 0) {
            s = parse_str(s, entry->message, sizeof(entry->message));
            if (!s) return -1;
        } else if (strcmp(key, "metadata") == 0) {
            /* 跳过 metadata 对象 */
            int depth = 0;
            if (*s == '{') {
                do {
                    if (*s == '{') depth++;
                    else if (*s == '}') depth--;
                    s++;
                } while (*s && depth > 0);
            }
        } else {
            /* 跳过未知字段 */
            if (*s == '"') {
                while (*s && *s != '"') s++;
                if (*s) s++;
            } else if (*s == '{') {
                int depth = 0;
                do {
                    if (*s == '{') depth++;
                    else if (*s == '}') depth--;
                    s++;
                } while (*s && depth > 0);
            } else if (*s == '[') {
                int depth = 0;
                do {
                    if (*s == '[') depth++;
                    else if (*s == ']') depth--;
                    s++;
                } while (*s && depth > 0);
            } else {
                while (*s && *s != ',' && *s != '}') s++;
            }
        }

        /* 跳过 ',' */
        s = skip_ws(s);
        if (*s == ',') {
            s = skip_ws(s + 1);
        }
    }

    return 0;
}

/* 比较两个日志条目 (因果排序) */
int log_entry_compare(const void *a, const void *b)
{
    const log_entry_t *ea = (const log_entry_t *)a;
    const log_entry_t *eb = (const log_entry_t *)b;

    vc_relation_t rel = vc_compare(&ea->vc, &eb->vc);

    switch (rel) {
        case VC_HAPPENS_BEFORE:
            return -1;
        case VC_HAPPENS_AFTER:
            return 1;
        case VC_EQUAL:
            /* 向量时钟相同，按时间戳排序 */
            if (ea->timestamp != eb->timestamp) {
                return (ea->timestamp < eb->timestamp) ? -1 : 1;
            }
            /* 时间戳也相同，按节点 ID 排序 */
            return strcmp(ea->node_id, eb->node_id);
        case VC_CONCURRENT:
        default:
            /* 并发事件，按时间戳排序 */
            if (ea->timestamp != eb->timestamp) {
                return (ea->timestamp < eb->timestamp) ? -1 : 1;
            }
            /* 时间戳相同，按节点 ID 排序 */
            return strcmp(ea->node_id, eb->node_id);
    }
}

/* 比较两个日志条目 (仅按时间戳) */
int log_entry_compare_by_time(const void *a, const void *b)
{
    const log_entry_t *ea = (const log_entry_t *)a;
    const log_entry_t *eb = (const log_entry_t *)b;

    if (ea->timestamp != eb->timestamp) {
        return (ea->timestamp < eb->timestamp) ? -1 : 1;
    }

    return strcmp(ea->node_id, eb->node_id);
}

/* 获取当前时间戳 (毫秒) */
uint64_t get_current_timestamp_ms(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (t - 116444736000000000ULL) / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/* 时间戳转换为可读字符串 */
int timestamp_to_string(uint64_t timestamp, char *buf, size_t buflen)
{
    if (!buf || buflen == 0) return -1;

    time_t sec = timestamp / 1000;
    uint32_t ms = timestamp % 1000;

    struct tm *tm = localtime(&sec);
    if (!tm) return -1;

    int ret = snprintf(buf, buflen,
                      "%04d-%02d-%02d %02d:%02d:%02d.%03u",
                      tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                      tm->tm_hour, tm->tm_min, tm->tm_sec, ms);

    return ret;
}