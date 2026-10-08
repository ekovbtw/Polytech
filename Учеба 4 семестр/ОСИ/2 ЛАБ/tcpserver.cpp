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
#define START_CAPACITY 512 // стартовая емкость для буфера клиента
#define OK_BUF_SIZE 512 // размер заготовки "okok..." (должен быть чётным)
int count_clients = 0;


typedef struct client_info // client structure
{
	unsigned int port; 
	unsigned int ip; 
	int i;
	unsigned char* buffer; // буффер для накопления сообщения
	int bytes_cnt; // количество байт, которые уже пришли
	int status; // статус: put - 1, else - 0
	int capacity;
	int send_bytes; // сколько байт ok ещё нужно отправить клиенту
	int stop; // 1 - клиент прислал stop, после отправки ok сервер завершается

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


void close_client(pollfd* pfd_info, client_info* info) // закрытие клиента, если больше не общается
{
	s_close(pfd_info->fd); // закрытие сокета
	pfd_info->fd = -1; // очистка
	pfd_info->events = 0;
	pfd_info->revents = 0;
	printf("Client disconnected: %u.%u.%u.%u: %d\n", (info->ip >> 24) & 0xff, (info->ip >> 16) & 0xff, (info->ip >> 8) & 0xff, (info->ip) & 0xff, info->port);
	free(info->buffer);
	memset(info, 0, sizeof(client_info)); // очистка
}


// отправка накопленных ok клиенту
// возвращает 0 - всё хорошо (даже если часть осталась на потом), -1 - ошибка, клиента надо отключить
int send_ok(pollfd* pfd_info, client_info* info)
{
	char ok_buf[OK_BUF_SIZE]; // заготовка "okokok..."
	for (int i = 0; i < OK_BUF_SIZE; i += 2)
	{
		ok_buf[i] = 'o';
		ok_buf[i+1] = 'k';
	}

	while (info->send_bytes > 0)
	{
		int start = info->send_bytes % 2; // осталось нечётное число байт - значит следующий 'k' (позиция 1)
		int len = info->send_bytes;
		if (len > OK_BUF_SIZE - start)
		{
			len = OK_BUF_SIZE - start; // не больше, чем есть в заготовке
		}

		int sent = send(pfd_info->fd, ok_buf + start, len, MSG_NOSIGNAL);
		if (sent == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break; // буфер отправки полон, остаток дошлём по POLLOUT
			}
			sock_err("send", pfd_info->fd);
			return -1;
		}
		info->send_bytes -= sent;
	}

	// POLLOUT нужен только пока есть что досылать, иначе poll будет срабатывать постоянно
	if (info->send_bytes > 0)
	{
		pfd_info->events = POLLIN | POLLOUT;
	}
	else
	{
		pfd_info->events = POLLIN;
	}
	return 0;
}


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


int find_len_message (client_info *info)
{
	for (int i = 15; i<=info->bytes_cnt-1; i++)
	{
		if (info->buffer[i] == '\0') return i+1;
	}
	return 0;
}


// разбор сообщения из начала буфера и запись в файл
// возвращает 1, если текст сообщения "stop", иначе 0
int parse_message (client_info *info, int len, FILE* f)
{
	unsigned int buff; // логика такая же как в клиенте, делаем буферную переменную, преобразовываем из сетевого порядка байт, перезаписываем в нормальную переменную
	int pointer = 0; 
	memcpy(&buff, info->buffer+pointer , 4);
	pointer+=sizeof(unsigned int);

	unsigned int number = ntohl(buff);
	(void)number; // номер в файл не пишется, переменная оставлена для отладки

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

	// проверка служебного слова stop (текст ровно "stop")
	if (strcmp((char*)(info->buffer + pointer), "stop") == 0)
	{
		return 1;
	}
	return 0;
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


	// открытие файла для записи сообщений (каждый запуск с чистого файла)
	FILE* f = fopen("msg.txt", "w");
	if (f == NULL)
	{
		printf("Error open msg.txt\n");
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

	// настройка массива клиентов 
	client_info array[MAX_CLIENTS];
	for (int i = 0; i<MAX_CLIENTS; i++)
	{
		array[i].buffer = NULL;
		array[i].bytes_cnt = 0;
		array[i].ip = 0;
		array[i].port = 0;
		array[i].status = 0;
		array[i].capacity = 0;
		array[i].send_bytes = 0;
		array[i].stop = 0;
	}


	int server_running = 1; // 0 - пришёл stop, выходим из главного цикла

	while (server_running == 1)
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
							
								array[k].i = k; // запоминаем номер слота
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
							printf("Too many clients\n");
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
						continue;
					}

					if (pfd[j].revents & POLLIN) // если чтение
					{
						if (array[j].capacity - array[j].bytes_cnt < START_CAPACITY) // если свободного места меньше 512 
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
							continue;
						}
						else if (status == -1)
						{
							if (errno != EAGAIN && errno != EWOULDBLOCK)
							{
								sock_err("recv", pfd[j].fd);
								close_client(&pfd[j], &array[j]);
								continue;
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

							int status_message; // длина сообщения
							// пока есть put, не было stop и находятся полные сообщения
							while (array[j].status == 1 && array[j].stop == 0 && (status_message = find_len_message(&array[j])) != 0)
							{
								printf("LEN_MESSAGE = %d\n", status_message); // печать длины
								int is_stop = parse_message(&array[j], status_message, f);

								array[j].send_bytes += 2; // на каждое сообщение должны клиенту "ok"

								memmove(array[j].buffer, array[j].buffer+status_message, array[j].bytes_cnt-status_message); // сдвиг на длину сообщения
								array[j].bytes_cnt -= status_message;

								if (is_stop == 1)
								{
									printf("STOP received\n");
									array[j].stop = 1; // дальше сообщения от клиента не разбираем
									break;
								}
							}

							// отправляем все накопленные ok одним разом
							if (array[j].send_bytes > 0)
							{
								if (send_ok(&pfd[j], &array[j]) == -1)
								{
									close_client(&pfd[j], &array[j]);
									continue;
								}
							}

							printf("Bytes_cnt now = %d\n", array[j].bytes_cnt);
							printf("%d bytes from: %u.%u.%u.%u: %d\n", status, (array[j].ip >> 24) & 0xff, (array[j].ip >> 16) & 0xff, (array[j].ip >> 8) & 0xff, (array[j].ip) & 0xff, array[j].port);
						}
					}

					// досылка ok, если сокет снова готов к записи (может прийти вместе с POLLIN, поэтому отдельный if)
					if (pfd[j].revents & POLLOUT)
					{
						if (send_ok(&pfd[j], &array[j]) == -1)
						{
							close_client(&pfd[j], &array[j]);
							continue;
						}
					}

					// stop: ok этому клиенту полностью отправлен - завершаем сервер
					if (array[j].stop == 1 && array[j].send_bytes == 0)
					{
						server_running = 0;
						break;
					}
				}
			}
		}
		
		
	}

	// stop: отключаем всех клиентов, закрываем слушающий сокет и файл
	for (int i = 0; i<MAX_CLIENTS-1; i++)
	{
		if (pfd[i].fd != -1)
		{
			close_client(&pfd[i], &array[i]);
		}
	}
	s_close(s);
	fclose(f);
	printf("Server stopped\n");
	return 0;
}