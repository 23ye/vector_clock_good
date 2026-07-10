#include "agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* 初始化网络 (Windows 需要初始化 Winsock) */
int agent_network_init(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (ret != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", ret);
        return -1;
    }
#endif
    return 0;
}

/* 清理网络资源 */
void agent_network_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

/* 初始化 Agent */
int agent_init(agent_t *agent, const agent_config_t *config, int node_count)
{
    if (!agent || !config) return -1;

    memset(agent, 0, sizeof(agent_t));
    memcpy(&agent->config, config, sizeof(agent_config_t));

    /* 初始化向量时钟 */
    vc_init(&agent->vc, node_count);

    /* 初始化序号 */
    agent->seq = 0;

    /* 设置运行状态 */
    agent->running = false;

    /* 打开日志文件 */
    agent->log_fp = fopen(config->log_path, "r");
    if (!agent->log_fp) {
        fprintf(stderr, "Warning: Cannot open log file '%s', will retry...\n",
                config->log_path);
        /* 不返回错误，文件可能稍后创建 */
    } else {
        /* 移动到文件末尾，只监控新增内容 */
        fseek(agent->log_fp, 0, SEEK_END);
        agent->last_offset = ftell(agent->log_fp);
    }

    /* 创建 UDP socket */
    agent->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (agent->udp_sock == INVALID_SOCKET) {
#else
    if (agent->udp_sock < 0) {
#endif
        fprintf(stderr, "Error: Failed to create UDP socket\n");
        return -1;
    }

    /* 配置 Server 地址 */
    memset(&agent->server_addr, 0, sizeof(agent->server_addr));
    agent->server_addr.sin_family = AF_INET;
    agent->server_addr.sin_port = htons(config->server_port);

    if (inet_pton(AF_INET, config->server_host,
                  &agent->server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Error: Invalid server address '%s'\n",
                config->server_host);
        return -1;
    }

    return 0;
}

/* 发送单条日志到 Server */
int agent_send_log(agent_t *agent, const log_entry_t *entry)
{
    if (!agent || !entry) return -1;

    char json[LOGAGG_MAX_ENTRY_SIZE];
    int len = log_entry_to_json(entry, json, sizeof(json));
    if (len < 0) {
        fprintf(stderr, "Error: Failed to serialize log entry\n");
        return -1;
    }

    int ret = sendto(agent->udp_sock, json, len, 0,
                     (struct sockaddr *)&agent->server_addr,
                     sizeof(agent->server_addr));

#ifdef _WIN32
    if (ret == SOCKET_ERROR) {
        fprintf(stderr, "Error: sendto failed: %d\n", WSAGetLastError());
        return -1;
    }
#else
    if (ret < 0) {
        perror("sendto failed");
        return -1;
    }
#endif

    return 0;
}

/* 解析日志行
 * 支持的格式:
 *   [VC:1,2,3] 2024-06-29 10:00:01 INFO  Request started  (带向量时钟前缀)
 *   2024-06-29 10:00:01 INFO  Request started              (无前缀)
 *   [INFO] Request started
 *   INFO: Request started
 *
 * 如果 parsed_vc 非空且日志行包含 [VC:...]，则填入解析到的向量时钟值
 */
int agent_parse_line(const char *line, log_entry_t *entry, int *parsed_vc)
{
    if (!line || !entry) return -1;

    /* 跳过空行 */
    if (line[0] == '\0' || line[0] == '\n') return -1;

    const char *content = line;
    int vc_local[3] = {0, 0, 0};
    bool has_vc = false;

    /* 尝试解析 [VC:x,y,z] 前缀 */
    if (strncmp(content, "[VC:", 4) == 0) {
        int a, b, c;
        if (sscanf(content + 4, "%d,%d,%d", &a, &b, &c) == 3) {
            vc_local[0] = a;
            vc_local[1] = b;
            vc_local[2] = c;
            has_vc = true;
            /* 跳过 ']' 之后的内容 */
            const char *bracket = strchr(content, ']');
            if (bracket) {
                content = bracket + 1;
                /* 跳过空格 */
                while (*content == ' ') content++;
            }
        }
    }

    /* 输出解析到的向量时钟 */
    if (parsed_vc) {
        if (has_vc) {
            parsed_vc[0] = vc_local[0];
            parsed_vc[1] = vc_local[1];
            parsed_vc[2] = vc_local[2];
        } else {
            parsed_vc[0] = parsed_vc[1] = parsed_vc[2] = -1;  /* 标记未解析到 */
        }
    }

    char level_str[16] = {0};
    char message[LOG_MESSAGE_MAX_LEN + 1] = {0};
    uint64_t timestamp = get_current_timestamp_ms();

    /* 尝试解析格式: "2024-06-29 10:00:01 INFO  Message" */
    int year, month, day, hour, min, sec, ms = 0;
    int parsed = sscanf(content, "%d-%d-%d %d:%d:%d.%d %15s %[^\n]",
                        &year, &month, &day, &hour, &min, &sec, &ms,
                        level_str, message);

    if (parsed >= 8) {
        /* 成功解析时间戳和级别 */
        struct tm t = {0};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_sec = sec;

        timestamp = (uint64_t)mktime(&t) * 1000 + ms;
    } else {
        /* 尝试解析格式: "[INFO] Message" 或 "INFO: Message" */
        parsed = sscanf(content, "[%15[^]]] %[^\n]", level_str, message);
        if (parsed < 2) {
            parsed = sscanf(content, "%15[^:]: %[^\n]", level_str, message);
        }
        if (parsed < 2) {
            /* 无法解析，整行作为 message，级别默认 INFO */
            strncpy(level_str, "INFO", sizeof(level_str) - 1);
            strncpy(message, content, LOG_MESSAGE_MAX_LEN);
            /* 去掉末尾换行符 */
            size_t len = strlen(message);
            if (len > 0 && message[len - 1] == '\n') {
                message[len - 1] = '\0';
            }
        }
    }

    /* 初始化日志条目 */
    log_entry_init(entry, NULL, NULL, LOG_INFO, message);

    /* 设置解析到的字段 */
    entry->timestamp = timestamp;
    entry->level = log_level_from_str(level_str);

    return 0;
}

/* 检查文件是否有新内容 (轮询方式) */
static bool check_file_changed(agent_t *agent)
{
    if (!agent->log_fp) {
        /* 尝试打开文件 */
        agent->log_fp = fopen(agent->config.log_path, "r");
        if (!agent->log_fp) return false;
        agent->last_offset = 0;
        return true;
    }

    /* 获取当前文件大小 */
    fseek(agent->log_fp, 0, SEEK_END);
    uint64_t current_size = ftell(agent->log_fp);

    if (current_size > agent->last_offset) {
        return true;
    }

    /* 文件被截断 (日志轮转) */
    if (current_size < agent->last_offset) {
        agent->last_offset = 0;
        return true;
    }

    return false;
}

/* 读取新增日志行并发送 */
static int process_new_lines(agent_t *agent)
{
    if (!agent->log_fp) return -1;

    /* 移动到上次读取位置 */
    fseek(agent->log_fp, agent->last_offset, SEEK_SET);

    char line[LOG_MESSAGE_MAX_LEN + 1];
    int count = 0;

    while (fgets(line, sizeof(line), agent->log_fp)) {
        /* 解析日志行（提取向量时钟前缀） */
        log_entry_t entry;
        int parsed_vc[3] = {-1, -1, -1};
        if (agent_parse_line(line, &entry, parsed_vc) == 0) {
            /* 设置节点 ID */
            strncpy(entry.node_id, agent->config.node_id, NODE_ID_MAX_LEN);

            /* 从 node_id 中提取数字索引（如 "node-01" -> 0, "node-02" -> 1）
             * 向量时钟使用 0-based 索引，所以减 1
             * 回退策略：如果找不到数字，使用 node_id 字符串的哈希值取模 */
            int my_node_idx = 0;
            const char *p = agent->config.node_id;
            while (*p && !isdigit((unsigned char)*p)) p++;
            if (*p) {
                my_node_idx = atoi(p) - 1;  /* 1-based -> 0-based */
                if (my_node_idx < 0) my_node_idx = 0;
            } else {
                /* 回退：用字符串哈希 */
                unsigned hash = 0;
                for (const char *s = agent->config.node_id; *s; s++) {
                    hash = hash * 31 + (unsigned char)*s;
                }
                my_node_idx = (int)(hash % 32);
            }

            /* 如果日志行携带了向量时钟，只合并其他节点的分量（跳过自己的，避免重复计数） */
            if (parsed_vc[0] >= 0) {
                for (int i = 0; i < 3; i++) {
                    if (i != my_node_idx && (uint64_t)parsed_vc[i] > agent->vc.clocks[i]) {
                        agent->vc.clocks[i] = (uint64_t)parsed_vc[i];
                    }
                }
            }

            /* 更新向量时钟 */
            vc_increment(&agent->vc, my_node_idx);
            vc_copy(&entry.vc, &agent->vc);

            /* 设置序号 */
            entry.seq = agent->seq++;

            /* 发送到 Server */
            if (agent_send_log(agent, &entry) == 0) {
                count++;
            }
        }
    }

    /* 更新偏移量 */
    agent->last_offset = ftell(agent->log_fp);

    return count;
}

/* Agent 主循环 */
int agent_run(agent_t *agent)
{
    if (!agent) return -1;

    agent->running = true;

    printf("Agent started: node_id=%s, file=%s, server=%s:%d\n",
           agent->config.node_id,
           agent->config.log_path,
           agent->config.server_host,
           agent->config.server_port);

    while (agent->running) {
        /* 检查文件是否有新内容 */
        if (check_file_changed(agent)) {
            int count = process_new_lines(agent);
            if (count > 0) {
                printf("Sent %d log entries\n", count);
            }
        }

        /* 等待下一次轮询 */
        sleep_ms(agent->config.poll_interval_ms);
    }

    return 0;
}

/* 停止 Agent */
void agent_stop(agent_t *agent)
{
    if (agent) {
        agent->running = false;
    }
}

/* 清理 Agent 资源 */
void agent_cleanup(agent_t *agent)
{
    if (!agent) return;

    if (agent->log_fp) {
        fclose(agent->log_fp);
        agent->log_fp = NULL;
    }

#ifdef _WIN32
    if (agent->udp_sock != INVALID_SOCKET) {
        closesocket(agent->udp_sock);
        agent->udp_sock = INVALID_SOCKET;
    }
#else
    if (agent->udp_sock >= 0) {
        close(agent->udp_sock);
        agent->udp_sock = -1;
    }
#endif
}