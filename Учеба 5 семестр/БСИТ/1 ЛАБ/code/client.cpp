#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
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


int recv_exact(SOCKET s, char* buf, int len)
{
    int got = 0;
    while (got < len) {
        int n = recv(s, buf + got, len - got, 0);
        if (n <= 0) return 0;   
        got += n;
    }
    return 1;
}


BYTE* recv_blob(SOCKET s, DWORD* outLen)
{
    DWORD len = 0;
    if (!recv_exact(s, (char*)&len, sizeof(len))) return NULL;   // длина — через цикл
    if (len == 0 || len > 100000) return NULL;
    BYTE* buf = (BYTE*)malloc(len + 1);
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
    
    if (!CryptImportKey(hProv, blob, blobLen, hMyKey, 0, &hSessionKey)) {
        printf("Import session key failed: %lu\n", GetLastError());
        return 0;
    }
    printf("Session key imported (decrypted).\n");
    return hSessionKey;
}


// шифрует 
int encrypt_message(HCRYPTKEY hKey, const char* text, BYTE* buf, DWORD bufSize, DWORD* outLen)
{
    DWORD len = (DWORD)strlen(text);
    if (len > bufSize) return 0;
    memcpy(buf, text, len);                
    if (!CryptEncrypt(hKey, 0, TRUE, 0, buf, &len, bufSize)) {
        printf("CryptEncrypt failed: %lu\n", GetLastError());
        return 0;
    }
    *outLen = len;                          
    return 1;
}


// расшифровывает
int decrypt_message(HCRYPTKEY hKey, BYTE* buf, DWORD* len)
{
    if (!CryptDecrypt(hKey, 0, TRUE, 0, buf, len)) {
        printf("CryptDecrypt failed: %lu\n", GetLastError());
        return 0;
    }
    buf[*len] = '\0';                      
    return 1;
}

int main()
{
    SetConsoleOutputCP(1251);
    WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);

    // --- крипто-подготовка ---
    HCRYPTPROV hProv = acquire_crypto_context();
    if (hProv == 0) return 1;
    HCRYPTKEY hKey = generate_keypair(hProv);
    DWORD pubLen; BYTE* pubBlob = export_public_key(hKey, &pubLen);

    // --- адрес и подключение ---
    char serverIp[64];
    printf("Enter server IP (e.g. 127.0.0.1): ");
    scanf("%63s", serverIp);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = inet_addr(serverIp);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("connect failed: %d\n", WSAGetLastError());
        return 1;
    }
    printf("Connected.\n");

    // --- рукопожатие (уже работает у вас) ---
    send_blob(s, pubBlob, pubLen);
    DWORD sessLen;
    BYTE* sessBlob = recv_blob(s, &sessLen);
    if (!sessBlob) { printf("No session key\n"); return 1; }
    HCRYPTKEY hSessionKey = import_session_key(hProv, hKey, sessBlob, sessLen);
    free(sessBlob);
    printf("=== Secure channel established! ===\n");

    while (1)
    {
        printf("\nChoose request (0 to exit):\n"
            " 1 - OS version\n 2 - Memory\n 3 - Time\n"
            " 4 - Disks\n 5 - Disk space\n"
            " 6 - File owner\n 7 - File permissions\n"
            " 8 - Registry owner\n 9 - Registry permissions\n"
            "Your choice: ");
        char cmd[256];
        scanf(" %255[^\n]", cmd);

        if (cmd[0] == '0') break;   // выход из цикла

        // шифруем и отправляем
        BYTE encReq[512]; DWORD encReqLen;
        encrypt_message(hSessionKey, cmd, encReq, sizeof(encReq), &encReqLen);
        send_blob(s, encReq, encReqLen);

        // принимаем и расшифровываем ответ
        DWORD encRespLen;
        BYTE* encResp = recv_blob(s, &encRespLen);
        if (!encResp) { printf("Connection lost\n"); break; }
        DWORD respLen = encRespLen;
        decrypt_message(hSessionKey, encResp, &respLen);
        printf("\n--- Server response ---\n%.*s\n", (int)respLen, (char*)encResp);
        free(encResp);
    }

    // --- очистка ---
    free(pubBlob);
    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);
    closesocket(s); WSACleanup();
    return 0;
}