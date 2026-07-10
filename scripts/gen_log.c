/*
 * 日志生成器 - 模拟多节点并发写入带因果关系的日志
 *
 * 用法:
 *   gen_log -d <dir> -n <node_id> -c <count>
 *
 * 生成的日志模拟以下因果关系:
 *   Node-01: [A]──────[C]────────[F]
 *   Node-02: ───[B]──────[D]──
 *   Node-03: ──────[E]──────────[G]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

/* 生成时间戳字符串 */
static void get_timestamp(char *buf, size_t buflen)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

/* 写入单条日志（文件中携带向量时钟，供 Agent 解析同步） */
static void write_log(FILE *fp, const char *node_id, int vc[],
                      const char *level, const char *message)
{
    char ts[32];
    get_timestamp(ts, sizeof(ts));

    /* 文件格式: [VC:x,y,z] timestamp level  message */
    fprintf(fp, "[VC:%d,%d,%d] %s %s  %s\n", vc[0], vc[1], vc[2], ts, level, message);
    fflush(fp);

    printf("[%s] VC=[%d,%d,%d] %s %s\n",
           node_id, vc[0], vc[1], vc[2], level, message);
}

/* 打印使用说明 */
static void print_usage(const char *prog)
{
    printf("Log Generator - Simulate multi-node causal logs\n");
    printf("\nUsage:\n");
    printf("  %s -d <dir> -n <node_id> [options]\n", prog);
    printf("\nOptions:\n");
    printf("  -d <dir>      Output directory for log files\n");
    printf("  -n <node_id>  Node ID: 1, 2, or 3\n");
    printf("  -c <count>    Number of cycles (default: 3)\n");
    printf("  -i <ms>       Interval between logs (default: 500)\n");
    printf("  -h            Show this help\n");
}

int main(int argc, char *argv[])
{
    char output_dir[256] = ".";
    int node_id = 0;
    int cycles = 3;
    int interval_ms = 500;

    /* 解析参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            strncpy(output_dir, argv[++i], sizeof(output_dir) - 1);
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            node_id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cycles = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            interval_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (node_id < 1 || node_id > 3) {
        fprintf(stderr, "Error: Node ID must be 1, 2, or 3\n");
        return 1;
    }

    /* 打开日志文件 */
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/app-node%d.log", output_dir, node_id);

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create file %s\n", filepath);
        return 1;
    }

    printf("Generating logs for node-%02d -> %s\n\n", node_id, filepath);

    /* 向量时钟: [node-01, node-02, node-03] */
    int vc[3] = {0, 0, 0};

    for (int cycle = 0; cycle < cycles; cycle++) {
        printf("--- Cycle %d ---\n", cycle + 1);

        if (node_id == 1) {
            /* Node-01: [A]──────[C]────────[F] */

            /* A: 本地事件 */
            vc[0]++;
            write_log(fp, "node-01", vc, "INFO",
                      "Request received from client");

            sleep_ms(interval_ms);

            /* 发消息给 Node-02 (B 会在 Node-02 生成) */
            vc[0]++;
            write_log(fp, "node-01", vc, "INFO",
                      "Calling node-02 for user lookup");

            sleep_ms(interval_ms);

            /* C: 收到 Node-02 回复后继续 */
            vc[0]++;
            write_log(fp, "node-01", vc, "INFO",
                      "Received response from node-02");

            sleep_ms(interval_ms);

            /* 发消息给 Node-03 (E 会在 Node-03 生成) */
            vc[0]++;
            write_log(fp, "node-01", vc, "INFO",
                      "Sending result to node-03 for logging");

            sleep_ms(interval_ms);

            /* F: 完成 */
            vc[0]++;
            write_log(fp, "node-01", vc, "INFO",
                      "Request completed successfully");

        } else if (node_id == 2) {
            /* Node-02: ───[B]──────[D]── */

            sleep_ms(interval_ms * 2);  /* 等待 Node-01 的消息 */

            /* B: 收到 Node-01 消息 */
            vc[0] = 2;  /* 知道 Node-01 的状态 */
            vc[1]++;
            write_log(fp, "node-02", vc, "INFO",
                      "Processing user lookup request");

            sleep_ms(interval_ms);

            /* D: 本地处理 */
            vc[1]++;
            write_log(fp, "node-02", vc, "WARN",
                      "Cache miss, querying database");

            sleep_ms(interval_ms);

            vc[1]++;
            write_log(fp, "node-02", vc, "INFO",
                      "User found: id=12345, name=test_user");

        } else if (node_id == 3) {
            /* Node-03: ──────[E]──────────[G] */

            sleep_ms(interval_ms * 4);  /* 等待 Node-01 的消息 */

            /* E: 收到 Node-01 消息 */
            vc[0] = 4;  /* 知道 Node-01 的状态 */
            vc[2]++;
            write_log(fp, "node-03", vc, "INFO",
                      "Logging request result");

            sleep_ms(interval_ms);

            /* G: 本地处理 */
            vc[2]++;
            write_log(fp, "node-03", vc, "DEBUG",
                      "Log entry saved to database");

            sleep_ms(interval_ms);

            vc[2]++;
            write_log(fp, "node-03", vc, "INFO",
                      "Audit trail updated");
        }

        printf("\n");
        sleep_ms(interval_ms);
    }

    fclose(fp);

    printf("Done! Generated %d cycles for node-%02d\n", cycles, node_id);
    printf("Log file: %s\n", filepath);

    return 0;
}