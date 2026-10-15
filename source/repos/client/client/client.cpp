#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#include <wincrypt.h> // cryptoapi
#pragma comment(lib, "advapi32.lib")  

HCRYPTPROV acquire_crypto_context()
{
    HCRYPTPROV hProv = 0;
    
    if (!CryptAcquireContextA(&hProv, NULL, MS_ENHANCED_PROV_A, PROV_RSA_FULL, 0))
    {
       
        if (!CryptAcquireContextA(&hProv, NULL, MS_ENHANCED_PROV_A, PROV_RSA_FULL, CRYPT_NEWKEYSET))
        {
            printf("CryptAcquireContext failed: %lu\n", GetLastError());
            return 0;
        }
    }
    printf("Crypto context acquired.\n");
    return hProv;
}

HCRYPTKEY generate_keypair(HCRYPTPROV hProv)
{
    HCRYPTKEY hKey = 0;
    if (!CryptGenKey(hProv, AT_KEYEXCHANGE, CRYPT_EXPORTABLE, &hKey))
    {
        printf("CryptGenKey failed: %lu\n", GetLastError());
        return 0;
    }
    printf("Key pair generated.\n");
    return hKey;
}



BYTE* export_public_key(HCRYPTKEY hKey, DWORD* outLen)
{
    DWORD blobLen = 0; // len
    if (!CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, NULL, &blobLen)) {
        printf("CryptExportKey (size) failed: %lu\n", GetLastError());
        return NULL;
    }
    BYTE* blob = (BYTE*)malloc(blobLen);
    if (!blob) return NULL;
    //key
    if (!CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, blob, &blobLen)) {
        printf("CryptExportKey (data) failed: %lu\n", GetLastError());
        free(blob);
        return NULL;
    }
    *outLen = blobLen;
    printf("Public key exported, %lu bytes.\n", blobLen);
    return blob;
}


int main()
{
    SetConsoleOutputCP(1251);
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

	HCRYPTPROV hProv = acquire_crypto_context();
    HCRYPTKEY hKey = generate_keypair(hProv);
    DWORD pubLen; 
    BYTE* pubBlob = export_public_key(hKey, &pubLen);

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


    printf("Choose request:\n 1 - OS version\n 2 - Memory\n 3 - Time\n 4 - DriverINFO\n 5 - Driver free space\n  6 - File owner\n7 - File permissions\n8 - Registry key owner\n9 - Registry key permissions\nYour choice: ");
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

    free(pubBlob);
    CryptDestroyKey(hKey);
    closesocket(s);
    WSACleanup();
    return 0;
}