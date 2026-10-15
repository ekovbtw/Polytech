#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <mswsock.h>
#pragma comment(lib, "mswsock.lib")
#include <aclapi.h>          // для GetNamedSecurityInfo
#include <sddl.h>            // для ConvertSidToStringSid
#pragma warning(disable : 4996)

// отправить: сначала 4 байта длины, потом данные
int send_blob(SOCKET s, BYTE* data, DWORD len)
{
    if (send(s, (char*)&len, sizeof(len), 0) != sizeof(len)) return 0;  // длина
    if (send(s, (char*)data, len, 0) != (int)len) return 0;             // данные
    return 1;
}


int recv_exact(SOCKET s, char* buf, int len)
{
    int got = 0;
    while (got < len) {
        int n = recv(s, buf + got, len - got, 0);
        if (n <= 0) return 0;   // разрыв или ошибка
        got += n;
    }
    return 1;
}


BYTE* recv_blob(SOCKET s, DWORD* outLen)
{
    DWORD len = 0;
    if (!recv_exact(s, (char*)&len, sizeof(len))) return NULL;   // длина — через цикл
    if (len == 0 || len > 100000) return NULL;
    BYTE* buf = (BYTE*)malloc(len);
    if (!buf) return NULL;
    if (!recv_exact(s, (char*)buf, len)) { free(buf); return NULL; }  // данные — тем же циклом
    *outLen = len;
    return buf;
}


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
    WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);

    HCRYPTPROV hProv = acquire_crypto_context();   // та же функция, что у клиента

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(listenSock, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 1);
    printf("Server listening on 9000...\n");

    SOCKET client = accept(listenSock, NULL, NULL);   // синхронно ждём одного клиента
    printf("Client connected.\n");

    // ПРИНИМАЕМ public-ключ клиента (шаг 3 схемы)
    DWORD pubLen; BYTE* pubBlob = recv_blob(client, &pubLen);
    if (pubBlob)
        printf("Received public key: %lu bytes.\n", pubLen);

    // ... дальше: импорт ключа, генерация сессионного, отправка (следующие шаги) ...

    free(pubBlob);
    CryptReleaseContext(hProv, 0);
    closesocket(client); closesocket(listenSock); WSACleanup();
    return 0;
}