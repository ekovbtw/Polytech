// LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h> 


typedef struct DATA
{
    unsigned char* data; 
    int len;
    int status;
} DATA;

DATA* array = NULL;
int count_data = 0;
int count_status_delivered = 0;
int capacity = 0;
struct sockaddr_in addr;

FILE* file_open(char* filename) 
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
	if (day < 1 || day > 31 || month < 1 || month > 12 || year < 0 || year > 9999) 
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

bool check_args(int argc, char* argv[]) // проверка аргументов командной строки
{
	if (argc != 3)
	{
		printf("Uncorrect arguments %s\n", argv[0]);
		return false;
	}
	return true;
}

void parse_ip_and_port(char* ip_port_str, char* ip, int* port) // парсинг ip и порта
{
	if (sscanf(ip_port_str, "%15[^:]:%d", ip, port) != 2)
	{
		printf("Uncorrect arguments\n");
		exit(1);
	}
	int len = strlen(ip);
	if (len > 15 || len < 7) // проверка длины ip
	{
		printf("Uncorrect arguments\n");
		exit(1);
	}
	if (*port < 1 || *port > 65535) // проверка диапазона порта
	{
		printf("Uncorrect port\n");
		exit(1);
	}
}

int sock_err(const char* function, int s) // вывод ошибки сокета
{
	int err = errno;
	fprintf(stdout, "%s: socket error: %d\n", function, err);
	return -1;
}

void s_close(int s) // закрытие сокета
{
	close(s);
}


unsigned char* init_BUFFER(int len, int day1, int month1, int year1, int day2, int month2, int year2, int hour, int minute, int second, char* message, int number) // сборка буфера по формату протокола
{
	unsigned char* BUFFER = NULL;
	BUFFER = (unsigned char*)malloc(sizeof(unsigned char) * (15 + len + 1)); // 15 байт заголовок, len байт текст, 1 байт \0
	if (BUFFER == NULL)
	{
		printf("Memory allocation failed\n"); 
		exit(1);
	}

	int pointer = 0; // текущая позиция в буфере
	int buff = 0; // временная переменная

	buff = htonl(number); // номер сообщения - 4 байта, сетевой порядок
	memcpy(BUFFER + pointer, &buff, sizeof(int));
	pointer += sizeof(int);

	unsigned char day1_buf = (unsigned char)day1;
	unsigned char month1_buf = (unsigned char)month1;
	unsigned short year1_buf = (unsigned short)year1;
	unsigned char day2_buf = (unsigned char)day2;
	unsigned char month2_buf = (unsigned char)month2;
	unsigned short year2_buf = (unsigned short)year2;
	unsigned char hour_buf = (unsigned char)hour;
	unsigned char minute_buf = (unsigned char)minute;
	unsigned char second_buf = (unsigned char)second;

	year1_buf = htons(year1_buf); // 2 байта - htons, однобайтовые не преобразуем
	year2_buf = htons(year2_buf);

	memcpy(BUFFER + pointer, &day1_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, &month1_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, &year1_buf, sizeof(unsigned short));
	pointer += sizeof(unsigned short);
	memcpy(BUFFER + pointer, &day2_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, &month2_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, &year2_buf, sizeof(unsigned short));
	pointer += sizeof(unsigned short);
	memcpy(BUFFER + pointer, &hour_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, &minute_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, &second_buf, 1);
	pointer += sizeof(unsigned char);
	memcpy(BUFFER + pointer, message, len);
	pointer += len;
	memcpy(BUFFER + pointer, "\0", 1);
	pointer += 1;
	return BUFFER;
}

int add_to_data_array (unsigned char* data, int len) // добавление сообщения в массив структуры
{
    if (count_data >= capacity)
    {
        int new_capacity = 16; // начальная емкость
        if (capacity != 0)
        {
            new_capacity += capacity;
        }
        DATA* tmp = (DATA*)realloc (array, new_capacity*sizeof(DATA));
        if (tmp == NULL)
        {
            printf("Error realloc\n");
            return -1;
        }
        else
        {
            array = tmp;
            
            capacity = new_capacity;
        }
    }
    array[count_data].data = data;
        array[count_data].len = len;
        array[count_data].status = 0;
        count_data++;
        return 0;
}

#define MAX_TEXT_LEN 65491 // maxUDP

int parse_line(char* line, int number)
{
	int day1, month1, year1;
	int day2, month2, year2;
	int hour, minute, second;
	int point = 0; 

	if (sscanf(line, "%d.%d.%d %d.%d.%d %d:%d:%d %n", &day1, &month1, &year1, &day2, &month2, &year2, &hour, &minute, &second, &point) != 9)
	{
		return 1;
	}
	if (check_date(day1, month1, year1) == 1 || check_date(day2, month2, year2) == 1)
	{
		return 1;
	}
	if (check_time(hour, minute, second) == 1)
	{
		return 1;
	}

	char* message = line + point; // текст - хвост строки, копировать не нужно
	int len = strlen(message);
	if (len == 0 || len > MAX_TEXT_LEN)
	{
		return 1;
	}

	unsigned char* BUFFER = init_BUFFER(len, day1, month1, year1, day2, month2, year2, hour, minute, second, message, number);
	if (add_to_data_array(BUFFER, 15 + len + 1) == -1)
	{
		printf("Memory allocation failed\n");
		free(BUFFER);
		exit(1);
	}
	return 0;
}


int send_again (int s)
{
    int sent_cnt = 0;
	int i = 0;
	while (i < count_data)
	{
		if (array[i].status == 0) 
		{
			int res = sendto(s, array[i].data, array[i].len, MSG_NOSIGNAL, (struct sockaddr*)&addr, sizeof(addr));
			if (res < 0)
			{
				sock_err("sendto", s); 
			}
			else
			{
				sent_cnt++;
			}
		}
		i++;
	}
	return sent_cnt;
}

pollfd init_poll (int s)
{
    struct pollfd pfd;
	pfd.fd = s;
	pfd.events = POLLIN; 
	pfd.revents = 0;
    return pfd;
}

int recv_response(int s, unsigned char* response)
{
	
    pollfd pfd = init_poll (s);
	int ev_cnt = poll(&pfd, 1, 100); // 1 сокет, 100 мс
	if (ev_cnt == 0)
	{
		return 0; 
	}
	if (ev_cnt < 0)
	{
		sock_err("poll", s);
		return -1;
	}

	if ((pfd.revents & POLLIN) || (pfd.revents & POLLERR)) // данные или ошибка (ICMP)
	{
		struct sockaddr_in from_addr;
		socklen_t addrlen = sizeof(from_addr);
		int received = recvfrom(s, response, 1024, 0, (struct sockaddr*)&from_addr, &addrlen);
		if (received < 0)
		{
			if (errno != ECONNREFUSED) 
			{
				sock_err("recvfrom", s);
			}
			return -1;
		}
		return received;
	}
	return 0;
}


void parse_from_server (unsigned char* response, int size)
{
	int pointer = 0;
	while (pointer + 4 <= size)
	{
		unsigned int number_buf;
		memcpy(&number_buf, response + pointer, sizeof(unsigned int));
		pointer += sizeof(unsigned int);
		unsigned int number = ntohl(number_buf); 
		if (number < (unsigned int)count_data && array[number].status == 0) 
		{
			array[number].status = 1;
			count_status_delivered++;
		}
	}
}

int main(int argc, char* argv[])
{

	if (!check_args(argc, argv)) // проверка аргументов
	{
		return 1;
	}

    char ip[16] = {0};
    int port;
    parse_ip_and_port(argv[1], ip, &port);

    FILE* file = file_open(argv[2]);


    memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET; // IPv4
	addr.sin_port = htons(port); // порт в сетевой порядок байт

    int status_inet_pton = inet_pton(AF_INET, ip, &addr.sin_addr); 
	if (status_inet_pton != 1) // 0 - строка не похожа на IPv4, -1 - ошибка
	{
		printf("Error inet_pton\n");
		fclose(file);
		return 1;
	}

    int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
	{
		sock_err("socket", s);
		fclose(file);
		return 1;
	}


    char* line = NULL; 
	size_t line_cap = 0; // размер буфера
	ssize_t read_len;
	int counter = 0; // номер следующего корректного сообщения
	while ((read_len = getline(&line, &line_cap, file)) != -1)
	{
		while (read_len > 0 && (line[read_len - 1] == '\n' || line[read_len - 1] == '\r'))
		{
			line[read_len - 1] = '\0';
			read_len--;
		}
		if (read_len == 0) // пустая строка
		{
			continue;
		}
		if (parse_line(line, counter) == 0)
		{
			counter++;
		}
	}
    free(line);

    int cnt_del = 20;
    if (count_data < 20)
    {
        cnt_del = count_data;
    }

    unsigned char response[1024];


    while (count_status_delivered<cnt_del)
    {
        send_again(s);
        int status_wait = 1;
        while (status_wait == 1 && count_status_delivered < cnt_del)
		{
			int received = recv_response(s, response);
			if (received > 0)
			{
				parse_from_server(response, received); 
			}
			else if (received == 0)
			{
				status_wait = 0; // 100 мс тишины - новый раунд отправки
            }	
        }
    }
	printf("Delivered: %d of %d\n", count_status_delivered, count_data);
    int i = 0;
	while (i < count_data)
	{
		free(array[i].data);
		i++;
	}
	free(array);

    s_close(s);
	fclose(file);
	return 0;
}