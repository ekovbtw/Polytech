#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// 1 - ошибка в строке 
// 0 - строка корректна
unsigned char* init_BUFFER(int len, int day1, int month1, int year1, int day2, int month2, int year2, int hour, int minute, int second, char* message, int number); 
int send_BUFFER(int s, unsigned char* buffer, int len);
int recv_response(int s);

FILE* file_open(char* filename) // открываем файл 
{
	FILE* file = fopen(filename, "r");
	if (file == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}
	return file;
}

int check_date(int day, int month, int year) // проверка корректности даты
{
	if (day < 1 || day > 31 || month < 1 || month > 12 || year < 0)
	{
		return 1;
	}
	if ((day == 31 && month == 4) || (day == 31 && month == 6) || (day == 31 && month == 9) || (day == 31 && month == 11))
	{
		return 1;
	}
	if (month == 2 && day > 29)
	{
		return 1;
	}
	return 0;
}

int check_time(int hour, int minute, int second) // проверка корректности времени
{
	if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
	{
		return 1;
	}
	return 0;
}

int parse_file(char* buffer, int number, int s) // парсинг строки из файла
{
	// день, месяц, год, день, месяц, год, час, минута, секунда, сообщение
	int day1, month1, year1;
	int day2, month2, year2;
	int hour, minute, second;
	char message[256];
	
	int point; // позиция в строке после sscanf (для дальнейшего считывания сообщения)
	if (sscanf(buffer, "%d.%d.%d %d.%d.%d %d:%d:%d %n", &day1, &month1, &year1, &day2, &month2, &year2, &hour, &minute, &second, &point) != 9) // считываем данные из строки
	{
		return 1;
	}
	
	int status1 = check_date(day1, month1, year1); // проверяем корректность даты
	int status2 = check_date(day2, month2, year2);
	if (status1 == 1 || status2 == 1) 
	{
		return 1;
	}
	int status3 = check_time(hour, minute, second); // проверяем корректность времени
	if (status3 == 1)
	{
		return 1;
	}
	
	// сканируем сообщение из строки, начиная с позиции point 
	if (sscanf(buffer + point, "%[^\n]", message) != 1) // все символы кроме \n	
	{
		return 1;
	}
	//printf("Message: %s\n", message);

	int len = strlen(message); // проверка корректности message
	if (len > 255 || len == 0)
	{
		return 1;
	}

	unsigned char* BUFFER = init_BUFFER(len, day1, month1, year1, day2, month2, year2, hour, minute, second, message, number);
	int len_BUFFER = len + 15 + 1; 
	int status_send = send_BUFFER(s, BUFFER, len_BUFFER);
	if (status_send != 0)
	{
		printf("Error send BUFFER\n");
		free(BUFFER);
		return 1;
	}
	int status = recv_response(s);
	if (status == 1)
	{
		printf("Error recv_response\n");
		free(BUFFER);
		return 1;
	}
	free(BUFFER);	
	return 0;
}

unsigned char* init_BUFFER(int len, int day1, int month1, int year1, int day2, int month2, int year2, int hour, int minute, int second, char* message, int number) // сборка буфера по формату протокола, для дальнейшей отправки
{
	// создаем буфер
	unsigned char* BUFFER = NULL;
	/*
	* unsigned char потому что в протоколе указано, что данные передаются в виде байтов, используем unsigned тк нужен диапазон от 0 до 255.
	* изначально char может быть signed, и при преобразовании числа 200 в char, оно станет отрицательным числом, что не должны быть
	*/
	BUFFER = (unsigned char*)malloc(sizeof(unsigned char)*(15 + len + 1)); // 15 байт на форматирование, len байт на сообщение, 1 байт на \0
	if (BUFFER == NULL)
	{
		exit(1);
	}


	int pointer = 0; // текущая позиция в буфере
	int buff = 0; // временная переменная 

	// преобразования в сетевой порядок байтов
	buff = htonl(number); // number - 4 байта (номер сообщения)
	memcpy(BUFFER + pointer, &buff, sizeof(int)); 

	pointer += sizeof(int);	// смещаем указатель

	unsigned char day1_buf = (unsigned char)day1; // временные переменные, чтобы не было проблем с преобразованием типов
	unsigned char month1_buf = (unsigned char)month1;
	unsigned short year1_buf = (unsigned short)year1;
	unsigned char day2_buf = (unsigned char)day2;
	unsigned char month2_buf = (unsigned char)month2;
	unsigned short year2_buf = (unsigned short)year2;
	unsigned char hour_buf = (unsigned char)hour;
	unsigned char minute_buf = (unsigned char)minute;
	unsigned char second_buf = (unsigned char)second;

	year1_buf = htons(year1_buf); // 2 байта, поэтому используем htons, (1 байтовые значения не нужно преобразовывать)
	year2_buf = htons(year2_buf);

	memcpy(BUFFER+pointer, &day1_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER+pointer, &month1_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER+pointer, &year1_buf, sizeof(unsigned short));
	pointer += sizeof(unsigned short);
	memcpy(BUFFER+pointer, &day2_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER+pointer, &month2_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER+pointer, &year2_buf, sizeof(unsigned short));
	pointer += sizeof(unsigned short);
	memcpy(BUFFER+pointer, &hour_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER+pointer, &minute_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER+pointer, &second_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, message, len);
	pointer += len;
	memcpy(BUFFER + pointer, "\0", 1);
	pointer += 1;
	return BUFFER;
}


bool check_args(int argc, char* argv[]) // проверка аргументов командной строки
{
	if (argc != 3)
	{
		printf("Uncorrect arguments %s\n", argv[0]);
		return false;
	}
	return true;
}


void parse_ip_and_port(char* ip_port_str, char* ip, int* port) // парсинг ip и порта из аргументов командной строки, используем указатели тк функция void
{
	// получаем ip и порт из строки ip_port_str, два массива ip и port передаем в функцию по указателю для сохранения значений
	if (sscanf(ip_port_str, "%15[^:]:%d", ip, port) != 2) // считываем ip и порт из строки
	{
		printf("Uncorrect arguments\n");
		exit(1);
	}
	int len = strlen(ip);
	if (len > 15 || len < 7) // проверка корректности ip
	{
		printf("Uncorrect arguments\n");
		exit(1);
	}
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
	fprintf(stderr, "%s: socket error: %d\n", function, err); 
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

// Отправляет put 
int send_put(int s)
{
	char buffer[3];
	buffer[0] = 'p';
	buffer[1] = 'u';
	buffer[2] = 't';

	int size = sizeof(buffer);
	int sent = 0; // количество отправленных 
#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif
	while (sent < size) 
	{
		// Отправка очередного блока данных
		int res = send(s, buffer + sent, size - sent, flags);
		if (res < 0)
			return sock_err("send", s);
		sent += res;
		printf(" %d bytes sent.\n", sent);
	}
	return 0;
}


// отправка сообщения (по аналогии выше)
int send_BUFFER(int s, unsigned char* buffer, int len)
{
	int sent = 0;
#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif
	while (sent < len)
	{
		// Отправка очередного блока данных
		int res = send(s, (char*)buffer + sent, len - sent, flags);
		if (res < 0)
			return sock_err("send", s);
		sent += res;
		printf(" %d bytes sent.\n", sent);
	}
	return 0;
}


// получаем "ok" подтверждение
int recv_response(int s) 
{
	printf("recv_response called\n");
	char buffer[2]; // буффер для ok 
	int all_res = 0; // общее накопленое количество байт
	int point = 0; // сдвиг для буффера чтобы не дублировать индексы
	bool status = true; 
	// Если соединение будет разорвано удаленным узлом recv вернет 0
	printf("all_res = %d\n", all_res);
	while (all_res < 2)
	{
		int current_res = 0; // текущее количество байт 
		current_res += recv(s, buffer+point, sizeof(buffer)-point, 0);
		printf("current_res = %d\n", current_res);
		all_res += current_res;
		point += current_res;
		if (current_res == 0)
		{
			printf("connection close\n");
			status = false;
			break;
		}
		else if (current_res < 0)
		{
			printf("connection error\n");
			status = false;
			break;
		}
	}
	printf("all_res = %d, status = %d\n", all_res, status);
	if (status == false)
		return 1;
	return 0;
}


int main(int argc, char* argv[])
{
	setlocale(LC_ALL, "Russian");
	struct sockaddr_in addr; // структура для хранения адреса сервера
	

	if (!check_args(argc, argv)) // проверка аргументов поданных на вход
	{
		return 1;
	}
	

	char ip[16]; // массив для хранения ip, 16 байт тк максимальная длина 15 символов + \0
	int port = 0; // переменная для хранения порта
	parse_ip_and_port(argv[1], ip, &port); // парсинг ip и порта из аргументов командной строки


	FILE* file = file_open(argv[2]); // закидываем имя файла


	int init_status = init(); // инициализация сокетов
	if (init_status == 0)
	{
		printf("Error initializing sockets\n");
		return 1;
	}
	

	/* обнуление и заполнение структуры адреса */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET; // IPv4
	addr.sin_port = htons(port); // преобразуем порт в сетевой порядок байтов
	int status_inet_pton = inet_pton(AF_INET, ip, &addr.sin_addr); // преобразуем ip из строки в структуру in_addr
	if (status_inet_pton == -1 || status_inet_pton == 0) // -1 если некорректный адрес, 0 если адрес по длине корректный, но диапазон чисел некорректный
	{
		printf("Error inet_pton\n");
		return 1; 
	}


	// Установка соединения с удаленным хостом
	int s; // сокет
	int counter_connect = 0; // счетчик до 10 попыток
	int connect_status = -1; // статус connect, -1 если неудача, 0 если удача
	while (counter_connect < 10)
	{
		s = socket(AF_INET, SOCK_STREAM, 0); // один сокет = одна итерация, иначе будет работать не корректно
		if (s < 0)
		{
			return sock_err("socket", s);
		}
		if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0) // если подключение успешно
		{
			connect_status = 0; // удача
			break;
		}
		// иначе
		counter_connect++;
		s_close(s); // закрытие сокета, чтобы его пересоздать
		Sleep(100);	// спим 100 мс
	}
	if (connect_status == -1) // если так и не подключился
	{
		s_close(s);
		return sock_err("connect", s);
	}
	

	int status_send = send_put(s);
	if (status_send != 0)
	{
		printf("Error send put\n");
		return 1;
	}

	int counter = 0;
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), file) != NULL)
	{
		if (strlen(buffer) == 1)
		{
			continue;
		}
		int status = parse_file(buffer, counter, s);
		if (status == 1)
		{
			continue;
		}
		else
		{
			counter++;
		}
	}


	printf("%d\n", counter);
	fclose(file);
	deinit(); // деинициализация сокетов
	s_close(s);
	return 0;
}