#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>
#include <locale.h>
#define LL unsigned long long
CRITICAL_SECTION crit_sec;

DWORD start_time, end_time;
int count_threads = 0; // кол-во потоков
int N = 0; // размер доски
int L = 0; // количество ладьей
int K = 0; // количество поставленных изначально
LL result = 0; // количество комбинаций
int start_str = -1; // начальная строка
int M = 0; // количество свободных столбцов
int	task_count = 0;
int flag = 0; // флаг особого случая (когда K==N && L>0)
int next_task = 0;

typedef struct KOOR
{
	int x;
	int y;
} KOOR;
typedef struct TASK
{
	int status; // 0 - пропуск, 1 - ставим ладью
	int column;
	int start_row;
} TASK;
typedef struct DATA
{
	LL local_result; 
	int* buffer_col;
} DATA;

void logic(int counter, int index, DATA* data);
DWORD WINAPI worker_thread(LPVOID param);

KOOR* k_array = NULL;
TASK* task_array = NULL;
DATA* data_array = NULL;
int* free_koor_str = NULL;
int* free_koor_col = NULL;
 // буферные массив для запоминания координат
int* buffer_col = NULL;


void readfile()
{
	FILE* file = fopen("input.txt", "r");
	if (file == NULL) exit(-1);
	if (fscanf(file, "%d", &count_threads) != 1)
	{
		fclose(file);
		exit(1);
	}
	

	if (fscanf(file, "%d", &N) != 1)
	{
		fclose(file);
		exit(1);
	}

	if (fscanf(file, "%d", &L) != 1)
	{
		fclose(file);
		exit(1);
	}

	if (fscanf(file, "%d", &K) != 1)
	{
		fclose(file);
		exit(1);
	}
	k_array = (KOOR*)malloc(sizeof(KOOR) * K);
	for (int i = 0; i < K; i++)
	{

		if (fscanf(file, "%d", &k_array[i].x) != 1)
		{
			fclose(file);
			exit(1);
		}
		if (k_array[i].x<0 || k_array[i].x>(N - 1))
		{
			fclose(file);
			exit(2);
		}

		if (fscanf(file, "%d", &k_array[i].y) != 1)
		{
			fclose(file);
			exit(1);
		}
		if (k_array[i].y<0 || k_array[i].y>(N - 1))
		{
			fclose(file);
			exit(2);
		}
	}

	
	free_koor_str = (int*)malloc(sizeof(int) * N); // 1 - занят, 0 - свободен
	
	free_koor_col = (int*)malloc(sizeof(int) * N);
	for (int i = 0; i < N; i++)
	{
		free_koor_str[i] = 0;
		free_koor_col[i] = 0;
	}
	for (int i = 0; i < K; i++)
	{
		free_koor_str[k_array[i].x] = 1;
		free_koor_col[k_array[i].y] = 1;
	}
	
	 // 1 - занят, 0 - свободен
	buffer_col = (int*)malloc(sizeof(int) * N);
	for (int i = 0; i < N; i++)
	{
		buffer_col[i] = 0;
	}
	for (int i = 0; i < N;i++)
	{
		if (free_koor_str[i] == 1) continue;
		else
		{
			start_str = i;
			break;
		}
	}
	// проверка крайних вариантов 
	if (K == N && L > 0) { result = 0; flag = 1; fclose(file); return; }
	else if (K == N && L == 0) { result = 1; fclose(file); return; }
	
	// задачи
	M = N - K; // число свободных столбцов
	task_count = M + 1; // число задач M + 1 (все столбцы + пропуск)
	task_array = (TASK*)malloc(sizeof(TASK) * task_count);
	int index = 0; // index - свободного столбца
	for (int i = 0; i < task_count; i++)
	{
		task_array[i].start_row = start_str; // начальная стр
		if (i == 0) { task_array[i].status = 0; task_array[i].column = -1; continue; } // пропуск
		for (int j = index; j < N; j++)
		{
			if (free_koor_col[j] == 0) { index = j; break; }
		}
		task_array[i].column = index;
		index++;
		task_array[i].status = 1;
	}

	fclose(file);
}
void data_entry()
{
	data_array = (DATA*)malloc(sizeof(DATA) * task_count);
	for (int i = 0;i < task_count;i++)
	{
		data_array[i].buffer_col = (int*)malloc(sizeof(int) * N);
		for (int j = 0; j < N; j++)
		{
			data_array[i].buffer_col[j] = 0;
		}
		data_array[i].local_result = 0;
		if (task_array[i].status == 1)
		{
			data_array[i].buffer_col[task_array[i].column] = 1;
		}
	}
	
}

void time_to_file()
{
	FILE* file = fopen("time.txt", "w");
	if (file == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}
	DWORD elapsed = end_time - start_time;
	fprintf(file, "%lu", elapsed);
	fclose(file);
}

void result_to_file()
{
	FILE* file = fopen("output.txt", "w");
	if (file == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}
	fprintf(file, "%llu", result);
	fclose(file);
}


int main()
{
	readfile();
	data_entry();
	if (result == 0 && flag == 0)
	{
		HANDLE* threads = (HANDLE*)malloc(count_threads * sizeof(HANDLE));
		InitializeCriticalSection(&crit_sec);

		for (int i = 0; i < count_threads; i++)
		{
			threads[i] = CreateThread(NULL, 0, worker_thread, (LPVOID)(size_t)i, CREATE_SUSPENDED, NULL);
		}
		start_time = GetTickCount();
		for (int i = 0; i < count_threads; i++)
		{
			ResumeThread(threads[i]);
		}
		for (int i = 0; i < count_threads; i++)
		{
			WaitForSingleObject(threads[i], INFINITE);
		}
		end_time = GetTickCount();
		for (int i = 0; i < count_threads; i++)
			CloseHandle(threads[i]);

		DeleteCriticalSection(&crit_sec);
		free(threads);
	}
	for (int i = 0; i < task_count; i++)
	{
		result += data_array[i].local_result;
	}
	
	time_to_file();
	result_to_file();
	free(k_array);
	free(task_array);
	free(data_array);
	free(free_koor_str);
	free(free_koor_col);
	for (int i = 0; i < task_count; i++)
	{
		free(data_array[i].buffer_col);
	}
	return 0;
}

void logic(int counter, int index, DATA* data)
{
	if (counter == L) data->local_result++;
	else if (counter < L && index == N) return;
	else if (counter < L && index < N)
	{
		if (free_koor_str[index] == 1) logic(counter, index + 1, data);
		else
		{
			logic (counter, index + 1, data);
			for (int i = 0; i < N;i++)
			{
				if (free_koor_col[i] == 1 || data->buffer_col[i] == 1) continue;
				else
				{
					data->buffer_col[i] = 1; 
					logic(counter+1, index + 1, data);
					data->buffer_col[i] = 0; 
				}
			}
		}	
	}
}  

DWORD WINAPI worker_thread(LPVOID param)
{
	while (true)
	{
		EnterCriticalSection(&crit_sec);
		if (next_task >= task_count)
		{
			LeaveCriticalSection(&crit_sec);
			break;
		}
		int index = 0;
		index = next_task;
		next_task++;
		LeaveCriticalSection(&crit_sec);
		if (task_array[index].status == 0) // задача пропуск
		{
			logic(0, task_array[index].start_row + 1, &data_array[index]);
		}
		else
		{
			// задача после постановки
			logic(1, task_array[index].start_row + 1, &data_array[index]);
		}
	}
	return 0;
}
