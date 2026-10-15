#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <mswsock.h>
#pragma comment(lib, "mswsock.lib")
#include <aclapi.h>          // для GetNamedSecurityInfo
#include <sddl.h>            // для ConvertSidToStringSid
#pragma warning(disable : 4996)
#include <windows.h>
#include <wincrypt.h> // cryptoapi
#pragma comment(lib, "advapi32.lib") 

// отправить: сначала 4 байта длины, потом данные
int send_blob(SOCKET s, BYTE* data, DWORD len)
{
    if (send(s, (char*)&len, sizeof(len), 0) != sizeof(len)) return 0;  // длина
    if (send(s, (char*)data, len, 0) != (int)len) return 0;             // данные
    return 1;
}

// принять: сначала 4 байта длины, потом ровно столько данных
// возвращает malloc'нутый буфер (не забыть free), длину — через *outLen
BYTE* recv_blob(SOCKET s, DWORD* outLen)
{
    DWORD len = 0;
    if (recv(s, (char*)&len, sizeof(len), 0) != sizeof(len)) return NULL; // длина
    if (len == 0 || len > 100000) return NULL;   // защита: отсекаем мусор/переполнение
    BYTE* buf = (BYTE*)malloc(len);
    if (!buf) return NULL;

    // recv может вернуть данные ЧАСТЯМИ — читаем в цикле, пока не наберём len
    DWORD got = 0;
    while (got < len) {
        int n = recv(s, (char*)buf + got, len - got, 0);
        if (n <= 0) { free(buf); return NULL; }
        got += n;
    }
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

    HCRYPTPROV hProv = acquire_crypto_context();
    HCRYPTKEY  hKey = generate_keypair(hProv);
    DWORD pubLen; BYTE* pubBlob = export_public_key(hKey, &pubLen);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("connect failed\n"); return 1;
    }
    printf("Connected.\n");

    // ОТПРАВЛЯЕМ public-ключ серверу (шаг 2 схемы)
    if (send_blob(s, pubBlob, pubLen))
        printf("Public key sent (%lu bytes).\n", pubLen);

    // ... дальше примем зашифрованный сеансовый ключ (следующий шаг) ...

    free(pubBlob);
    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);
    closesocket(s); WSACleanup();
    return 0;
}