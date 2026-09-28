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
