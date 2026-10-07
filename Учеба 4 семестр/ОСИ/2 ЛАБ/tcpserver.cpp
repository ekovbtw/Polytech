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

#define MAX_CLIENTS 256 // maximalnoe count clients
int count_clients = 0;


typedef struct client_info // client structure
{
	unsigned int port; 
	unsigned int ip; 
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


void close_client(pollfd* pfd_info, client_info* info)
{
	s_close(pfd_info->fd); // закрытие сокета
	pfd_info->fd = -1; // отчистка
	pfd_info->events = 0;
	pfd_info->revents = 0;
	printf("Client disconnected: %u.%u.%u.%u: %d\n", (info->ip >> 24) & 0xff, (info->ip >> 16) & 0xff, (info->ip >> 8) & 0xff, (info->ip) & 0xff, info->port);
	memset(info, 0, sizeof(client_info)); // отчистка
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


	// poll, настройка
	struct pollfd pfd[MAX_CLIENTS];
	for (int i = 0; i<MAX_CLIENTS-1; i++)
	{
		pfd[i].fd = -1;
		pfd[i].events = 0;
	}
	pfd[MAX_CLIENTS-1].fd = s;
	pfd[MAX_CLIENTS-1].events = POLLIN;


	client_info array[MAX_CLIENTS];


	while (1)
	{
		int ev_cnt = poll(pfd, sizeof(pfd) / sizeof(pfd[0]), 1000); // каждую секунду проверка события
		if (ev_cnt>0)
		{
			if (pfd[MAX_CLIENTS-1].revents & POLLIN) // если пришел клиент
			{
				socklen_t socklen = sizeof(addr);
				int socket_client = accept(s, (struct sockaddr*) &addr, &socklen); // берем сокет клиента
				
				int status = 1; // сокет хороший
				if (socket_client == -1)
				{
					status = 0; // сокета нет в любом случае
					if (errno != EAGAIN && errno != EWOULDBLOCK)
					{
						sock_err("accept", s);
					}
				}


				if (status == 1)
				{
					// ставим неблокирующий режим у сокета клиента
					status_set_non_block = set_non_block_mode(socket_client);
					if (status_set_non_block == -1)
					{
						printf("Error set non block mode\n");
						s_close(socket_client);
					}

					if (status_set_non_block != -1)
					{
						// ищем свободную структуру
						int k = 0;
						int free_pfd_status = 0; 
						while (k<MAX_CLIENTS-1)
						{
							if (pfd[k].fd == -1) // нашли
							{
								pfd[k].fd = socket_client; // присвоили
								free_pfd_status = 1;
								unsigned int ip_socket_client = ntohl(addr.sin_addr.s_addr); //  парсинг айпи от сокета
								array[k].ip = ip_socket_client;
							
								array[k].i = k; // запоминаем номер сокета
								unsigned int port_socket_client = ntohs(addr.sin_port); //  парсинг port от сокета
								array[k].port = port_socket_client;

								pfd[k].events = POLLIN; // ставим флаг, чтобы poll искал сокеты, которые вызвали событие чтения 

								printf(" New client connected: %u.%u.%u.%u: %d\n", (array[k].ip >> 24) & 0xff, (array[k].ip >> 16) & 0xff, (array[k].ip >> 8) & 0xff, (array[k].ip) & 0xff, array[k].port);
								break;
							}
							k++;
						}
						if(free_pfd_status == 0) // не нашли
						{
							s_close(socket_client);
						}
					}
					
				}
			}
			for (int j = 0; j<MAX_CLIENTS-1; j++)
			{
				if (pfd[j].fd == -1) continue; // пропускаем если сокет == -1 
				if (pfd[j].revents == 0) continue; // пропускаем если нет обратных событий

				
				if (pfd[j].revents > 0) // если есть события
				{
					if ((pfd[j].revents & POLLERR) || (pfd[j].revents & POLLHUP) || (pfd[j].revents & POLLNVAL))
					{
						close_client(&pfd[j], &array[j]);
					}
					else if (pfd[j].revents & POLLIN)
					{
						unsigned char buffer[512] = {0};
					
						int status = recv(pfd[j].fd, buffer, 512, 0);
						if (status == 0)
						{
							close_client(&pfd[j], &array[j]);
						}
						else if (status == -1)
						{
							if (errno != EAGAIN && errno != EWOULDBLOCK)
							{
								sock_err("recv", pfd[j].fd);
								close_client(&pfd[j], &array[j]);
							}
						}
						else if (status > 0)
						{
							printf("%d bytes from: %u.%u.%u.%u: %d\n", status, (array[j].ip >> 24) & 0xff, (array[j].ip >> 16) & 0xff, (array[j].ip >> 8) & 0xff, (array[j].ip) & 0xff, array[j].port);
						}
					}
				}
			}
		}
		
		
	}
}
