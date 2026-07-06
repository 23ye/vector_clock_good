#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOTAL_LOGS 1000000

static const char *levels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
static const char *messages[] = {
    "Request received from client",
    "Calling node-02 for user lookup",
    "Received response from node-02",
    "Sending result to node-03 for logging",
    "Request completed successfully"
};

int main(int argc, char *argv[]) {

    const char *output_dir = "./testlogs";

    if (argc > 1) {
        output_dir = argv[1];
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/testlogs.jsonl", output_dir);

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        perror("open file failed");
        return 1;
    }

    printf("Generating logs to: %s\n", filepath);

    srand((unsigned)time(NULL));

    int vc0 = 0, vc1 = 0, vc2 = 0;

    for (int i = 0; i < TOTAL_LOGS; i++) {

        // 模拟 vector clock 递增（node-0主增长）
        vc0++;

        // 少量模拟其他节点变化
        if (rand() % 10 == 0) vc1++;
        if (rand() % 15 == 0) vc2++;

        const char *level = levels[rand() % 4];
        const char *msg = messages[rand() % 5];

        long long timestamp = (long long)time(NULL) * 1000 + i;

        fprintf(fp,
            "{\"node_id\":\"node-01\","
            "\"vector_clock\":{\"node-0\":%d,\"node-1\":%d,\"node-2\":%d},"
            "\"timestamp\":%lld,"
            "\"level\":\"%s\","
            "\"message\":\"%s\"}\n",
            vc0, vc1, vc2,
            timestamp,
            level,
            msg
        );

        // 可选：进度输出
        if (i % 100000 == 0) {
            printf("generated %d logs...\n", i);
        }
    }

    fclose(fp);

    printf("Done: logs.jsonl generated (1,000,000 lines)\n");
    return 0;
}