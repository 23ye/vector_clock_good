#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* 全局 Server 指针，用于信号处理 */
static server_t *g_server = NULL;

/* 信号处理函数 */
static void signal_handler(int sig)
{
    (void)sig;
    printf("\nReceived signal, stopping server...\n");
    if (g_server) {
        server_stop(g_server);
    }
}

/* 打印使用说明 */
static void print_usage(const char *prog_name)
{
    printf("LogAgg Server - Distributed Log Aggregation Server\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s [options]\n", prog_name);
    printf("\n");
    printf("Options:\n");
    printf("  -p <port>     Listen port (default: 9999)\n");
    printf("  -d <dir>      Storage directory (default: ./logs)\n");
    printf("  -h            Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s\n", prog_name);
    printf("  %s -p 8888 -d /var/log/logagg\n", prog_name);
    printf("  %s -p 9999 -d C:\\logs\\logagg\n", prog_name);
}

int main(int argc, char *argv[])
{
    server_config_t config;

    /* 初始化配置 */
    memset(&config, 0, sizeof(config));
    config.port = 9999;  /* 默认端口 */
    strncpy(config.storage_dir, "./logs", sizeof(config.storage_dir) - 1);

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -p requires an argument\n");
                return 1;
            }
            int port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Error: Invalid port number '%s'\n", argv[i]);
                return 1;
            }
            config.port = (uint16_t)port;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -d requires an argument\n");
                return 1;
            }
            strncpy(config.storage_dir, argv[++i],
                    sizeof(config.storage_dir) - 1);
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* 初始化网络 */
    if (server_network_init() != 0) {
        return 1;
    }

    /* 初始化 Server */
    server_t server;
    if (server_init(&server, &config) != 0) {
        fprintf(stderr, "Error: Failed to initialize server\n");
        server_network_cleanup();
        return 1;
    }

    /* 设置全局指针和信号处理 */
    g_server = &server;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 打印启动信息 */
    printf("Starting LogAgg Server...\n");
    printf("  Port:      %d\n", config.port);
    printf("  Storage:   %s\n", config.storage_dir);
    printf("\nPress Ctrl+C to stop.\n");

    /* 运行 Server */
    int ret = server_run(&server);

    /* 打印统计信息 */
    server_print_stats(&server);

    /* 清理 */
    server_cleanup(&server);
    server_network_cleanup();

    printf("Server stopped.\n");
    return ret;
}