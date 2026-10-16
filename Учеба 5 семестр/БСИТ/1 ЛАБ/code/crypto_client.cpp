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
    BYTE* buf = (BYTE*)malloc(len+1);
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


HCRYPTKEY import_session_key(HCRYPTPROV hProv, HCRYPTKEY hMyKey, BYTE* blob, DWORD blobLen)
{
    HCRYPTKEY hSessionKey = 0;
    // 4-й аргумент = hMyKey (наша пара с приватным ключом) → расшифрует блоб
    if (!CryptImportKey(hProv, blob, blobLen, hMyKey, 0, &hSessionKey)) {
        printf("Import session key failed: %lu\n", GetLastError());
        return 0;
    }
    printf("Session key imported (decrypted).\n");
    return hSessionKey;
}


// шифрует строку text ключом hKey, результат пишет в buf, длину — в *outLen
int encrypt_message(HCRYPTKEY hKey, const char* text, BYTE* buf, DWORD bufSize, DWORD* outLen)
{
    DWORD len = (DWORD)strlen(text);
    if (len > bufSize) return 0;
    memcpy(buf, text, len);                 // кладём открытый текст в буфер
    if (!CryptEncrypt(hKey, 0, TRUE, 0, buf, &len, bufSize)) {
        printf("CryptEncrypt failed: %lu\n", GetLastError());
        return 0;
    }
    *outLen = len;                          // len теперь = длина шифртекста
    return 1;
}


// расшифровывает buf (длины len) ключом hKey, результат — в том же buf (C-строкой)
int decrypt_message(HCRYPTKEY hKey, BYTE* buf, DWORD* len)
{
    if (!CryptDecrypt(hKey, 0, TRUE, 0, buf, len)) {
        printf("CryptDecrypt failed: %lu\n", GetLastError());
        return 0;
    }
    buf[*len] = '\0';                       // завершаем расшифрованную строку нулём
    return 1;
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

    DWORD sessLen;
    BYTE* sessBlob = recv_blob(s, &sessLen);
    if (!sessBlob) { printf("Failed to receive session key\n"); return 1; }
    printf("Received encrypted session key: %lu bytes.\n", sessLen);

    HCRYPTKEY hSessionKey = import_session_key(hProv, hKey, sessBlob, sessLen);
    free(sessBlob);

    printf("=== Secure channel established! ===\n");


    BYTE encBuf[512];
    DWORD encLen;
    encrypt_message(hSessionKey, "Hello from client!", encBuf, sizeof(encBuf), &encLen);
    send_blob(s, encBuf, encLen);
    printf("Encrypted message sent (%lu bytes).\n", encLen);

    free(pubBlob);
    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);
    closesocket(s); WSACleanup();
    return 0;
}