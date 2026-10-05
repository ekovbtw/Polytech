// LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>

#define MAX_CLIENTS 256; // maximalnoe count clients
int count_clients = 0;


typedef struct pollfd // poll func structure
{ 
	int fd; /* описатель сокета */ 
	short events; /* запрошенные события mask byte*/ 
	short revents; /* возвращенные события */
	int i;
} pollfd;

typedef struct client_info // client structure
{
	int port; 
	int id; 
	int i;
} client_info;


int check_args(int argc, char* argv[]) // проверка аргументов командной строки
{
	if (argc != 2)
	{
		printf("Uncorrect arguments %s\n", argv[0]);
		return 0;
	}
	if (atoi(argv[1]) < 1 || atoi(argv[1]) > 65535) // 65535 граница
	{
	    printf("Uncorrect port %s\n", argv[1]);
		return 0;
	}
	return 1;
}

int init()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSAStartup перед началом использования сокетов
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data)); // (2.2) - версия Winsock, wsa_data - структура, которая будет заполнена информацией о версии
#else
	return 1; // Для других ОС действий не требуется
#endif
}
void deinit()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSACleanup в конце работы
	WSACleanup();
#else
	// Для других ОС действий не требуется
#endif
}


int sock_err(const char* function, int s) // функция для вывода ошибки сокета, принимает имя функции и сокет
{
	int err;
#ifdef _WIN32
	err = WSAGetLastError(); // возвращает код последней ошибки сокета
#else
	err = errno;
#endif
	fprintf(stdout, "%s: socket error: %d\n", function, err);
	return -1;
}


void s_close(int s) // функция для закрытия сокета, принимает сокет
{
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
}

int set_non_block_mode(int s) // установка неблокирующего режима
{
    int fl = fcntl(s, F_GETFL, 0); // читаем флаги
    if (fl == -1)
    {
        return -1;
    }
    return fcntl(s, F_SETFL, fl | O_NONBLOCK); // добавляем флаг через побитовое или
}


int poll(struct pollfd *ufds, unsigned int nfds, int timeout)
{
	
}


int main(int argc, char* argv[])
{
    // работа с аргументами
	if (!check_args(argc, argv)) // проверка аргументов поданных на вход
	{
		return 1;
	}
	int port = atoi(argv[1]);

	// инициализация сокета
	int s;
	struct sockaddr_in addr;
	init();
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
	    printf("socket not created\n");
		return 1;
	}
	// обнуление и добавление описания
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port); // Сервер прослушивает порт
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // Все адреса

	// включение SO_REUSEADDR
	const int enable = 1; // флаг включить
	// SOL_SOCKET - уровень опции
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1)
	{
	    printf("WARN, SO_REUSEADDR doesn't enable\n");
	}


	// переводим сокет в неблокирующий режим
	int status_set_non_block = set_non_block_mode(s);
	if (status_set_non_block == -1)
	{
	    printf("Error set non block mode\n");
		s_close(s);
		return 1;
	}


	// привязка сокета к локальным адресам пк, порта. (связывание сокета и адреса прослушивания)
	if(bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0)
	{
        sock_err("bind", s);
        s_close(s);
        return 1;
	}


	// Начало прослушивания
	if (listen(s, SOMAXCONN) < 0) // SOMAXCONN - системный максимум
	{
	    sock_err("listen", s);
		s_close(s);
		return 1;
	}


	while(1)sleep(100);
}
