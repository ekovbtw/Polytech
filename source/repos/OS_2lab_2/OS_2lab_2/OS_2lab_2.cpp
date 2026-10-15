#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PORTS 100 
#define MAX_CLIENTS 256 
#define DATAGRAM_SIZE 65536 // максимальный размер дейтаграммы UDP
#define CLIENT_TIMEOUT_MS 30000 // клиент удаляется из базы, если молчит 30 секунд

typedef struct client_info
{
	int used; // 1 - запись занята, 0 - свободна
	unsigned int ip;
	unsigned int port;

	unsigned int* numbers; // хранение сообщений
	int numbers_count; // сколько
	int capacity; 

	unsigned int last_numbers[20]; // последние принятые номера
	int cnt_last_numbers; // сколько последних номеров хранится

	DWORD last_time; 
} client_info;


int find_add_client(client_info* info, unsigned int ip, unsigned int port) // поиск клиента по ip и port
{
	int i = 0;
	while (i < MAX_CLIENTS)
	{
		if (info[i].used && info[i].ip == ip && info[i].port == port)
		{
			return i;
		}
		i++;
	}
	if (i == MAX_CLIENTS)
	{
		i = 0;
		while (i < MAX_CLIENTS)
		{
			if (!info[i].used)
			{
				info[i].used = 1;
				info[i].ip = ip;
				info[i].port = port;
				info[i].numbers = NULL;
				info[i].numbers_count = 0;
				info[i].capacity = 0;
				info[i].cnt_last_numbers = 0;
				info[i].last_time = GetTickCount();
				return i;
			}
			i++;
		}
	}
	return -1; // новый клиент не добавлен и не найден 
}

int check_duplicate(client_info* info, unsigned int number) // поиск дубликата
{
	for (int i = 0; i < info->numbers_count; i++)
	{
		if (info->numbers[i] == number)
		{
			return 1; 
		}
	}
	return 0; 
}

void add_number_to_numbers(client_info* info, unsigned int number) // добавление номера в массив
{
	if (info->capacity == 0)
	{
		info->capacity = 20;
		info->numbers = (unsigned int*)malloc(info->capacity * sizeof(unsigned int));
		if (info->numbers == NULL)
		{
			printf("Memory allocation failed\n");
			exit(1);
		}
	}
	else if (info->numbers_count >= info->capacity)
	{
		info->capacity *= 2;
		unsigned int* new_numbers = (unsigned int*)realloc(info->numbers, info->capacity * sizeof(unsigned int));
		if (new_numbers == NULL)
		{
			printf("Memory reallocation failed\n");
			exit(1);
		}
		info->numbers = new_numbers;
	}
	info->numbers[info->numbers_count] = number;
	info->numbers_count++;
}

void add_number_to_last_numbers(client_info* info, unsigned int number) // добавление номера в массив последних номеров
{
	if (info->cnt_last_numbers < 20)
	{
		info->last_numbers[info->cnt_last_numbers] = number;
		info->cnt_last_numbers++;
	}
	else
	{
		for (int i = 1; i < 20; i++)
		{
			info->last_numbers[i - 1] = info->last_numbers[i];
		}
		info->last_numbers[19] = number;
	}
}

int build_response(client_info* info, unsigned char* response) // собираем ответ клиенту
{
	int pointer = 0;
	int i = 0;
	while (i < info->cnt_last_numbers && pointer + 4 <= 80)
	{
		unsigned int number = htonl(info->last_numbers[i]);
		memcpy(response + pointer, &number, sizeof(unsigned int));
		i++;
		pointer += sizeof(int);
	}
	return pointer;
}

void remove_client(client_info* info) // удаление клиента
{
	printf("Client removed: %u.%u.%u.%u:%u\n", (info->ip >> 24) & 0xff, (info->ip >> 16) & 0xff, (info->ip >> 8) & 0xff, (info->ip) & 0xff, info->port);
	free(info->numbers);
	memset(info, 0, sizeof(client_info)); 
}

void remove_old_clients(client_info* array) // удаление клиентов, молчащих больше 30 секунд
{
	DWORD now = GetTickCount(); // текущее время, один раз на проход
	int i = 0;
	while (i < MAX_CLIENTS)
	{
		if (array[i].used && now - array[i].last_time > CLIENT_TIMEOUT_MS)
		{
			remove_client(&array[i]);
		}
		i++;
	}
}


int init()
{
	
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data)); // (2.2) - версия Winsock
}

void deinit()
{
	
	WSACleanup();
}

int sock_err(const char* function, SOCKET s) // функция для вывода ошибки сокета, принимает имя функции и сокет
{
	int err = WSAGetLastError(); // возвращает код последней ошибки сокета
	fprintf(stdout, "%s: socket error: %d\n", function, err);
	return -1;
}

void s_close(SOCKET s) // функция для закрытия сокета
{
	closesocket(s);
}

int set_non_block_mode(SOCKET s) // установка неблокирующего режима
{
	unsigned long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
}


// проверка аргументов: udpserver первый_порт последний_порт
int check_args(int argc, char* argv[], int* first, int* last)
{
	if (argc != 3)
	{
		printf("Usage: %s first_port last_port\n", argv[0]);
		return 0;
	}
	*first = atoi(argv[1]);
	*last = atoi(argv[2]);
	if (*first < 1 || *first > 65535 || *last < 1 || *last > 65535)
	{
		printf("Incorrect port\n");
		return 0;
	}
	if (*first > *last)
	{
		printf("First port must be <= last port\n");
		return 0;
	}
	if (*last - *first + 1 > MAX_PORTS)
	{
		printf("Too many ports (max %d)\n", MAX_PORTS);
		return 0;
	}
	return 1;
}


// создание UDP-сокета, привязка к порту и перевод в неблокирующий режим
SOCKET create_udp_socket(int port)
{
	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET)
	{
		sock_err("socket", s);
		return INVALID_SOCKET;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // все адреса

	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		sock_err("bind", s);
		s_close(s);
		return INVALID_SOCKET;
	}

	if (set_non_block_mode(s) != 0)
	{
		sock_err("ioctlsocket", s);
		s_close(s);
		return INVALID_SOCKET;
	}

	return s;
}


// номер сообщения из первых 4 байт дейтаграммы (сетевой порядок)
unsigned int get_message_number(unsigned char* data)
{
	unsigned int buff;
	memcpy(&buff, data, 4);
	return ntohl(buff);
}


// проверка, что дейтаграмма похожа на сообщение протокола:
int check_datagram(unsigned char* data, int len)
{
	if (len < 16)
	{
		return 0;
	}
	for (int i = 15; i < len; i++)
	{
		if (data[i] == '\0') return 1;
	}
	return 0;
}


// разбор сообщения из дейтаграммы и запись в файл
int parse_message(unsigned char* data, unsigned int ip, unsigned int port, FILE* f)
{
	unsigned short year_buf;
	int pointer = 4; // первые 4 байта - номер, в файл не пишется

	unsigned char day1 = data[pointer]; pointer += 1;
	unsigned char month1 = data[pointer]; pointer += 1;
	memcpy(&year_buf, data + pointer, 2); pointer += 2;
	unsigned short year1 = ntohs(year_buf);

	unsigned char day2 = data[pointer]; pointer += 1;
	unsigned char month2 = data[pointer]; pointer += 1;
	memcpy(&year_buf, data + pointer, 2); pointer += 2;
	unsigned short year2 = ntohs(year_buf);

	unsigned char hour = data[pointer]; pointer += 1;
	unsigned char minute = data[pointer]; pointer += 1;
	unsigned char second = data[pointer]; pointer += 1;

	fprintf(f, "%u.%u.%u.%u:%u %02d.%02d.%04d %02d.%02d.%04d %02d:%02d:%02d %s\n",
		(ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, (ip) & 0xff,
		port,
		day1, month1, year1,
		day2, month2, year2,
		hour, minute, second,
		(char*)(data + pointer));
	fflush(f); 

	if (strcmp((char*)(data + pointer), "stop") == 0)
	{
		return 1;
	}
	return 0;
}


int handle_datagram(SOCKET s, unsigned char* data, int len, struct sockaddr_in* from_addr, client_info* array, FILE* f)
{
	if (check_datagram(data, len) == 0) // проверка формата дейтаграммы
	{
		printf("Failed check_datagram\n");
		return 0;
	}


	unsigned int ip = ntohl(from_addr->sin_addr.s_addr);
	unsigned int port = ntohs(from_addr->sin_port);

	int status_find = find_add_client(array, ip, port);
	if (status_find == -1)
	{
		printf("Client database is full\n");
		return 0;
	}

	// последняя активность
	array[status_find].last_time = GetTickCount(); 
	int status_parse = 0;
	unsigned int number = get_message_number(data);
	int status_duplicate = check_duplicate(&array[status_find], number);
	if (status_duplicate == 0) // новый номер
	{
		add_number_to_numbers(&array[status_find], number);

		status_parse = parse_message(data, ip, port, f);
		
		add_number_to_last_numbers(&array[status_find], number);
	}
	unsigned char response[80];
	int response_size = build_response(&array[status_find], response);
	int status = sendto(s, (char*)response, response_size, 0, (struct sockaddr*)from_addr, sizeof(struct sockaddr_in));
	if (status == SOCKET_ERROR)
	{
		sock_err("sendto", s); 
		//s_close(s);
	}
	if (status_parse == 1) // пришёл stop
	{
		return 1;
	}
	return 0; 
}


int main(int argc, char* argv[])
{
	int first_port, last_port;
	if (!check_args(argc, argv, &first_port, &last_port))
	{
		return 1;
	}
	int ports_cnt = last_port - first_port + 1;

	if (!init())
	{
		printf("WSAStartup error\n");
		return 1;
	}

	// создание сокетов на все порты диапазона
	SOCKET socks[MAX_PORTS];
	for (int i = 0; i < ports_cnt; i++)
	{
		socks[i] = create_udp_socket(first_port + i);
		if (socks[i] == INVALID_SOCKET)
		{
			printf("Cannot open UDP port %d\n", first_port + i);
			for (int k = 0; k < i; k++) // закрываем уже открытые
			{
				s_close(socks[k]);
			}
			deinit();
			return 1;
		}
	}
	printf("Listening UDP ports %d-%d\n", first_port, last_port);

	// открытие файла для записи сообщений (каждый запуск с чистого файла)
	FILE* f = fopen("msg.txt", "w");
	if (f == NULL)
	{
		printf("Error open msg.txt\n");
		for (int i = 0; i < ports_cnt; i++) s_close(socks[i]);
		deinit();
		return 1;
	}

	
	client_info clients[MAX_CLIENTS];
	memset(clients, 0, sizeof(clients));

	
	unsigned char datagram[DATAGRAM_SIZE];

	// event
	WSAEVENT ev = WSACreateEvent();
	int status_event = 1; // 1 - событие создано и все сокеты привязаны, 0 - ошибка
	if (ev == WSA_INVALID_EVENT)
	{
		sock_err("WSACreateEvent", 0);
		status_event = 0;
	}

	// FDREAD
	int k = 0;
	while (status_event == 1 && k < ports_cnt) 
	{
		if (WSAEventSelect(socks[k], ev, FD_READ) != 0)
		{
			sock_err("WSAEventSelect", socks[k]);
			status_event = 0;
			break;
		}
		k++;
	}

	if (status_event == 0) // общая очистка при любой ошибке
	{
		for (int j = 0; j < ports_cnt; j++) s_close(socks[j]);
		if (ev != WSA_INVALID_EVENT) WSACloseEvent(ev); 
		fclose(f);
		deinit();
		return 1;
	}

	int server_running = 1;
	while (server_running == 1)
	{
		
		DWORD dw = WSAWaitForMultipleEvents(1, &ev, FALSE, 1000, FALSE);
		if (dw == WSA_WAIT_FAILED)
		{
			sock_err("WSAWaitForMultipleEvents", 0);
			break;
		}

		
		WSAResetEvent(ev);

		int cnt = 0;
		while (server_running == 1 && cnt < ports_cnt)
		{
			WSANETWORKEVENTS ne;
			if (WSAEnumNetworkEvents(socks[cnt], NULL, &ne) != 0)
			{
				sock_err("WSAEnumNetworkEvents", socks[cnt]);
			}
			else if (ne.lNetworkEvents & FD_READ)
			{
				int status_read = 1; // 1 - на сокете ещё могут быть дейтаграммы
				while (status_read == 1 && server_running == 1)
				{
					struct sockaddr_in from_addr;
					int addrlen = sizeof(from_addr); // каждый раз заново
					int rcv = recvfrom(socks[cnt], (char*)datagram, DATAGRAM_SIZE, 0, (struct sockaddr*)&from_addr, &addrlen);
					if (rcv >= 0)
					{
						if (handle_datagram(socks[cnt], datagram, rcv, &from_addr, clients, f) == 1)
						{
							printf("Stop received\n");
							server_running = 0;
						}
					}
					else
					{
						int err = WSAGetLastError();
						if (err == WSAEWOULDBLOCK)
						{
							status_read = 0; // дейтаграммы на этом сокете кончились
						}
						else if (err == WSAECONNRESET)
						{
							
						}
						else
						{
							sock_err("recvfrom", socks[cnt]);
							status_read = 0;
						}
					}
				} 
			} 
			cnt++; 
		}

		remove_old_clients(clients);
	}
	

	int c = 0;
	while (c < MAX_CLIENTS)
	{
		if (clients[c].used)
		{
			free(clients[c].numbers); 
		}
		c++;
	}

	for (int j = 0; j < ports_cnt; j++)
	{
		s_close(socks[j]);
	}

	WSACloseEvent(ev);

	fclose(f);

	deinit();
	printf("Server stopped\n");
	return 0;

}