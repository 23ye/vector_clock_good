/*
 * 简单的 UDP 接收器 - 用于测试 Agent 发送
 * 接收并打印 Agent 发送的日志
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#define BUFFER_SIZE 4096
#define DEFAULT_PORT 9999

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    /* 创建 UDP socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
#else
    if (sock < 0) {
#endif
        fprintf(stderr, "Failed to create socket\n");
        return 1;
    }

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to bind port %d\n", port);
        return 1;
    }

    printf("========================================\n");
    printf("  UDP Receiver listening on port %d\n", port);
    printf("========================================\n");
    printf("\nWaiting for Agent to send logs...\n");
    printf("(Run: build\\agent.exe -n node-01 -f test.log -s 127.0.0.1:%d)\n\n", port);

    /* 接收循环 */
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    int count = 0;

    while (1) {
        int n = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                         (struct sockaddr *)&client_addr, &client_len);

        if (n > 0) {
            buffer[n] = '\0';
            count++;

            /* 获取客户端地址 */
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            int client_port = ntohs(client_addr.sin_port);

            printf("[%d] From %s:%d\n", count, client_ip, client_port);
            printf("    %s\n\n", buffer);
        }
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    return 0;
}