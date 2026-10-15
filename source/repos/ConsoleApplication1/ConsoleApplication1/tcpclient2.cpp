// tcpclient2: получение всех сообщений с сервера по команде get
// запуск: tcpclient2 IP:Port get FILENAME
#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else // LINUX (только для отладки, основная ОС - Windows)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#define HEADER_SIZE 15 // заголовок сообщения: 4 (номер) + 4 (дата) + 4 (дата) + 3 (время)
#define START_CAPACITY 1024 // начальный размер буфера приёма
#define RECV_CHUNK 4096 // сколько байт читаем за один recv


int init()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSAStartup перед началом использования сокетов
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
	return 1; // Для других ОС действий не требуется
#endif
}

void deinit()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSACleanup в конце работы
	WSACleanup();
#endif
}

int sock_err(const char* function, int s) // вывод ошибки сокета
{
	int err;
#ifdef _WIN32
	err = WSAGetLastError();
#else
	err = errno;
#endif
	fprintf(stdout, "%s: socket error: %d\n", function, err);
	return -1;
}

void s_close(int s) // закрытие сокета
{
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
}

void sleep_ms(int ms) // пауза между попытками подключения
{
#ifdef _WIN32
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}


// проверка аргументов: tcpclient2 IP:Port get FILENAME
bool check_args(int argc, char* argv[])
{
	if (argc != 4)
	{
		printf("Usage: %s IP:Port get FILENAME\n", argv[0]);
		return false;
	}
	if (strcmp(argv[2], "get") != 0) // второй аргумент - именно команда get
	{
		printf("Unknown command %s\n", argv[2]);
		return false;
	}
	return true;
}


// разбор строки "IP:порт", ip и port возвращаем через указатели
bool parse_ip_and_port(char* ip_port_str, char* ip, int* port)
{
	if (sscanf(ip_port_str, "%15[^:]:%d", ip, port) != 2)
	{
		printf("Uncorrect address %s\n", ip_port_str);
		return false;
	}
	int len = strlen(ip);
	if (len > 15 || len < 7) // от "1.1.1.1" до "255.255.255.255"
	{
		printf("Uncorrect ip %s\n", ip);
		return false;
	}
	if (*port < 1 || *port > 65535)
	{
		printf("Uncorrect port %d\n", *port);
		return false;
	}
	return true;
}


// подключение к серверу: до 10 попыток с паузой 100 мс
// возвращает сокет или -1
int connect_to_server(struct sockaddr_in* addr)
{
	int counter_connect = 0;
	while (counter_connect < 10)
	{
		int s = socket(AF_INET, SOCK_STREAM, 0); // один сокет = одна попытка
		if (s < 0)
		{
			sock_err("socket", s);
			return -1;
		}
		if (connect(s, (struct sockaddr*)addr, sizeof(*addr)) == 0)
		{
			return s; // удача
		}
		counter_connect++;
		s_close(s); // закрываем неудачный сокет, чтобы создать новый
		sleep_ms(100);
	}
	sock_err("connect", -1);
	return -1;
}


// отправка команды get (3 байта), send в цикле на случай частичной отправки
int send_get(int s)
{
	char cmd[3] = { 'g', 'e', 't' };
	int size = sizeof(cmd);
	int sent = 0;
#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif
	while (sent < size)
	{
		int res = send(s, cmd + sent, size - sent, flags);
		if (res < 0)
		{
			return sock_err("send", s);
		}
		sent += res;
	}
	return 0;
}


// поиск конца сообщения ('\0' после заголовка)
// возвращает длину полного сообщения или 0, если сообщение пришло не целиком
int find_len_message(unsigned char* buffer, int bytes_cnt)
{
	for (int i = HEADER_SIZE; i < bytes_cnt; i++)
	{
		if (buffer[i] == '\0')
		{
			return i + 1;
		}
	}
	return 0;
}


// разбор одного сообщения из начала буфера и запись в файл
// в файл: "IP:порт сервера" пробел и сообщение в исходном виде, номер не пишется
void write_message(unsigned char* msg, FILE* out, char* ip, int port)
{
	unsigned int number_buf;
	unsigned short year_buf;

	memcpy(&number_buf, msg, 4);
	unsigned int number = ntohl(number_buf); // номер служебный, в файл не пишется
	(void)number;

	unsigned char day1 = msg[4];
	unsigned char month1 = msg[5];
	memcpy(&year_buf, msg + 6, 2);
	unsigned short year1 = ntohs(year_buf);

	unsigned char day2 = msg[8];
	unsigned char month2 = msg[9];
	memcpy(&year_buf, msg + 10, 2);
	unsigned short year2 = ntohs(year_buf);

	unsigned char hour = msg[12];
	unsigned char minute = msg[13];
	unsigned char second = msg[14];

	fprintf(out, "%s:%d %02d.%02d.%04d %02d.%02d.%04d %02d:%02d:%02d %s\n",
		ip, port,
		day1, month1, year1,
		day2, month2, year2,
		hour, minute, second,
		(char*)(msg + HEADER_SIZE));
}


int main(int argc, char* argv[])
{
	setlocale(LC_ALL, "Russian");

	if (!check_args(argc, argv))
	{
		return 1;
	}

	char ip[16] = { 0 };
	int port = 0;
	if (!parse_ip_and_port(argv[1], ip, &port))
	{
		return 1;
	}

	if (!init())
	{
		printf("Error initializing sockets\n");
		return 1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	addr.sin_addr.s_addr = inet_addr(ip); // inet_addr есть и в старых Winsock (VS 2010)
	if (addr.sin_addr.s_addr == INADDR_NONE)
	{
		printf("Error ip address %s\n", ip);
		deinit();
		return 1;
	}

	int s = connect_to_server(&addr);
	if (s < 0)
	{
		deinit();
		return 1;
	}

	if (send_get(s) != 0)
	{
		printf("Error send get\n");
		s_close(s);
		deinit();
		return 1;
	}

	// файл открываем только после успешного подключения, чтобы не создавать пустой файл зря
	FILE* out = fopen(argv[3], "w");
	if (out == NULL)
	{
		printf("Error opening file %s\n", argv[3]);
		s_close(s);
		deinit();
		return 1;
	}

	// приём: сообщения идут подряд, recv может вернуть часть сообщения или несколько сразу,
	// поэтому копим байты в буфере и вынимаем из него полные сообщения
	int capacity = START_CAPACITY;
	int bytes_cnt = 0; // сколько байт сейчас в буфере
	unsigned char* buffer = (unsigned char*)malloc(capacity);
	if (buffer == NULL)
	{
		printf("Memory allocation failed\n");
		fclose(out);
		s_close(s);
		deinit();
		return 1;
	}

	int count = 0; // сколько сообщений сохранено
	int status = 0; // 0 - сервер закрыл соединение (норма), 1 - ошибка
	while (1)
	{
		if (capacity - bytes_cnt < RECV_CHUNK) // места мало - увеличиваем буфер в 2 раза
		{
			unsigned char* tmp = (unsigned char*)realloc(buffer, capacity * 2);
			if (tmp == NULL)
			{
				printf("Memory allocation failed\n");
				status = 1;
				break;
			}
			buffer = tmp;
			capacity *= 2;
		}

		int res = recv(s, (char*)buffer + bytes_cnt, capacity - bytes_cnt, 0);
		if (res == 0) // сервер передал всё и отключил клиента
		{
			break;
		}
		if (res < 0)
		{
			sock_err("recv", s);
			status = 1;
			break;
		}
		bytes_cnt += res;

		int len;
		while ((len = find_len_message(buffer, bytes_cnt)) != 0) // пока в буфере есть полные сообщения
		{
			write_message(buffer, out, ip, port);
			count++;
			memmove(buffer, buffer + len, bytes_cnt - len); // сдвигаем остаток в начало
			bytes_cnt -= len;
		}
	}

	if (status == 0 && bytes_cnt != 0) // соединение закрыто посреди сообщения
	{
		printf("Connection closed, %d bytes of incomplete message lost\n", bytes_cnt);
	}

	printf("%d messages received\n", count);

	free(buffer);
	fclose(out);
	s_close(s);
	deinit();
	return status;
}