#include "agent.h"
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
    printf("\nReceived signal, stopping agent...\n");
    if (g_agent) {
        agent_stop(g_agent);
    }
}

/* 打印使用说明 */
static void print_usage(const char *prog_name)
{
    printf("LogAgg Agent - Distributed Log Collection Agent\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s -n <node_id> -f <log_file> -s <server_addr> [options]\n", prog_name);
    printf("\n");
    printf("Required:\n");
    printf("  -n <node_id>      Node identifier (e.g., node-01)\n");
    printf("  -f <log_file>     Path to log file to monitor\n");
    printf("  -s <server_addr>  Server address in format host:port (e.g., 127.0.0.1:9999)\n");
    printf("\n");
    printf("Options:\n");
    printf("  -i <interval>     Poll interval in milliseconds (default: 1000)\n");
    printf("  -c <node_count>   Total number of nodes in cluster (default: 3)\n");
    printf("  -h                Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -n node-01 -f /var/log/app.log -s 127.0.0.1:9999\n", prog_name);
    printf("  %s -n web-server -f C:\\logs\\app.log -s 192.168.1.100:9999 -i 500\n", prog_name);
}

/* 解析 server 地址 */
static int parse_server_addr(const char *addr_str, char *host, size_t host_len,
                             uint16_t *port)
{
    const char *colon = strrchr(addr_str, ':');
    if (!colon) {
        fprintf(stderr, "Error: Invalid server address format '%s'\n", addr_str);
        fprintf(stderr, "Expected format: host:port (e.g., 127.0.0.1:9999)\n");
        return -1;
    }

    size_t host_part_len = colon - addr_str;
    if (host_part_len >= host_len) {
        fprintf(stderr, "Error: Host name too long\n");
        return -1;
    }

    strncpy(host, addr_str, host_part_len);
    host[host_part_len] = '\0';

    int p = atoi(colon + 1);
    if (p <= 0 || p > 65535) {
        fprintf(stderr, "Error: Invalid port number '%s'\n", colon + 1);
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
                fprintf(stderr, "Error: -n requires an argument\n");
                return 1;
            }
            strncpy(config.node_id, argv[++i], NODE_ID_MAX_LEN);
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -f requires an argument\n");
                return 1;
            }
            strncpy(config.log_path, argv[++i], sizeof(config.log_path) - 1);
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -s requires an argument\n");
                return 1;
            }
            if (parse_server_addr(argv[++i], config.server_host,
                                  sizeof(config.server_host),
                                  &config.server_port) != 0) {
                return 1;
            }
        } else if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -i requires an argument\n");
                return 1;
            }
            config.poll_interval_ms = (uint32_t)atoi(argv[++i]);
            if (config.poll_interval_ms == 0) {
                config.poll_interval_ms = 1000;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -c requires an argument\n");
                return 1;
            }
            node_count = atoi(argv[++i]);
            if (node_count <= 0 || node_count > 32) {
                fprintf(stderr, "Error: Node count must be between 1 and 32\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* 检查必需参数 */
    if (config.node_id[0] == '\0') {
        fprintf(stderr, "Error: Node ID (-n) is required\n");
        print_usage(argv[0]);
        return 1;
    }
    if (config.log_path[0] == '\0') {
        fprintf(stderr, "Error: Log file path (-f) is required\n");
        print_usage(argv[0]);
        return 1;
    }
    if (config.server_host[0] == '\0' || config.server_port == 0) {
        fprintf(stderr, "Error: Server address (-s) is required\n");
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
        fprintf(stderr, "Error: Failed to initialize agent\n");
        agent_network_cleanup();
        return 1;
    }

    /* 设置全局指针和信号处理 */
    g_agent = &agent;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 运行 Agent */
    printf("Starting LogAgg Agent...\n");
    printf("  Node ID:      %s\n", config.node_id);
    printf("  Log File:     %s\n", config.log_path);
    printf("  Server:       %s:%d\n", config.server_host, config.server_port);
    printf("  Poll Interval: %u ms\n", config.poll_interval_ms);
    printf("  Node Count:   %d\n", node_count);
    printf("\nPress Ctrl+C to stop.\n\n");

    int ret = agent_run(&agent);

    /* 清理 */
    agent_cleanup(&agent);
    agent_network_cleanup();

    printf("Agent stopped.\n");
    return ret;
}