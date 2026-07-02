#ifndef AGENT_H
#define AGENT_H

#include "logagg.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

/* Agent 配置 */
typedef struct {
    char node_id[NODE_ID_MAX_LEN + 1];  /* 节点 ID */
    char log_path[256];                  /* 监控的日志文件路径 */
    char server_host[128];               /* Server IP 地址 */
    uint16_t server_port;                /* Server 端口 */
    uint32_t poll_interval_ms;           /* 轮询间隔 (毫秒) */
} agent_config_t;

/* Agent 状态 */
typedef struct {
    agent_config_t config;       /* 配置 */
    vector_clock_t vc;           /* 向量时钟 */
    socket_t udp_sock;           /* UDP socket */
    struct sockaddr_in server_addr;  /* Server 地址 */
    FILE *log_fp;                /* 日志文件句柄 */
    uint64_t last_offset;        /* 上次读取的文件偏移量 */
    uint64_t seq;                /* 同节点内递增序号 */
    bool running;                /* 运行状态 */
} agent_t;

/*
 * 初始化 Agent
 * @param agent  Agent 状态指针
 * @param config 配置
 * @param node_count 集群节点总数
 * @return 0 成功，-1 失败
 */
int agent_init(agent_t *agent, const agent_config_t *config, int node_count);

/*
 * 启动 Agent 主循环
 * @param agent Agent 状态指针
 * @return 0 正常退出，-1 错误
 */
int agent_run(agent_t *agent);

/*
 * 停止 Agent
 * @param agent Agent 状态指针
 */
void agent_stop(agent_t *agent);

/*
 * 清理 Agent 资源
 * @param agent Agent 状态指针
 */
void agent_cleanup(agent_t *agent);

/*
 * 发送单条日志到 Server
 * @param agent Agent 状态指针
 * @param entry 日志条目
 * @return 0 成功，-1 失败
 */
int agent_send_log(agent_t *agent, const log_entry_t *entry);

/*
 * 解析日志行
 * @param line   原始日志行
 * @param entry  输出的日志条目
 * @return 0 成功，-1 失败
 */
int agent_parse_line(const char *line, log_entry_t *entry);

/*
 * 初始化网络 (Windows 需要)
 * @return 0 成功，-1 失败
 */
int agent_network_init(void);

/*
 * 清理网络资源 (Windows 需要)
 */
void agent_network_cleanup(void);

#endif /* AGENT_H */