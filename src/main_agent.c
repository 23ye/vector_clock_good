#include "agent.h"
#include "i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* 全局 Agent 指针，用于信号处理 */
static agent_t *g_agent = NULL;

/* 信号处理函数 */
static void signal_handler(int sig)
{
    (void)sig;
    printf("\n" I18N_AGENT_RECEIVED_SIGNAL "\n");
    if (g_agent) {
        agent_stop(g_agent);
    }
}

/* 打印使用说明 */
static void print_usage(const char *prog_name)
{
    printf("LogAgg Agent\n");
    printf("\n");
    printf(I18N_QUERY_USAGE ":\n");
    printf("  %s -n <node_id> -f <log_file> -s <server_addr> [options]\n", prog_name);
    printf("\n");
    printf(I18N_QUERY_REQUIRED ":\n");
    printf("  -n <node_id>      " I18N_AGENT_NODE_ID "\n");
    printf("  -f <log_file>     " I18N_AGENT_LOG_FILE "\n");
    printf("  -s <server_addr>  " I18N_AGENT_SERVER " host:port\n");
    printf("\n");
    printf(I18N_QUERY_OPTIONS ":\n");
    printf("  -i <interval>     " I18N_AGENT_POLL_INTERVAL " (ms, default: 1000)\n");
    printf("  -c <node_count>   " I18N_AGENT_NODE_COUNT " (default: 3)\n");
    printf("  -h                " I18N_QUERY_USAGE "\n");
    printf("\n");
    printf(I18N_QUERY_EXAMPLES ":\n");
    printf("  %s -n node-01 -f /var/log/app.log -s 127.0.0.1:9999\n", prog_name);
    printf("  %s -n web-server -f C:\\logs\\app.log -s 192.168.1.100:9999 -i 500\n", prog_name);
}

/* 解析 server 地址 */
static int parse_server_addr(const char *addr_str, char *host, size_t host_len,
                             uint16_t *port)
{
    const char *colon = strrchr(addr_str, ':');
    if (!colon) {
        fprintf(stderr, I18N_ERROR ": " I18N_ERR_INVALID_PARAM " '%s'\n", addr_str);
        return -1;
    }

    size_t host_part_len = colon - addr_str;
    if (host_part_len >= host_len) {
        fprintf(stderr, I18N_ERROR ": Host name too long\n");
        return -1;
    }

    strncpy(host, addr_str, host_part_len);
    host[host_part_len] = '\0';

    int p = atoi(colon + 1);
    if (p <= 0 || p > 65535) {
        fprintf(stderr, I18N_ERROR ": " I18N_ERR_INVALID_PARAM " port '%s'\n", colon + 1);
        return -1;
    }

    *port = (uint16_t)p;
    return 0;
}

int main(int argc, char *argv[])
{
    agent_config_t config;
    int node_count = 3;  /* 默认 3 个节点 */

    /* 初始化配置 */
    memset(&config, 0, sizeof(config));
    config.poll_interval_ms = 1000;  /* 默认 1 秒轮询 */

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -n " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            strncpy(config.node_id, argv[++i], NODE_ID_MAX_LEN);
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -f " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            strncpy(config.log_path, argv[++i], sizeof(config.log_path) - 1);
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -s " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            if (parse_server_addr(argv[++i], config.server_host,
                                  sizeof(config.server_host),
                                  &config.server_port) != 0) {
                return 1;
            }
        } else if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -i " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            config.poll_interval_ms = (uint32_t)atoi(argv[++i]);
            if (config.poll_interval_ms == 0) {
                config.poll_interval_ms = 1000;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, I18N_ERROR ": -c " I18N_ERR_MISSING_PARAM "\n");
                return 1;
            }
            node_count = atoi(argv[++i]);
            if (node_count <= 0 || node_count > 32) {
                fprintf(stderr, I18N_ERROR ": " I18N_AGENT_NODE_COUNT " 1-32\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, I18N_ERROR ": " I18N_ERR_INVALID_PARAM " '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* 检查必需参数 */
    if (config.node_id[0] == '\0') {
        fprintf(stderr, I18N_ERROR ": " I18N_AGENT_NODE_ID " (-n) " I18N_ERR_MISSING_PARAM "\n");
        print_usage(argv[0]);
        return 1;
    }
    if (config.log_path[0] == '\0') {
        fprintf(stderr, I18N_ERROR ": " I18N_AGENT_LOG_FILE " (-f) " I18N_ERR_MISSING_PARAM "\n");
        print_usage(argv[0]);
        return 1;
    }
    if (config.server_host[0] == '\0' || config.server_port == 0) {
        fprintf(stderr, I18N_ERROR ": " I18N_AGENT_SERVER " (-s) " I18N_ERR_MISSING_PARAM "\n");
        print_usage(argv[0]);
        return 1;
    }

    /* 初始化网络 */
    if (agent_network_init() != 0) {
        return 1;
    }

    /* 初始化 Agent */
    agent_t agent;
    if (agent_init(&agent, &config, node_count) != 0) {
        fprintf(stderr, I18N_ERROR ": Agent " I18N_FAILED "\n");
        agent_network_cleanup();
        return 1;
    }

    /* 设置全局指针和信号处理 */
    g_agent = &agent;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 运行 Agent */
    printf(I18N_AGENT_STARTING "\n");
    printf("  " I18N_AGENT_NODE_ID ":       %s\n", config.node_id);
    printf("  " I18N_AGENT_LOG_FILE ":      %s\n", config.log_path);
    printf("  " I18N_AGENT_SERVER ":        %s:%d\n", config.server_host, config.server_port);
    printf("  " I18N_AGENT_POLL_INTERVAL ": %u ms\n", config.poll_interval_ms);
    printf("  " I18N_AGENT_NODE_COUNT ":    %d\n", node_count);
    printf("\n" I18N_AGENT_PRESS_CTRL ".\n\n");

    int ret = agent_run(&agent);

    /* 清理 */
    agent_cleanup(&agent);
    agent_network_cleanup();

    printf(I18N_AGENT_STOPPED ".\n");
    return ret;
}