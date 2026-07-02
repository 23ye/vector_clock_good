#ifndef SERVER_H
#define SERVER_H

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

/* Server 配置 */
typedef struct {
    uint16_t port;              /* 监听端口 */
    char storage_dir[256];      /* 日志存储目录 */
    int buffer_size;            /* 接收缓冲区大小 */
} server_config_t;

/* Server 统计信息 */
typedef struct {
    uint64_t total_received;    /* 总接收日志数 */
    uint64_t total_stored;      /* 总存储日志数 */
    uint64_t total_errors;      /* 总错误数 */
    uint64_t bytes_received;    /* 总接收字节数 */
} server_stats_t;

/* Server 状态 */
typedef struct {
    server_config_t config;     /* 配置 */
    socket_t udp_sock;          /* UDP socket */
    FILE *log_fp;               /* 当前日志文件句柄 */
    char current_date[16];      /* 当前日期 (YYYY-MM-DD) */
    server_stats_t stats;       /* 统计信息 */
    bool running;               /* 运行状态 */
} server_t;

/*
 * 初始化网络 (Windows 需要)
 * @return 0 成功，-1 失败
 */
int server_network_init(void);

/*
 * 清理网络资源 (Windows 需要)
 */
void server_network_cleanup(void);

/*
 * 初始化 Server
 * @param server Server 状态指针
 * @param config 配置
 * @return 0 成功，-1 失败
 */
int server_init(server_t *server, const server_config_t *config);

/*
 * 启动 Server 主循环
 * @param server Server 状态指针
 * @return 0 正常退出，-1 错误
 */
int server_run(server_t *server);

/*
 * 停止 Server
 * @param server Server 状态指针
 */
void server_stop(server_t *server);

/*
 * 清理 Server 资源
 * @param server Server 状态指针
 */
void server_cleanup(server_t *server);

/*
 * 获取统计信息
 * @param server Server 状态指针
 * @param stats  输出的统计信息
 */
void server_get_stats(const server_t *server, server_stats_t *stats);

/*
 * 打印统计信息
 * @param server Server 状态指针
 */
void server_print_stats(const server_t *server);

/*
 * 存储单条日志到文件
 * @param server Server 状态指针
 * @param json   JSON 字符串
 * @param len    字符串长度
 * @return 0 成功，-1 失败
 */
int server_store_log(server_t *server, const char *json, int len);

/*
 * 打开或切换日志文件
 * @param server Server 状态指针
 * @return 0 成功，-1 失败
 */
int server_open_log_file(server_t *server);

/*
 * 获取当前日期字符串 (YYYY-MM-DD)
 * @param buf    输出缓冲区
 * @param buflen 缓冲区大小
 * @return 0 成功，-1 失败
 */
int get_current_date(char *buf, size_t buflen);

#endif /* SERVER_H */