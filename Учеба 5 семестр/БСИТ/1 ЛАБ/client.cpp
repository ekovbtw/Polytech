#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

int main()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    char serverIp[64];
    printf("Enter server IP (e.g. 127.0.0.1): ");
    scanf("%63s", serverIp);


    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) { printf("socket error\n"); return 1; }


    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = inet_addr(serverIp);   // строка в IP

   
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("connect failed: %d\n", WSAGetLastError());
        closesocket(s); WSACleanup(); return 1;
    }
    printf("Connected.\n");

    
    printf("Choose request:\n 1 - OS version\n 2 - Memory\n 3 - Time\n 4 - DriverINFO\n 5 - Driver free space\nYour choice: ");
    char cmd[8];
    scanf("%7s", cmd);

    
    char request[16];
    sprintf(request, "%s\n", cmd);
    send(s, request, (int)strlen(request), 0);

    
    char response[512];
    int n = recv(s, response, sizeof(response) - 1, 0);
    if (n > 0) {
        response[n] = '\0';
        printf("\n--- Server response ---\n%s\n", response);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}