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

Дальше обозначим `#define START_CAPACITY 512 // стартовая емкость для буфера клиента`

Дальше дополним функцию закрытия клиента, добавим очищение массива buffer до memset

```
void close_client(pollfd* pfd_info, client_info* info) // закрытие клиента, если больше не общается
{
	s_close(pfd_info->fd); // закрытие сокета
	pfd_info->fd = -1; // отчистка
	pfd_info->events = 0;
	pfd_info->revents = 0;
	printf("Client disconnected: %u.%u.%u.%u: %d\n", (info->ip >> 24) & 0xff, (info->ip >> 16) & 0xff, (info->ip >> 8) & 0xff, (info->ip) & 0xff, info->port);
	free(info->buffer);
	memset(info, 0, sizeof(client_info)); // отчистка
}
```

Написал новую функцию для добавления памяти в buffer при нехватке. Каждый проход при нехватке памяти удваивает на 2.

```
int add_buffer_mem (client_info* info)
{
	int new_capacity = info->capacity;
	if (info->capacity == 0)
	{ 
		new_capacity = START_CAPACITY;
	}
	unsigned char* tmp = (unsigned char*) realloc(info->buffer, new_capacity*2);
	if (tmp == NULL)
	{
		return -1; // памяти нет, старый буфер остался на месте
	}
	info->buffer = tmp;
	info->capacity = new_capacity*2;
	return 1;
}
```

Обновим цикл проверки флагов, добавим буфер и проверку на put: 

```
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
					else if (pfd[j].revents & POLLIN) // если чтение
					{
						if (array[j].capacity - array[j].bytes_cnt < START_CAPACITY) // если вместимость текущая меньше 512 
						{
							int status = add_buffer_mem (&array[j]); // расширяем буфер
							if (status == -1)
							{
								close_client(&pfd[j], &array[j]);
								continue; // если неудача выделения памяти 
							}
						}
						
						int status = recv(pfd[j].fd, array[j].buffer+array[j].bytes_cnt, array[j].capacity - array[j].bytes_cnt, 0);
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
							array[j].bytes_cnt += status; // прибавляем полученные байты
							if (array[j].bytes_cnt >= 3 && array[j].status == 0) // если байтов больше 3 и статуса put еще нет
							{
								if (array[j].buffer[0] == 'p' && array[j].buffer[1] == 'u' && array[j].buffer[2] == 't') // проверка
								{
									array[j].status = 1;
									memmove(array[j].buffer, array[j].buffer+3, array[j].bytes_cnt-3); // сдвигаем, чтобы очистить от put
									array[j].bytes_cnt = array[j].bytes_cnt-3;
									printf("PUT complete\n");
								}
								else // иначе отключаем
								{
									printf("Unknown command\n");
									close_client(&pfd[j], &array[j]);
									continue;
								}
							}
							printf("Bytes_cnt now = %d\n", array[j].bytes_cnt);
							printf("%d bytes from: %u.%u.%u.%u: %d\n", status, (array[j].ip >> 24) & 0xff, (array[j].ip >> 16) & 0xff, (array[j].ip >> 8) & 0xff, (array[j].ip) & 0xff, array[j].port);
						}
					}
				}
			}
```

## Шаг 9
В этом шаге нужно разобрать сообщение из буфера. 
Напоминание: 

![[Pasted image 20261007184509.png]]

Конец сообщения будем определять по нулевому байту, если в буфере меньше 15 байт, значит сообщение не дошло и нужно ждать. Если нашли нулевой знак на позиции point, то длина сообщения point+1 (так как отсчет с нуля байт). 
Добавим функцию поиска длины сообщения в buffer 

```
int find_len_message (client_info *info)
{
	for (int i = 15; i<=info->bytes_cnt-1; i++) // начинаем с 15 байт (тк заголовок не считаем, ищем 0)
	{
		if (info->buffer[i] == '\0') return i+1;
	}
	return 0;
}
```

Добавим вызов функции в блок status>0 после recv 

```
else if (status > 0)
						{
							array[j].bytes_cnt += status; // прибавляем полученные байты
							if (array[j].bytes_cnt >= 3 && array[j].status == 0) // если байтов больше 3 и статуса put еще нет
							{
								if (array[j].buffer[0] == 'p' && array[j].buffer[1] == 'u' && array[j].buffer[2] == 't') // проверка
								{
									array[j].status = 1;
									memmove(array[j].buffer, array[j].buffer+3, array[j].bytes_cnt-3); // сдвигаем, чтобы очистить от put
									array[j].bytes_cnt = array[j].bytes_cnt-3;
									printf("PUT complete\n");
								}
								else // иначе отключаем
								{
									printf("Unknown command\n");
									close_client(&pfd[j], &array[j]);
									continue;
								}
							}
							int status_message; // длина сообщения
							while (array[j].status == 1  && (status_message = find_len_message(&array[j])) != 0) // пока находятся сообщения и есть put
							{
								printf("LEN_MESSAGE = %d\n", status_message); // печать длины
								memmove(array[j].buffer, array[j].buffer+status_message, array[j].bytes_cnt-status_message); // сдвиг на длину сообщения
								array[j].bytes_cnt -= status_message;
							}
							printf("Bytes_cnt now = %d\n", array[j].bytes_cnt);
							printf("%d bytes from: %u.%u.%u.%u: %d\n", status, (array[j].ip >> 24) & 0xff, (array[j].ip >> 16) & 0xff, (array[j].ip >> 8) & 0xff, (array[j].ip) & 0xff, array[j].port);
						}
```

Напишем функцию, которая будет парсить сообщение переданное клиентом: 

```
void parse_message (client_info *info, int len)
{
	unsigned int buff; // логика такая же как в клиенте, делаем буферную переменную, преобразовываем из сетевого порядка байт, перезаписываем в нормальную переменную
	int pointer = 0; 
	memcpy(&buff, info->buffer+pointer , 4);
	pointer+=sizeof(unsigned int);

	unsigned int number = ntohl(buff);

	unsigned char day1_buf;
	unsigned char month1_buf;
	unsigned short year1_buf;
	unsigned char day2_buf;
	unsigned char month2_buf;
	unsigned short year2_buf;
	unsigned char hour_buf;
	unsigned char minute_buf;
	unsigned char second_buf;

	memcpy(&day1_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char day1 = day1_buf;
	memcpy(&month1_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char month1 = month1_buf;
	memcpy(&year1_buf, info->buffer+pointer, 2);
	pointer += sizeof(unsigned short);
	unsigned short year1 = ntohs(year1_buf);

	memcpy(&day2_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char day2 = day2_buf;
	memcpy(&month2_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char month2 = month2_buf;
	memcpy(&year2_buf, info->buffer+pointer, 2);
	pointer += sizeof(unsigned short);
	unsigned short year2 = ntohs(year2_buf);
	
	memcpy(&hour_buf, info->buffer+pointer, 1);
	pointer += sizeof(unsigned char);
	unsigned char hour = hour_buf;
	memcpy(&minute_buf, info->buffer+pointer, 1);
	pointer += sizeof(unsigned char);
	unsigned char minute = minute_buf;
	memcpy(&second_buf, info->buffer+pointer, 1);
	pointer += sizeof(unsigned char);
	unsigned char second = second_buf;

	//message парсить не нужно, достатоно просто вывести буфер с позиции поинтер
	printf("#%u: %02d.%02d.%04d %02d.%02d.%04d %02d:%02d:%02d %s\n",number, day1, month1, year1, day2, month2, year2, hour, minute, second, (char*)(info->buffer + pointer));
	
}
```

Добавим в цикл while вызов функции parse_message 

```
while (array[j].status == 1  && (status_message = find_len_message(&array[j])) != 0) // пока находятся сообщения и есть put
							{
								printf("LEN_MESSAGE = %d\n", status_message); // печать длины
								parse_message(&array[j], status_message);
								memmove(array[j].buffer, array[j].buffer+status_message, array[j].bytes_cnt-status_message); // сдвиг на длину сообщения
								array[j].bytes_cnt -= status_message;
							}
```

## Шаг 10
На этом шаге напишем вывод в файл: 

```
void parse_message (client_info *info, int len, FILE* f)
{
	unsigned int buff; // логика такая же как в клиенте, делаем буферную переменную, преобразовываем из сетевого порядка байт, перезаписываем в нормальную переменную
	int pointer = 0; 
	memcpy(&buff, info->buffer+pointer , 4);
	pointer+=sizeof(unsigned int);

	unsigned int number = ntohl(buff);

	unsigned char day1_buf;
	unsigned char month1_buf;
	unsigned short year1_buf;
	unsigned char day2_buf;
	unsigned char month2_buf;
	unsigned short year2_buf;
	unsigned char hour_buf;
	unsigned char minute_buf;
	unsigned char second_buf;

	memcpy(&day1_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char day1 = day1_buf;
	memcpy(&month1_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char month1 = month1_buf;
	memcpy(&year1_buf, info->buffer+pointer, 2);
	pointer += sizeof(unsigned short);
	unsigned short year1 = ntohs(year1_buf);

	memcpy(&day2_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char day2 = day2_buf;
	memcpy(&month2_buf, info->buffer+pointer, 1);
	pointer+=sizeof(unsigned char);
	unsigned char month2 = month2_buf;
	memcpy(&year2_buf, info->buffer+pointer, 2);
	pointer += sizeof(unsigned short);
	unsigned short year2 = ntohs(year2_buf);
	
	memcpy(&hour_buf, info->buffer+pointer, 1);
	pointer += sizeof(unsigned char);
	unsigned char hour = hour_buf;
	memcpy(&minute_buf, info->buffer+pointer, 1);
	pointer += sizeof(unsigned char);
	unsigned char minute = minute_buf;
	memcpy(&second_buf, info->buffer+pointer, 1);
	pointer += sizeof(unsigned char);
	unsigned char second = second_buf;

	//message парсить не нужно, достатоно просто вывести буфер с позиции поинтер
	// запись в файл: IP:порт пробел сообщение (без номера)
	fprintf(f, "%u.%u.%u.%u:%u %02d.%02d.%04d %02d.%02d.%04d %02d:%02d:%02d %s\n",
		(info->ip >> 24) & 0xff, (info->ip >> 16) & 0xff, (info->ip >> 8) & 0xff, (info->ip) & 0xff,
		info->port,
		day1, month1, year1,
		day2, month2, year2,
		hour, minute, second,
		(char*)(info->buffer + pointer));
	fflush(f); // сразу на диск, чтобы не потерять при аварийной остановке
}
```

перед настройкой poll: 

```
// открытие файла для записи сообщений (каждый запуск с чистого файла)
	FILE* f = fopen("msg.txt", "w");
	if (f == NULL)
	{
		printf("Error open msg.txt\n");
		s_close(s);
		return 1;
	}
```

Изменим вызов функции: 

```
parse_message(&array[j], status_message, f);
```

## Шаг 11
По условию протокола на каждое сообщение нужно отвечать "ok". 

Изменим рядом с циклом while: 

```
int client_alive = 1; // 1 - клиент подключён, 0 - отключили
int status_message; // длина сообщения
while (array[j].status == 1 && (status_message = find_len_message(&array[j])) != 0) // пока находятся сообщения и есть put
{
	printf("LEN_MESSAGE = %d\n", status_message); // печать длины
	parse_message(&array[j], status_message, f);

	int sent = send(pfd[j].fd, "ok", 2, MSG_NOSIGNAL); // отправляем ок 
	if (sent == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			printf("WARN: ok not sent (buffer full)\n");
		}
		else
		{
			// настоящая ошибка, отключаем клиента
			sock_err("send", pfd[j].fd);
			close_client(&pfd[j], &array[j]);
			client_alive = 0;
			break; // буфер клиента освобождён, разбор продолжать нельзя
		}
	}
	else if (sent < 2)
	{
	// ушла только часть ok
		printf("WARN: ok sent partially (%d of 2)\n", sent);
	}

	memmove(array[j].buffer, array[j].buffer+status_message, array[j].bytes_cnt-status_message); // сдвиг на длину сообщения
	array[j].bytes_cnt -= status_message;
}

	if (client_alive == 0)
	{
		continue; // клиент отключён, переходим к следующему клиенту
	}
```

