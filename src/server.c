#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/* 初始化网络 */
int server_network_init(void)
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
void server_network_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

/* 获取当前日期字符串 */
int get_current_date(char *buf, size_t buflen)
{
    if (!buf || buflen < 11) return -1;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) return -1;

    snprintf(buf, buflen, "%04d-%02d-%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    return 0;
}

/* 确保目录存在 */
static int ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return 0;  /* 目录已存在 */
    }

    return mkdir(path, 0755);
}

/* 打开或切换日志文件 */
int server_open_log_file(server_t *server)
{
    if (!server) return -1;

    char today[16];
    get_current_date(today, sizeof(today));

    /* 如果日期没变且文件已打开，不需要切换 */
    if (server->log_fp && strcmp(server->current_date, today) == 0) {
        return 0;
    }

    /* 关闭旧文件 */
    if (server->log_fp) {
        fclose(server->log_fp);
        server->log_fp = NULL;
    }

    /* 确保存储目录存在 */
    if (ensure_directory(server->config.storage_dir) != 0) {
        fprintf(stderr, "Error: Cannot create directory '%s'\n",
                server->config.storage_dir);
        return -1;
    }

    /* 构建文件路径: logs/YYYY-MM-DD.jsonl */
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s.jsonl",
             server->config.storage_dir, today);

    /* 打开文件 (追加模式) */
    server->log_fp = fopen(filepath, "a");
    if (!server->log_fp) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filepath);
        return -1;
    }

    /* 更新当前日期 */
    strncpy(server->current_date, today, sizeof(server->current_date) - 1);

    printf("Opened log file: %s\n", filepath);
    return 0;
}

/* 初始化 Server */
int server_init(server_t *server, const server_config_t *config)
{
    if (!server || !config) return -1;

    memset(server, 0, sizeof(server_t));
    memcpy(&server->config, config, sizeof(server_config_t));

    /* 设置默认值 */
    if (server->config.port == 0) {
        server->config.port = 9999;
    }
    if (server->config.storage_dir[0] == '\0') {
        strncpy(server->config.storage_dir, "./logs",
                sizeof(server->config.storage_dir) - 1);
    }
    if (server->config.buffer_size == 0) {
        server->config.buffer_size = LOGAGG_MAX_ENTRY_SIZE;
    }

    /* 创建 UDP socket */
    server->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (server->udp_sock == INVALID_SOCKET) {
#else
    if (server->udp_sock < 0) {
#endif
        fprintf(stderr, "Error: Failed to create UDP socket\n");
        return -1;
    }

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->config.port);

    if (bind(server->udp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error: Failed to bind port %d\n", server->config.port);
        return -1;
    }

    /* 打开日志文件 */
    if (server_open_log_file(server) != 0) {
        return -1;
    }

    server->running = false;
    return 0;
}

/* 存储单条日志到文件 */
int server_store_log(server_t *server, const char *json, int len)
{
    if (!server || !json || len <= 0) return -1;

    /* 检查是否需要切换日期 */
    server_open_log_file(server);

    if (!server->log_fp) return -1;

    /* 写入日志 (添加换行符) */
    fwrite(json, 1, len, server->log_fp);
    fputc('\n', server->log_fp);
    fflush(server->log_fp);

    server->stats.total_stored++;
    return 0;
}

/* Server 主循环 */
int server_run(server_t *server)
{
    if (!server) return -1;

    server->running = true;

    printf("\nServer started on port %d\n", server->config.port);
    printf("Storage directory: %s\n", server->config.storage_dir);
    printf("\nWaiting for Agent connections...\n\n");

    char *buffer = malloc(server->config.buffer_size);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate buffer\n");
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (server->running) {
        /* 接收数据 */
        int n = recvfrom(server->udp_sock, buffer, server->config.buffer_size - 1,
                         0, (struct sockaddr *)&client_addr, &client_len);

        if (n <= 0) {
#ifdef _WIN32
            if (n == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEINTR) continue;  /* 被信号中断 */
                fprintf(stderr, "recvfrom error: %d\n", err);
            }
#else
            if (n < 0) {
                perror("recvfrom");
            }
#endif
            server->stats.total_errors++;
            continue;
        }

        buffer[n] = '\0';
        server->stats.bytes_received += n;
        server->stats.total_received++;

        /* 获取客户端信息 */
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        /* 存储日志 */
        if (server_store_log(server, buffer, n) == 0) {
            /* 每 100 条打印一次状态 */
            if (server->stats.total_received % 100 == 0) {
                printf("Received %lu logs from %s:%d\n",
                       (unsigned long)server->stats.total_received,
                       client_ip, client_port);
            }
        } else {
            fprintf(stderr, "Failed to store log from %s:%d\n",
                    client_ip, client_port);
            server->stats.total_errors++;
        }
    }

    free(buffer);
    return 0;
}

/* 停止 Server */
void server_stop(server_t *server)
{
    if (server) {
        server->running = false;
    }
}

/* 清理 Server 资源 */
void server_cleanup(server_t *server)
{
    if (!server) return;

    if (server->log_fp) {
        fclose(server->log_fp);
        server->log_fp = NULL;
    }

#ifdef _WIN32
    if (server->udp_sock != INVALID_SOCKET) {
        closesocket(server->udp_sock);
        server->udp_sock = INVALID_SOCKET;
    }
#else
    if (server->udp_sock >= 0) {
        close(server->udp_sock);
        server->udp_sock = -1;
    }
#endif
}

/* 获取统计信息 */
void server_get_stats(const server_t *server, server_stats_t *stats)
{
    if (server && stats) {
        memcpy(stats, &server->stats, sizeof(server_stats_t));
    }
}

/* 打印统计信息 */
void server_print_stats(const server_t *server)
{
    if (!server) return;

    printf("\n========== Server Statistics ==========\n");
    printf("  Total Received:  %lu\n", (unsigned long)server->stats.total_received);
    printf("  Total Stored:    %lu\n", (unsigned long)server->stats.total_stored);
    printf("  Total Errors:    %lu\n", (unsigned long)server->stats.total_errors);
    printf("  Bytes Received:  %lu\n", (unsigned long)server->stats.bytes_received);
    printf("========================================\n");
}