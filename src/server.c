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

/* 内部辅助因果排序与刷新函数声明 */
static int log_entry_compare_causal(const log_entry_t *a, const log_entry_t *b);
static void server_insert_to_buffer(server_t *server, log_entry_t *entry);
static void server_flush_causal_buffer(server_t *server);
static void server_check_timeout_flush(server_t *server);
static int server_write_entry_to_file(server_t *server, const log_entry_t *entry);

/* 声明外部函数（复用 log_entry.c 中已实现的获取毫秒时间戳函数） */
extern uint64_t get_current_timestamp_ms(void);

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

    /* 初始化因果排序缓冲区相关的核心变量 */
    vc_init(&server->committed_vc, config->node_count); /* 全局已提交时钟视图初始化 */
    server->buffer_head = NULL;
    server->buffer_count = 0;

    /* 创建存储目录 */
    mkdir(server->config.storage_dir, 0777);

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

/* 函数：核心高精度因果比较器 */
static int log_entry_compare_causal(const log_entry_t *a, const log_entry_t *b)
{
    vc_relation_t rel = vc_compare(&a->vc, &b->vc);
    if (rel == VC_HAPPENS_BEFORE) return -1;
    if (rel == VC_HAPPENS_AFTER)  return 1;

    /* 并发 (VC_CONCURRENT) 或相等，用物理时间戳排序 */
    if (a->timestamp != b->timestamp) {
        return (a->timestamp < b->timestamp) ? -1 : 1;
    }
    /* 时间戳相同，用 node_id 唯一性字典序兜底 */
    return strcmp(a->node_id, b->node_id);
}

/* 函数：将新日志有序插入内存缓存链表（有氧排序） */
static void server_insert_to_buffer(server_t *server, log_entry_t *entry)
{
    log_buffer_node_t *new_node = malloc(sizeof(log_buffer_node_t));
    if (!new_node) return;

    memcpy(&new_node->entry, entry, sizeof(log_entry_t));
    new_node->arrival_time = get_current_timestamp_ms();
    new_node->next = NULL;

    /* 链表为空，或者新节点应该插入在最头部 */
    if (!server->buffer_head || log_entry_compare_causal(&new_node->entry, &server->buffer_head->entry) < 0) {
        new_node->next = server->buffer_head;
        server->buffer_head = new_node;
    } else {
        /* 迭代寻找合适的插入位置，保持整条链表的单调性 */
        log_buffer_node_t *curr = server->buffer_head;
        while (curr->next && log_entry_compare_causal(&new_node->entry, &curr->next->entry) >= 0) {
            curr = curr->next;
        }
        new_node->next = curr->next;
        curr->next = new_node;
    }
    server->buffer_count++;
}

/* 函数：因果连续性检测推进与安全落盘机制  */
static void server_flush_causal_buffer(server_t *server)
{
    bool progress = true;

    while (progress && server->buffer_head) {
        progress = false;
        log_buffer_node_t *prev = NULL;
        log_buffer_node_t *curr = server->buffer_head;

        while (curr) {
            log_entry_t *entry = &curr->entry;
            vc_relation_t rel = vc_compare(&entry->vc, &server->committed_vc);
            
            /* 防御性处理：如果收到的日志时钟序列号已经落后于已提交时钟，说明是网络重发的重复包，直接剔除 */
            if (rel == VC_EQUAL || rel == VC_HAPPENS_BEFORE) {
                log_buffer_node_t *to_free = curr;
                if (prev == NULL) {
                    server->buffer_head = curr->next;
                } else {
                    prev->next = curr->next;
                }
                curr = curr->next;
                free(to_free);
                server->buffer_count--;
                progress = true;
                break;
            }

            /* 判断这条日志是否满足因果连续条件：
               计算 entry 视图比全局 committed_vc 视图多前进了多少步 */
            int increments = 0;
            for (int i = 0; i < server->committed_vc.node_count; i++) {
                if (entry->vc.clocks[i] > server->committed_vc.clocks[i]) {
                    increments += (entry->vc.clocks[i] - server->committed_vc.clocks[i]);
                }
            }

            /* 如果恰好只多前进了 1 步，证明因果链完全闭合连续（没有中间网络丢包），可以安全落盘 */
            if (increments == 1) {
                /* 移出缓冲区并写入文件 */
                server_write_entry_to_file(server, entry);
                
                /* 核心：将该日志的时钟合并进全局视图，相当于向前推进了因果线 */
                vc_merge(&server->committed_vc, &entry->vc);

                log_buffer_node_t *to_free = curr;
                if (prev == NULL) {
                    server->buffer_head = curr->next;
                } else {
                    prev->next = curr->next;
                }
                curr = curr->next;
                free(to_free);
                server->buffer_count--;
                
                server->stats.total_stored++;
                progress = true; /* 设置标记，重新引发外层 while 循环，检查是否能解冻后续原本滞留的乱序包 */
                break;
            }

            prev = curr;
            curr = curr->next;
        }
    }
}

/* 新增功能函数：超时强制落盘机制 */
static void server_check_timeout_flush(server_t *server)
{
    uint64_t now = get_current_timestamp_ms();
    
    /* 检查队列最前面的包（最早到达服务器且未能正常因果落盘的乱序包）是否超时 */
    while (server->buffer_head && (now - server->buffer_head->arrival_time > server->config.timeout_ms)) {
        log_buffer_node_t *to_flush = server->buffer_head;
        
        /* 强行写盘 */
        server_write_entry_to_file(server, &to_flush->entry);
        
        /* 强行推进全局时钟，将其强制合入，从而打破死锁死结，让后续连续包可以正常步进 */
        vc_merge(&server->committed_vc, &to_flush->entry.vc);
        
        server->buffer_head = to_flush->next;
        free(to_flush);
        server->buffer_count--;
        server->stats.total_stored++;

        /* 强制释放一个卡住的包后，立刻触发一次标准因果刷盘，尝试连锁释放后续节点 */
        server_flush_causal_buffer(server);
    }
}

/* 新增功能函数：将单个日志实体序列化为 JSON 字符串并写盘 */
static int server_write_entry_to_file(server_t *server, const log_entry_t *entry)
{
    if (server_open_log_file(server) != 0) return -1;

    char out_buf[LOGAGG_MAX_ENTRY_SIZE];
    int len = log_entry_to_json(entry, out_buf, sizeof(out_buf));
    if (len > 0) {
        size_t written = fwrite(out_buf, 1, len, server->log_fp);
        fwrite("\n", 1, 1, server->log_fp); /* JSON Lines 换行符 */
        fflush(server->log_fp);
        return (written == (size_t)len) ? 0 : -1;
    }
    return -1;
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

    // while (server->running) {
    //     /* 接收数据 */
    //     int n = recvfrom(server->udp_sock, buffer, server->config.buffer_size - 1,
    //                      0, (struct sockaddr *)&client_addr, &client_len);

    //     if (n <= 0) {
#ifdef _WIN32
            // if (n == SOCKET_ERROR) {
            //     int err = WSAGetLastError();
            //     if (err == WSAEINTR) continue;  /* 被信号中断 */
            //     fprintf(stderr, "recvfrom error: %d\n", err);
            // }
            DWORD timeout = 200;
            setsockopt(server->udp_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
            // if (n < 0) {
            //     perror("recvfrom");
            // }
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 200000;
            setsockopt(server->udp_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    //         server->stats.total_errors++;
    //         continue;
    //     }

    //     buffer[n] = '\0';
    //     server->stats.bytes_received += n;
    //     server->stats.total_received++;

    //     /* 获取客户端信息 */
    //     char client_ip[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    //     int client_port = ntohs(client_addr.sin_port);

    //     /* 存储日志 */
    //     if (server_store_log(server, buffer, n) == 0) {
    //         /* 每 100 条打印一次状态 */
    //         if (server->stats.total_received % 100 == 0) {
    //             printf("Received %lu logs from %s:%d\n",
    //                    (unsigned long)server->stats.total_received,
    //                    client_ip, client_port);
    //         }
    //     } else {
    //         fprintf(stderr, "Failed to store log from %s:%d\n",
    //                 client_ip, client_port);
    //         server->stats.total_errors++;
    //     }
    // }

while (server->running) {
        int n = recvfrom(server->udp_sock, buffer, server->config.buffer_size - 1, 0,
                         (struct sockaddr *)&client_addr, &client_len);

        if (n > 0) {
            buffer[n] = '\0';
            server->stats.total_received++;
            server->stats.bytes_received += n;

            /* === 修改：收到数据包后不再无脑写盘，改为有序入队排序并尝试因果流水线写盘 === */
            log_entry_t entry;
            if (log_entry_from_json(&entry, buffer) == 0) {
                server_insert_to_buffer(server, &entry); /* 有序存入缓冲区 */
                server_flush_causal_buffer(server);     /* 检测并引发因果链解冻落盘 */
            } else {
                server->stats.total_errors++;
            }
        }

        /* === 新增：每次 Socket 阻塞轮询结束时，强制做一次链表包的超时检查 === */
        server_check_timeout_flush(server);

        if (server->stats.total_received % 100 == 0 && server->stats.total_received > 0) {
            server_print_stats(server);
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

    log_buffer_node_t *curr = server->buffer_head;
    while (curr) {
        log_buffer_node_t *next = curr->next;
        server_write_entry_to_file(server, &curr->entry);
        free(curr);
        curr = next;
    }
    server->buffer_head = NULL;

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
    printf("  Cached_In_Mem:   %d\n", (int)server->buffer_count);
    printf("  Total Errors:    %lu\n", (unsigned long)server->stats.total_errors);
    printf("  Bytes Received:  %lu\n", (unsigned long)server->stats.bytes_received);
    printf("========================================\n");
}