#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <mswsock.h>
#pragma comment(lib, "mswsock.lib")

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

CLIENT_CONTEXT array_clients[MAX_CLIENTS+1];


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
	if(CreateIoCompletionPort((HANDLE)acceptedSocket, hCompletionPort, (ULONG_PTR)idx, 0) == NULL)
	{
		printf("CreateIoCompletionPort failed with error: %d\n", GetLastError());
		closesocket(acceptedSocket);
		return;
	}

	//schedule_read(i);
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

	if(CreateIoCompletionPort((HANDLE)listenSocket, hCompletionPort, 0, 0) == NULL)
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
	}


	CloseHandle(hCompletionPort);
	closesocket(listenSocket);
	WSACleanup();
	return 0;
}