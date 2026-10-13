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
typedef struct CLIENT_CONTEXT
{
	SOCKET socket;
	OVERLAPPED overlapped_recv;
	OVERLAPPED overlapped_send;
	OVERLAPPED overlapped_cancel;
	char buffer_recv[512];
	char buffer_send[512];
	unsigned long bytes_received;
	unsigned long bytes_sent;
	unsigned long bytes_send_total;
	DWORD flags_WSA_recv;
} CLIENT_CONTEXT;

CLIENT_CONTEXT array_clients[MAX_CLIENTS + 1];


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

		if (GetDriveTypeA(root) != DRIVE_FIXED) continue;  // только локальные 

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


void process_request(int idx)
{
	char cmd = array_clients[idx].buffer_recv[0];   
	char* out = array_clients[idx].buffer_send;
	int out_size = sizeof(array_clients[idx].buffer_send);

	switch (cmd) {
	case '1':
		get_os_version(out, out_size);
		break;
	case '2':
		get_memory_info(out, out_size);
		break;
	case '3':
		get_time_info(out, out_size);
		break;
	case '4':
		get_disks_info(out, out_size); 
		break;
	case '5':
		get_disk_space_info(out, out_size); 
		break;
	default:
		sprintf_s(out, out_size, "Unknown command\n");
		break;
	}

	array_clients[idx].bytes_send_total = strlen(out);
	array_clients[idx].bytes_sent = 0;
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
				process_request((int)key);
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
				}
			}
		}
	}


	CloseHandle(hCompletionPort);
	closesocket(listenSocket);
	WSACleanup();
	return 0;
}