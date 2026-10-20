#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <mswsock.h>
#pragma comment(lib, "mswsock.lib")
#include <aclapi.h>          // для GetNamedSecurityInfo
#include <sddl.h>            // для ConvertSidToStringSid

void schedule_read(int idx);
void schedule_write(int idx);

#define MAX_CLIENTS 100
#define STAGE_HANDSHAKE 0
#define STAGE_READY     1

typedef struct CLIENT_CONTEXT
{
	SOCKET socket;
	OVERLAPPED overlapped_recv;
	OVERLAPPED overlapped_send;
	OVERLAPPED overlapped_cancel;
	char buffer_recv[512];
	char buffer_send[4096];
	unsigned long bytes_received;
	unsigned long bytes_sent;
	unsigned long bytes_send_total;
	DWORD flags_WSA_recv;

	int stage;                  // стадия: 0 = ждем public, 1 = ready (шифр. обмен)
	HCRYPTKEY hClientPublic;    
	HCRYPTKEY hSessionKey;      
} CLIENT_CONTEXT;


CLIENT_CONTEXT array_clients[MAX_CLIENTS + 1];


HCRYPTPROV hProv;   // криптопровайдер


HANDLE hCompletionPort; // IOCP 


SOCKET acceptedSocket;

void schedule_accept()
{
	SOCKET sClient = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (sClient == INVALID_SOCKET)
	{
		printf("WSASocket failed with error: %d\n", WSAGetLastError());
		return;
	}
	acceptedSocket = sClient;
	memset(&array_clients[0].overlapped_recv, 0, sizeof(OVERLAPPED));
	if (AcceptEx(array_clients[0].socket, acceptedSocket, array_clients[0].buffer_recv, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, 0, &array_clients[0].overlapped_recv) == FALSE)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			printf("AcceptEx failed with error: %d\n", err);
			closesocket(acceptedSocket);
		}
	}
}


void add_accept_connection()
{
	bool status = false;
	int idx = 0;
	for (int i = 1; i <= MAX_CLIENTS; i++)
	{
		if (array_clients[i].socket == 0)
		{
			array_clients[i].socket = acceptedSocket;
			status = true;
			idx = i;
			printf("New client connected, assigned to slot %d\n", i);
			break;
		}
	}
	if (!status)
	{
		printf("Failed to add new client connection.\n");
		closesocket(acceptedSocket);
		return;
	}
	if (CreateIoCompletionPort((HANDLE)acceptedSocket, hCompletionPort, (ULONG_PTR)idx, 0) == NULL)
	{
		printf("CreateIoCompletionPort failed with error: %d\n", GetLastError());
		closesocket(acceptedSocket);
		return;
	}

	schedule_read(idx);
}


void schedule_read(int idx)
{
	WSABUF wsaBuf;
	wsaBuf.buf = array_clients[idx].buffer_recv + array_clients[idx].bytes_received;
	wsaBuf.len = sizeof(array_clients[idx].buffer_recv) - array_clients[idx].bytes_received;


	array_clients[idx].flags_WSA_recv = 0;
	memset(&array_clients[idx].overlapped_recv, 0, sizeof(OVERLAPPED));

	if (WSARecv(array_clients[idx].socket, &wsaBuf, 1, NULL, &array_clients[idx].flags_WSA_recv, &array_clients[idx].overlapped_recv, NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			printf("WSARecv failed with error: %d\n", WSAGetLastError());
			return;
		}
	}
}


void schedule_write(int idx)
{
	WSABUF wsaBuf;
	wsaBuf.buf = array_clients[idx].buffer_send + array_clients[idx].bytes_sent;
	wsaBuf.len = array_clients[idx].bytes_send_total - array_clients[idx].bytes_sent;

	memset(&array_clients[idx].overlapped_send, 0, sizeof(OVERLAPPED));

	if (WSASend(array_clients[idx].socket, &wsaBuf, 1, NULL, 0,
		&array_clients[idx].overlapped_send, NULL) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
			printf("WSASend failed with error: %d\n", err);
	}
}

#pragma warning(disable : 4996)
void get_os_version(char* out, int out_size)
{
	OSVERSIONINFOA osvi;
	memset(&osvi, 0, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	if (!GetVersionExA(&osvi)) {
		sprintf_s(out, out_size, "Failed to get OS version\n");
		return;
	}

	// по таблице
	const char* name = "Unknown Windows";
	if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1) name = "Windows 7";
	else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2) name = "Windows 8 (or newer)";
	else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3) name = "Windows 8.1";
	else if (osvi.dwMajorVersion == 10) name = "Windows 10/11";
	else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1) name = "Windows XP";

	// разрядность:
	SYSTEM_INFO si;
	memset(&si, 0, sizeof(si));
	GetNativeSystemInfo(&si);
	const char* arch = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
		? "x64" : "x86";

	sprintf_s(out, out_size,
		"OS type: %s\r\nVersion: %lu.%lu (build %lu)\r\nArchitecture: %s\r\n",
		name, osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, arch);
}


void get_memory_info(char* out, int out_size)
{
	MEMORYSTATUSEX ms; // использует 64 битные поля для больших объёмов памяти (в отличие от globalmemorystatus)
	ms.dwLength = sizeof(ms);
	if (!GlobalMemoryStatusEx(&ms)) {
		sprintf_s(out, out_size, "Failed to get memory info\r\n");
		return;
	}
	sprintf_s(out, out_size,
		"Memory load: %lu%%\r\n"
		"Physical total: %llu MB\r\n"
		"Physical free:  %llu MB\r\n",
		ms.dwMemoryLoad,
		ms.ullTotalPhys / (1024 * 1024),
		ms.ullAvailPhys / (1024 * 1024));
}


void get_time_info(char* out, int out_size)
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	ULONGLONG ms = GetTickCount64();
	ULONGLONG sec = ms / 1000;
	ULONGLONG days = sec / 86400;
	ULONGLONG hrs = (sec % 86400) / 3600;
	ULONGLONG mins = (sec % 3600) / 60;
	ULONGLONG secs = sec % 60;

	sprintf_s(out, out_size,
		"Current time: %02d:%02d:%02d  %02d.%02d.%04d\r\n"
		"Uptime: %llu days, %llu hours, %llu min, %llu sec\r\n",
		st.wHour, st.wMinute, st.wSecond, st.wDay, st.wMonth, st.wYear,
		days, hrs, mins, secs);
}


void get_disks_info(char* out, int out_size)
{
	out[0] = '\0';
	DWORD drives = GetLogicalDrives();  // битовая маска

	for (int i = 0; i < 26; i++)
	{
		if (!((drives >> i) & 1)) continue;

		char root[4] = { (char)('A' + i), ':', '\\', '\0' };  // "C:\"

		// тип диска
		UINT type = GetDriveTypeA(root);
		const char* typeStr;
		switch (type) {
		case DRIVE_FIXED:     typeStr = "Local";     break;
		case DRIVE_REMOVABLE: typeStr = "Removable"; break;
		case DRIVE_REMOTE:    typeStr = "Network";   break;
		case DRIVE_CDROM:     typeStr = "CD-ROM";    break;
		default:              typeStr = "Unknown";   break;
		}

		// файловая система
		char fsName[16] = "";
		GetVolumeInformationA(root, NULL, 0, NULL, NULL, NULL, fsName, sizeof(fsName));

		char line[64];
		sprintf_s(line, sizeof(line), "%s  %-9s  FS: %s\r\n", root, typeStr, fsName);
		strcat_s(out, out_size, line);
	}
}


void get_disk_space_info(char* out, int out_size)
{
	out[0] = '\0';
	DWORD drives = GetLogicalDrives();

	for (int i = 0; i < 26; i++)
	{
		if (!((drives >> i) & 1)) continue;

		char root[4] = { (char)('A' + i), ':', '\\', '\0' };

		UINT type = GetDriveTypeA(root);
		if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) continue; 

		ULARGE_INTEGER freeBytes, totalBytes;
		if (GetDiskFreeSpaceExA(root, &freeBytes, &totalBytes, NULL))
		{
			char line[80];
			sprintf_s(line, sizeof(line), "%s  free: %llu MB / total: %llu MB\r\n",
				root,
				freeBytes.QuadPart / (1024 * 1024),
				totalBytes.QuadPart / (1024 * 1024));
			strcat_s(out, out_size, line);
		}
	}
}


void get_owner_info(const char* path, SE_OBJECT_TYPE objType,  char* out, int out_size)
{
	PSID pOwnerSid = NULL; 
	PSECURITY_DESCRIPTOR pSD = NULL; 


	DWORD res = GetNamedSecurityInfoA(
		path,                          
		objType,               // тип объекта
		OWNER_SECURITY_INFORMATION, // OWNER     
		&pOwnerSid,                     
		NULL, NULL, NULL,               
		&pSD);                          

	if (res != ERROR_SUCCESS) {
		sprintf_s(out, out_size, "Failed to get owner (error %lu)\r\n", res);
		return;
	}

	
	char name[256] = "", domain[256] = "";
	DWORD nameLen = sizeof(name), domainLen = sizeof(domain);
	SID_NAME_USE sidType;
	BOOL ok = LookupAccountSidA(NULL, pOwnerSid,
		name, &nameLen,
		domain, &domainLen, &sidType);

	
	char* sidString = NULL;
	ConvertSidToStringSidA(pOwnerSid, &sidString);


	if (ok)
		sprintf_s(out, out_size, "Owner: %s\\%s\r\nSID: %s\r\n",
			domain, name, sidString ? sidString : "?");
	else
		sprintf_s(out, out_size, "Owner name unknown\r\nSID: %s\r\n",
			sidString ? sidString : "?");

	if (sidString) LocalFree(sidString);
	if (pSD) LocalFree(pSD);
}


void get_permissions_info(const char* path, SE_OBJECT_TYPE objType,  char* out, int out_size)
{
	PACL pDacl = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	out[0] = '\0';


	DWORD res = GetNamedSecurityInfoA(path, objType,
		DACL_SECURITY_INFORMATION, //DACL 
		NULL, NULL, &pDacl, NULL, &pSD);
	if (res != ERROR_SUCCESS) {
		sprintf_s(out, out_size, "Failed to get DACL (error %lu)\r\n", res);
		return;
	}
	if (pDacl == NULL) {
		sprintf_s(out, out_size, "No DACL (access unrestricted)\r\n");
		if (pSD) LocalFree(pSD);
		return;
	}

	//ACL count
	ACL_SIZE_INFORMATION aclSize;
	GetAclInformation(pDacl, &aclSize, sizeof(aclSize), AclSizeInformation);

	char line[400];
	sprintf_s(line, sizeof(line), "DACL has %lu entries:\r\n", aclSize.AceCount);
	strcat_s(out, out_size, line);

	
	for (DWORD i = 0; i < aclSize.AceCount; i++)
	{
		
		ACCESS_ALLOWED_ACE* pAce = NULL;
		if (!GetAce(pDacl, i, (void**)&pAce)) continue; // ACE

		PSID pSid = (PSID)&pAce->SidStart; // take SID 

		
		char name[256] = "", domain[256] = ""; // sid to char
		DWORD nameLen = sizeof(name), domainLen = sizeof(domain);
		SID_NAME_USE st;
		LookupAccountSidA(NULL, pSid, name, &nameLen, domain, &domainLen, &st);

		// тип ACE
		const char* aceType =
			(pAce->Header.AceType == ACCESS_ALLOWED_ACE_TYPE) ? "ALLOW" :
			(pAce->Header.AceType == ACCESS_DENIED_ACE_TYPE) ? "DENY" : "OTHER";

		// access mask
		ACCESS_MASK mask = pAce->Mask;
		char rights[200] = "";
		if (mask & GENERIC_ALL)      strcat_s(rights, sizeof(rights), "ALL ");
		if (mask & GENERIC_READ)     strcat_s(rights, sizeof(rights), "READ ");
		if (mask & GENERIC_WRITE)    strcat_s(rights, sizeof(rights), "WRITE ");
		if (mask & GENERIC_EXECUTE)  strcat_s(rights, sizeof(rights), "EXECUTE ");
		if (mask & FILE_READ_DATA)   strcat_s(rights, sizeof(rights), "READ_DATA ");
		if (mask & FILE_WRITE_DATA)  strcat_s(rights, sizeof(rights), "WRITE_DATA ");
		if (mask & DELETE)           strcat_s(rights, sizeof(rights), "DELETE ");
		if (rights[0] == '\0') sprintf_s(rights, sizeof(rights), "mask=0x%lX", mask);

		sprintf_s(line, sizeof(line), "  [%lu] %s %s\\%s : %s\r\n",
			i, aceType, domain, name, rights);
		strcat_s(out, out_size, line);
	}

	if (pSD) LocalFree(pSD);
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


HCRYPTKEY import_public_key(HCRYPTPROV hProv, BYTE* blob, DWORD blobLen)
{
	HCRYPTKEY hKey = 0;
	if (!CryptImportKey(hProv, blob, blobLen, 0, 0, &hKey)) {
		printf("CryptImportKey failed: %lu\n", GetLastError());
		return 0;
	}
	printf("Client public key imported.\n");
	return hKey;
}


HCRYPTKEY generate_session_key(HCRYPTPROV hProv)
{
	HCRYPTKEY hKey = 0;
	if (!CryptGenKey(hProv, CALG_RC4, CRYPT_EXPORTABLE, &hKey)) {
		printf("Session key gen failed: %lu\n", GetLastError());
		return 0;
	}
	printf("Session key generated.\n");
	return hKey;
}


BYTE* export_session_key(HCRYPTKEY hSessionKey, HCRYPTKEY hClientPublic, DWORD* outLen)
{
	DWORD len = 0;
	// 1-й вызов — размер
	if (!CryptExportKey(hSessionKey, hClientPublic, SIMPLEBLOB, 0, NULL, &len)) {
		printf("Export session (size) failed: %lu\n", GetLastError());
		return NULL;
	}
	BYTE* blob = (BYTE*)malloc(len);
	if (!blob) return NULL;
	// 2-й вызов — данные
	if (!CryptExportKey(hSessionKey, hClientPublic, SIMPLEBLOB, 0, blob, &len)) {
		printf("Export session (data) failed: %lu\n", GetLastError());
		free(blob);
		return NULL;
	}
	*outLen = len;
	printf("Session key exported (encrypted), %lu bytes.\n", len);
	return blob;
}


// шифрует строку 
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

// собирает ответ по команде в out, НИЧЕГО не отправляет
void build_response(char cmd, const char* arg, char* out, int out_size)
{
	switch (cmd) {
	case '1': get_os_version(out, out_size); break;
	case '2': get_memory_info(out, out_size); break;
	case '3': get_time_info(out, out_size); break;
	case '4': get_disks_info(out, out_size); break;
	case '5': get_disk_space_info(out, out_size); break;
	case '6': get_owner_info(arg, SE_FILE_OBJECT, out, out_size); break;
	case '7': get_permissions_info(arg, SE_FILE_OBJECT, out, out_size); break;
	case '8': get_owner_info(arg, SE_REGISTRY_KEY, out, out_size); break;
	case '9': get_permissions_info(arg, SE_REGISTRY_KEY, out, out_size); break;
	default:  sprintf_s(out, out_size, "Unknown command\r\n"); break;
	}
}


void handle_request(int idx)
{
	CLIENT_CONTEXT* c = &array_clients[idx];

	// длину, потом тело
	if (c->bytes_received < sizeof(DWORD)) { schedule_read(idx); return; }
	DWORD blobLen;
	memcpy(&blobLen, c->buffer_recv, sizeof(DWORD));
	if (blobLen > 400) {   
		printf("Bad request length from client %d\n", idx);
		closesocket(c->socket); memset(c, 0, sizeof(*c)); return;
	}
	if (c->bytes_received < sizeof(DWORD) + blobLen) { schedule_read(idx); return; }

	// расшифровка
	BYTE encRequest[512];
	memcpy(encRequest, c->buffer_recv + sizeof(DWORD), blobLen);
	DWORD reqLen = blobLen;
	decrypt_message(c->hSessionKey, encRequest, &reqLen);
	

	
	encRequest[reqLen] = '\0';           // завершаем строку (reqLen — длина расшифрованного)
	char cmd = encRequest[0];

	// ищем аргумент: пропускаем "цифру" и пробел
	char* arg = (char*)encRequest + 1;   // после цифры
	while (*arg == ' ') arg++;           // пропускаем пробел(ы)
	

	char plainResponse[4096];
	build_response(cmd, arg, plainResponse, sizeof(plainResponse));
	

	//  шифруем 
	BYTE encResponse[4096];
	DWORD encRespLen;
	encrypt_message(c->hSessionKey, plainResponse, encResponse, sizeof(encResponse), &encRespLen);

	
	memcpy(c->buffer_send, &encRespLen, sizeof(DWORD));
	memcpy(c->buffer_send + sizeof(DWORD), encResponse, encRespLen);
	c->bytes_send_total = sizeof(DWORD) + encRespLen;
	c->bytes_sent = 0;
	c->bytes_received = 0;   

	schedule_write(idx);
}


void handle_handshake(int idx) // обработка рукопожатия
{
	CLIENT_CONTEXT* c = &array_clients[idx];

	// ждём, пока придёт хотя бы длина (4 байта)
	if (c->bytes_received < sizeof(DWORD)) { schedule_read(idx); return; }

	
	DWORD blobLen;
	memcpy(&blobLen, c->buffer_recv, sizeof(DWORD));

	// защита
	if (blobLen > 1000) {
		printf("Bad public key length from client %d\n", idx);
		closesocket(c->socket);
		memset(c, 0, sizeof(*c));
		return;
	}

	// ждем остальное
	if (c->bytes_received < sizeof(DWORD) + blobLen) { schedule_read(idx); return; }

	
	BYTE* pubBlob = (BYTE*)(c->buffer_recv + sizeof(DWORD));   // копируем данные

	c->hClientPublic = import_public_key(hProv, pubBlob, blobLen);
	c->hSessionKey = generate_session_key(hProv);

	DWORD sessLen;
	BYTE* sessBlob = export_session_key(c->hSessionKey, c->hClientPublic, &sessLen);

	// формируем ответ
	memcpy(c->buffer_send, &sessLen, sizeof(DWORD));
	memcpy(c->buffer_send + sizeof(DWORD), sessBlob, sessLen);
	c->bytes_send_total = sizeof(DWORD) + sessLen;
	c->bytes_sent = 0;
	free(sessBlob);

	c->bytes_received = 0;        
	c->stage = STAGE_READY;       
	printf("Handshake done with client %d\n", idx);

	schedule_write(idx);          
}



int main()
{
	WSAData wsaData;
	int WSAStartupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (WSAStartupResult != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAStartupResult);
		return 1;
	}
	printf("WSAStartup succeeded.\n");


	hProv = acquire_crypto_context();
	if (hProv == 0) { /* ошибка, выход */ }


	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("WSASocket failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}
	printf("WSASocket succeeded.\n");
	array_clients[0].socket = listenSocket;


	sockaddr_in serverAddress;
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(9000);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}
	printf("bind succeeded.\n");


	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}
	printf("listen succeeded.\n");


	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hCompletionPort == NULL)
	{
		printf("CreateIoCompletionPort failed with error: %d\n", GetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	if (CreateIoCompletionPort((HANDLE)listenSocket, hCompletionPort, 0, 0) == NULL)
	{
		printf("CreateIoCompletionPort failed with error: %d\n", GetLastError());
		CloseHandle(hCompletionPort);
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}
	printf("CreateIoCompletionPort succeeded and listensocket is bound to it\n");


	DWORD bytesTransferred;
	ULONG_PTR key;
	OVERLAPPED* lpOverlapped;
	schedule_accept();
	while (TRUE)
	{
		bool status = GetQueuedCompletionStatus(hCompletionPort, &bytesTransferred, &key, &lpOverlapped, INFINITE);
		if (lpOverlapped == NULL)
		{
			continue;
		}
		printf("event: key=%llu, bytes=%lu\n", (unsigned long long)key, bytesTransferred);
		if (key == 0)
		{
			add_accept_connection();
			schedule_accept();
		}
		else
		{
			if (lpOverlapped == &array_clients[key].overlapped_recv) // если это событие чтения
			{
				if (bytesTransferred == 0) // клиент закрыл соединение
				{
					printf("Client %llu disconnected.\n", (unsigned long long)key);
					closesocket(array_clients[key].socket);
					memset(&array_clients[key], 0, sizeof(array_clients[key]));
					continue;
				}
				array_clients[key].bytes_received += bytesTransferred;
				if (array_clients[key].stage == STAGE_HANDSHAKE) {
					handle_handshake((int)key);
				}
				else {
					handle_request((int)key);
				}
			}
			else if (lpOverlapped == &array_clients[key].overlapped_send) // если это событие записи
			{
				array_clients[key].bytes_sent += bytesTransferred;
				if (array_clients[key].bytes_sent < array_clients[key].bytes_send_total)
				{
					schedule_write(key);
				}
				else
				{
					printf("response sent to client %llu\n", (unsigned long long)key);
					schedule_read((int)key);  
				}
			}
		}
	}


	CloseHandle(hCompletionPort);
	closesocket(listenSocket);
	WSACleanup();
	return 0;
}