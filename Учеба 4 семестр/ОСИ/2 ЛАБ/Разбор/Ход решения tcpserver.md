Сервер нужно написать под Linux. 
## ШАГ 1
Сначала инициализация, проверка аргументов, обнуление структуры и добавление ее описания.

```
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



int main(int argc, char* argv[])
{
    // работа с аргументами
	if (!check_args(argc, argv)) // проверка аргументов поданных на вход
	{
		return 1;
	}
	int port = atoi(argv[1]);


	int s;
	struct sockaddr_in addr;
	init();
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	// Заполнение адреса прослушивания
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port); // Сервер прослушивает порт
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // Все адреса

}

```

## Шаг 2
В ОС Linux после того как сервер закончил свое соединение, примерно минуту TCP слушает запоздалые пакеты, те пакеты, которые не успели прийти во время. Следовательно, каждый раз перезапуская сервер нужно будет ждать около минуты до полной его остановки. Чтобы этого избежать добавим в main опцию сокета. 

```
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
		return sock_err("socket", s);
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
	
	
	return 0;
}

```

## Шаг 3
В современных операционных системах предусмотрен ряд механизмов для обеспечения серверами параллельного обслуживания нескольких клиентов без использования механизмов многопоточности — неблокирующий режим работы сокетов. В этом режиме функции connect, accept, send, recv, sendto, recvfrom и некоторые другие не блокируют управление до наступления соответствующего события, а возвращают значение немедленно. При этом, если данные от клиента еще не были получены, функции возвращают соответствующую ошибку. Блокирующие сокеты блокировали бы выполнение программы.
У каждого дескриптора есть набор флагов, хранящихся как биты в одном числе. Один из этих битов, O_NONBLOCK, как раз и включает неблокирующий режим.
Работа с флагами идёт через функцию fcntl, и делается в два действия:
1. Прочитать текущие флаги (команда F_GETFL).
2. Добавить к ним бит O_NONBLOCK через побитовое ИЛИ и записать обратно (команда F_SETFL).

```
int set_non_block_mode(int s) // установка неблокирующего режима
{
    int fl = fcntl(s, F_GETFL, 0); // читаем флаги
    if (fl == -1)
    {
        return -1;
    }
    return fcntl(s, F_SETFL, fl | O_NONBLOCK); // добавляем флаг через побитовое или
}
```

Определим в main после установки опций: 

```
// переводим сокет в неблокирующий режим
	int status_set_non_block = set_non_block_mode(s);
	if (status_set_non_block == -1)
	{
	    printf("Error set non block mode\n");
		s_close(s);
		return -1;
	}
```

## Шаг 4
Чтобы созданный TCP-сокет имел возможность принимать входящие подключения необходимо "привязать" его к определенным адресам локального компьютера, а также указать номер прослушивающего порта. Эти действия выполняет функция bind. 
Функция возвращает 0 в случае успешного завершения, отрицательное число в противном случае. Для TCP-клиентов вызов функции bind не требуется т.к. привязка сокета к адресу производится функцией connect.

```
if(bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0)
	{
        sock_err("bind", s);
        s_close(s);
        return 1;
	}
```

## Шаг 5
После успешной привязки сокет должен начать "прослушивание", для этого должна быть вызвана функция listen. Она переводит сокет в слушающее состояние, 
**backlog** — размер очереди соединений, например если указано 1, то сервер одновременно может принять только одно соединение, все другие попытки подключения клиентов к серверу будут отменены. Это относится только к клиентам, желающим подключиться к серверу в настоящий момент, но не относится к уже подключившимся клиентам.

```
// Начало прослушивания
	if (listen(s, SOMAXCONN) < 0) // SOMAXCONN - системный максимум
	{
	    sock_err("listen", s);
		s_close(s);
		return 1;
	}
```

## Шаг 6
Чтобы программа не перебирала все открытые сокеты, пытаясь из каждого получить данные или отправить их, предусмотрены функции и механизмы поиска сокетов, для которых вызов функций чтения или отправки данных имеет смысл в данный момент. Например, с помощью этих механизмов можно отобрать только те сокеты, которые содержат готовые для чтения данные, пришедшие от клиентов. С помощью этих же механизмов можно отобрать те прослушивающие TCP-сокеты, к которым подключились клиенты. 
Универсальным механизмов отбора таких сокетов является select и poll (WSAPool для Windows).
Проще говоря, poll это функция, которая будит программу, когда есть сокеты готовые обмениваться прям сейчас, функция периодически опрашивает состояние задачи, проверяя можно ли с ней работать в данный момент. 
На вход подается массив структур, в котором описан сокет и какое событие необходимо. Также в функции poll есть таймер, по истечении которого программа поднимается и заполняет поля структур событиями, которые произошли. 
Механизм poll (WSAPoll для Windows) с точки зрения вызывающей программы во многом напоминает select. Отличие состоит в способе упаковки дескрипторов сокетов в соответствующую структуру данных. Дескриптор каждого сокета, события которого интересуют программу, сохраняется в структуре struct pollfd, определенной следующим образом: 

```
struct pollfd
{ int fd; /* описатель сокета */ 
short events; /* запрошенные события */ 
short revents; /* возвращенные события */ 
};
```

Поле events представляет собой битовую маску: каждое интересующее событие заносится в виде бита в это поле. Поле revents заполняется функцией poll, в случае, если какое-либо из запрашиваемых событий возникло. Поддерживаемые события определены следующими константами:

```
#define POLLIN ... /* Можно считывать данные */
#define POLLOUT ... /* Можно отравлять (запись не будет блокирована) */
#define POLLERR ... /* Произошла ошибка */
#define POLLHUP ... /* Соединение разорвано удаленной стороной ("Положили трубку") */
```

Функция poll определена следующим образом: 
`int poll(struct pollfd *ufds, unsigned int nfds, int timeout);`
параметр ufds — массив структур (по одному экземпляру на сокет), nfds — количество элементов в массиве ufds, timeout — время ожидания событий на всех переданных сокетах (в миллисекундах).

Сначала добавим следующее:

```
#define MAX_CLIENTS 256 // maximalnoe count clients
int count_clients = 0;


typedef struct client_info // client structure
{
	unsigned int port; 
	unsigned int ip; 
	int i;
} client_info;

```
Это нужно для сохранения IP, port каждого полученного клиента. 

Затем настроим (обнулим) структуру poll и client_info (в main после listen): 

```
// poll, настройка
	struct pollfd pfd[MAX_CLIENTS];
	for (int i = 0; i<MAX_CLIENTS-1; i++)
	{
		pfd[i].fd = -1;
		pfd[i].events = 0;
	}
	pfd[MAX_CLIENTS-1].fd = s; // последний элемент - прослушивающий сокет
	pfd[MAX_CLIENTS-1].events = POLLIN; // ожидаем сигнала "можно считывать данные"


	client_info array[MAX_CLIENTS];
```

Добавим цикл, который будет искать активные сокеты через poll: 

```
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
		}
		
		
	}
```

## Шаг 7
В этом шаге необходимо сделать отключение клиента от сервера, если клиент больше ничего не сообщает. Нужно закрыть сокет и освободить слот, чтобы можно было записать новый сокет. 
Сначала определим, что сокет, который завершил свою отправку данных подает сигнал, а значит для сервера это выглядит как событие чтения (POLLIN). Если после этого события вызвать recv, то recv вернет 0 - что означает завершение. 
Поставим каждому сокету в поле events флаг POLLIN, чтобы poll следил за включениями этого флага у сокетов (в цикле while) . 

```
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
```

Теперь напишем функцию закрытия сокета и очищения структуры pollfd и client_info по индексу. 

```
void close_client(pollfd* pfd_info, client_info* info)
{
	s_close(pfd_info->fd);
	pfd_info->fd = -1;
	pfd_info->events = 0;
	pfd_info->revents = 0;
	printf("Client disconnected: %u.%u.%u.%u: %d\n", (info->ip >> 24) & 0xff, (info->ip >> 16) & 0xff, (info->ip >> 8) & 0xff, (info->ip) & 0xff, info->port);
	memset(info, 0, sizeof(client_info));
}

```
 
 Начнем писать цикл, в котором будут обрабатываться revents, сначала напишем для ошибок (пишем в блоке ev_cnt > 0, но после обработки слушающего сокета):

```
for (int j = 0; j<MAX_CLIENTS-1; j++)
			{
				if (pfd[j].fd == -1) continue;
				if (pfd[j].revents == 0) continue;


				if (pfd[j].revents > 0)
				{
					if ((pfd[j].revents & POLLERR) || (pfd[j].revents & POLLHUP) || (pfd[j].revents & POLLNVAL))
					{
						close_client(&pfd[j], &array[j]);
					}
				}
			}
```
- **POLLERR:** на сокете ошибка (например, клиент упал и соединение сброшено).
- **POLLHUP:** соединение закрыто.
- **POLLNVAL:** дескриптор недействителен. Обычно означает ошибку в твоём коде: сокет закрыт, а в pfd остался его номер.

Дальше добавим в этот цикл обработку POLLIN сокета

```
for (int j = 0; j<MAX_CLIENTS-1; j++)
			{
				if (pfd[j].fd == -1) continue; // пропускаем если сокет == -1 
				if (pfd[j].revents == 0) continue; // пропускаем если нет обратных событий

				
				if (pfd[j].revents > 0) // если есть события
				{
					if ((pfd[j].revents & POLLERR) || (pfd[j].revents & POLLHUP) || (pfd[j].revents & POLLNVAL)) // если ошибки
					{
						close_client(&pfd[j], &array[j]);
					}
					else if (pfd[j].revents & POLLIN) // если чтение
					{
						unsigned char buffer[512] = {0}; // буфер для сообщения 
					
						int status = recv(pfd[j].fd, buffer, 512, 0); // получаем данные
						if (status == 0) // если статус = закрытое соединение
						{
							close_client(&pfd[j], &array[j]);
						}
						else if (status == -1) // если ошибка recv
						{
							if (errno != EAGAIN && errno != EWOULDBLOCK)
							{
								sock_err("recv", pfd[j].fd);
								close_client(&pfd[j], &array[j]);
							}
						}
						else if (status > 0) // если пришли байты 
						{
							printf("%d bytes from: %u.%u.%u.%u: %d\n", status, (array[j].ip >> 24) & 0xff, (array[j].ip >> 16) & 0xff, (array[j].ip >> 8) & 0xff, (array[j].ip) & 0xff, array[j].port);
						}
					}
				}
			}
```

## Шаг 8
В этом шаге будем создавать буфер сообщения, которое передает клиент серверу. Для начала добавим в описание структуры client_info сам буфер и остальные переменные

```
typedef struct client_info // client structure
{
	unsigned int port; 
	unsigned int ip; 
	int i;
	unsigned char* buffer; // буффер для накопления сообщения
	int bytes_cnt; // количество байт, которые уже пришли
	int status; // статус: put - 1, else - 0
	int capacity; 

} client_info;
```
