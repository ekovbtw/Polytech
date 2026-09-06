#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <Windows.h>
//#include <intrin.h>
#define SUM_MAX (2147483647LL + 99999LL)
#define SUM_MIN (-2147483648LL - 99999LL)
#define LL long long
#define DEBUG_PRINT 0
#define SIZE 512
CRITICAL_SECTION crit_sec;
int count_threads = 0;
int* start_array = NULL;
int N = 0;
HANDLE start_sem;
LL result = 0;
DWORD WINAPI worker_thread(LPVOID param);
LL count_solved = 0;
LL next_task = 0;
DWORD start_time, end_time;
LL total = 0;
LL total_combinations();
int number_of_digits(LL task_index); // функция для номера знака (для кода Грэя)

FILE* open_file_and_scan()
{
	FILE* file = fopen("input.txt", "r");
	if (file == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}
	return file;
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
	fprintf(file, "%d\n", count_threads);
	fprintf(file, "%d\n", N);
	fprintf(file, "%lld\n", count_solved);
	fclose(file);
}

int main()
{
	setlocale(LC_ALL, "Russian");
	FILE* file = open_file_and_scan();
	fscanf(file, "%d", &count_threads);
	fscanf(file, "%d", &N);
	start_array = (int*)malloc(sizeof(int) * N);
	InitializeCriticalSection(&crit_sec);

	for (int i = 0; i < N; i++)
	{
		fscanf(file, "%d", &start_array[i]);
	}

	fscanf(file, "%lld", &result);
	fclose(file);

	
	total = total_combinations();

	
	start_sem = CreateSemaphore(NULL, 0, count_threads, NULL);
	HANDLE* threads = (HANDLE*)malloc(count_threads * sizeof(HANDLE));

	for (int i = 0; i < count_threads; i++)
	{
		threads[i] = CreateThread(NULL, 0, worker_thread, (LPVOID)(size_t)i, 0, NULL);
	}

	start_time = GetTickCount();


	ReleaseSemaphore(start_sem, count_threads, NULL);

	for (int i = 0; i < count_threads; i++)
	{
		WaitForSingleObject(threads[i], INFINITE);
	}

	end_time = GetTickCount();

	time_to_file();
	result_to_file();

	for (int i = 0; i < count_threads; i++)
		CloseHandle(threads[i]);

	free(threads);
	CloseHandle(start_sem);
	DeleteCriticalSection(&crit_sec);
	free(start_array);
	return 0;
}

LL total_combinations()
{
	return 1LL << (N - 1);
}

bool check_combination(LL task_index)
{
	int count_signs = N - 1;
	LL sum = (LL)start_array[0];

	for (int i = 0; i < count_signs; i++)
	{
		int digit = (task_index >> (count_signs - 1 - i)) & 1;
		if (digit == 1)
			sum += (LL)start_array[i + 1];
		else
			sum -= (LL)start_array[i + 1];

		if (sum > SUM_MAX || sum < SUM_MIN)
			return false;
	}

	if (sum == result)
	{
#if DEBUG_PRINT
		EnterCriticalSection(&crit_sec);
		printf("Решение: %d", start_array[0]);
		for (int i = 0; i < count_signs; i++)
		{
			int digit = (task_index >> (count_signs - 1 - i)) & 1;
			printf(" %c %d", digit ? '+' : '-', start_array[i + 1]);
		}
		printf(" = %lld\n", result);
		LeaveCriticalSection(&crit_sec);
#endif
		return true;
	}
	return false;
}

DWORD WINAPI worker_thread(LPVOID param)
{
	int idx = (int)(size_t)param;

	// Поток блокируется здесь, пока main не вызовет ReleaseSemaphore
	WaitForSingleObject(start_sem, INFINITE);

	while (true)
	{
		int index;
		int end_index;

		EnterCriticalSection(&crit_sec);
		if (next_task >= total)
		{
			LeaveCriticalSection(&crit_sec);
			break;
		}
		index = next_task;
		end_index = next_task + SIZE;
		if (end_index > total)
			end_index = (int)total;
		next_task = end_index;
		LeaveCriticalSection(&crit_sec);

		int local_solved = 0;
		
		int count_signs = N - 1; // количество знаков
		int current_digit_array[64] = { 0 }; // массив для хранения текущего знака каждого числа (0 - минус, 1 - плюс)
		LL summ = start_array[0]; // сумма текущей комбинации, начинаем с первого числа (тк оно всегда положительное)
		int Grey_code = index ^ (index >> 1); // код Грея для текущего индекса (xor - ^)
		for (int k = 0; k < count_signs; k++)
		{
			int Grey_code_new = Grey_code >> k;
			Grey_code_new = Grey_code_new & 1; // получение знака
			current_digit_array[k] = Grey_code_new; // запись текущего знака (для текущего числа) в массив
			if (Grey_code_new == 1) // обработка знака
			{
				summ += start_array[k + 1];
			}
			else
			{
				summ -= start_array[k + 1];
			}
		}
		if (summ == result)
		{
			local_solved++;
		}
		for (int i = index + 1; i < end_index;i++)
		{
			int k = number_of_digits(i); // получили позицию знака, который изменился
			current_digit_array[k] = 1 - current_digit_array[k]; // меняем знак на противоположный
			if (current_digit_array[k] == 1) // обработка знака
			{
				summ += 2 * start_array[k + 1]; // прибавляем дважды, тк мы меняем знак с минуса на плюс
			}
			else
			{
				summ -= 2 * start_array[k + 1]; // вычитаем дважды, тк мы меняем знак с плюса на минус
			}
			if (summ == result)
			{
				local_solved++;
			}
		}
		if (local_solved > 0)
		{
			EnterCriticalSection(&crit_sec);
			count_solved += local_solved;
			LeaveCriticalSection(&crit_sec);
		}
	}

	return 0;
}

int number_of_digits(LL task_index) // суть в том чтобы найти позицию первой еденицы справа 
{
	int c = 0;
	while ((task_index & 1) == 0)
	{
		task_index >>= 1;
		c++;
	}
	return c;
}