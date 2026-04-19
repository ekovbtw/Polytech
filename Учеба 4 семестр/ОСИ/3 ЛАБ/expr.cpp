#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <locale.h>
#include <stdbool.h>

#define LL long long
#define COMB_SIZE 100
#define MAX_SIGNS 20

CRITICAL_SECTION task_cs;   // защищает раздачу задач и общий результат
HANDLE task_sem;            // семафор, ограничивающий число одновременно работающих потоков
HANDLE start_event;         // событие старта

int global_next_task_index = 0;    // индекс не пройденной комбинации
int global_count_combination = 0;  // всего комбинаций
int global_result = 0;      // общий ответ
int global_count_An = 0;    // количество чисел
int* global_Array_An = NULL; // массив чисел
LL global_S = 0;            // целевая сумма


typedef struct Combination
{
    int binary_sign[MAX_SIGNS];
    bool complete;
} Combination;

Combination global_task[COMB_SIZE]; // массив для хранения всех комбинаций 

void include_to_combination(int digit, int comb_index, int bit_index)
{
    global_task[comb_index].binary_sign[bit_index] = digit;
}

bool init_combination()
{
    int count_signs = global_count_An - 1;         // количество мест под +/-
    if (global_count_An - 1 > 20)
    {
        printf("Слишком много знаков для binary_sign[20]\n");
        return false;
    }

    //printf("Количество комбинаций: %d\n", global_count_combination);

    // на всякий случай проверка
    if (global_count_combination > COMB_SIZE)
    {
        printf("Слишком много комбинаций для массива global_task\n");
        return false;
    }

    // сначала очистим массив
    for (int i = 0; i < global_count_combination; i++)
    {
        for (int j = 0; j < count_signs; j++)
        {
            global_task[i].binary_sign[j] = 0;
        }
        global_task[i].complete = false;
    }

    // генерируем все комбинации
    for (int i = 0; i < global_count_combination; i++)
    {
        for (int j = 0; j < count_signs; j++)
        {
            int digit = (i >> j) & 1;

            include_to_combination(digit, i, count_signs - 1 - j);
        }
    }

    // отладка
    for (int i = 0; i < global_count_combination; i++)
    {
        for (int j = 0; j < count_signs; j++)
        {
            //printf("%d ", global_task[i].binary_sign[j]);
        }
        //printf("| complete = %d\n", global_task[i].complete);
    }

    return true;
}


bool check_combination(int task_index) // проверка комбинации на выполнение условия
{

    int count_signs = global_count_An - 1;

    int* comb = global_task[task_index].binary_sign;
    LL sum = global_Array_An[0]; // начинаем с первого числа
    for (int i = 0; i < count_signs; i++)
    {
        if (comb[i] == 1)
        {
            sum += global_Array_An[i + 1]; // ставим +
        }
        else
        {
            sum -= global_Array_An[i + 1]; // ставим -
        }
    }
    global_task[task_index].complete = true; // помечаем комбинацию как проверенную
    if (sum == global_S)
    {
        //printf("Комбинация %d подходит\n", task_index); // отладка
        return true; // комбинация подходит
    }
    else
    {

        //printf("Комбинация %d не подходит\n", task_index); // отладка
        return false; // комбинация не подходит
    }
}


DWORD WINAPI worker_thread(LPVOID param)
{
    // ждём сигнал старта алгоритма
    WaitForSingleObject(start_event, INFINITE);

    while (1)
    {
        int task_index;

        // Ограничиваем количество одновременно работающих потоков
        WaitForSingleObject(task_sem, INFINITE);

        // Безопасно забираем следующую задачу
        EnterCriticalSection(&task_cs);

        if (global_next_task_index >= global_count_combination)
        {
            LeaveCriticalSection(&task_cs);
            ReleaseSemaphore(task_sem, 1, NULL);
            break;
        }

        task_index = global_next_task_index;
        global_next_task_index++;

        LeaveCriticalSection(&task_cs);

        // Проверяем конкретную комбинацию
        if (check_combination(task_index))
        {
            EnterCriticalSection(&task_cs);
            global_result++;
            LeaveCriticalSection(&task_cs);
        }

        ReleaseSemaphore(task_sem, 1, NULL);
    }

    return 0;
}

int main()
{
    setlocale(LC_ALL, "Russian");
    FILE* file = fopen("input.txt", "r");

    if (file == NULL)
    {
        printf("Не удалось открыть input.txt\n");
        return 1;
    }

    int count_threads = 0; // количество потоков

    fscanf(file, "%d", &count_threads);
    fscanf(file, "%d", &global_count_An);

    // инициализация массива чисел из файла
    global_Array_An = (int*)malloc(global_count_An * sizeof(int)); // массив где хранятся числа
    if (global_Array_An == NULL)
    {
        printf("Не удалось выделить память под массив чисел\n");
        fclose(file);
        return 1;
    }

    for (int i = 0; i < global_count_An; i++)
    {
        fscanf(file, "%d", &global_Array_An[i]);
    }

    fscanf(file, "%lld", &global_S);
    fclose(file);

    global_count_combination = 1 << (global_count_An - 1); // инициализируем массив комбинаций

    if (!init_combination())
    {
        free(global_Array_An);
        return 1;
    }

    InitializeCriticalSection(&task_cs);

    task_sem = CreateSemaphore(NULL, count_threads, count_threads, NULL);
    if (task_sem == NULL)
    {
        printf("Не удалось создать семафор\n");
        DeleteCriticalSection(&task_cs);
        free(global_Array_An);
        return 1;
    }

    // создаём событие старта
    start_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (start_event == NULL)
    {
        printf("Не удалось создать start_event\n");
        CloseHandle(task_sem);
        DeleteCriticalSection(&task_cs);
        free(global_Array_An);
        return 1;
    }

    HANDLE* threads = (HANDLE*)malloc(count_threads * sizeof(HANDLE)); // создаем потоки
    if (threads == NULL)
    {
        printf("Не удалось выделить память под потоки\n");
        CloseHandle(start_event);
        CloseHandle(task_sem);
        DeleteCriticalSection(&task_cs);
        free(global_Array_An);
        return 1;
    }

    for (int i = 0; i < count_threads; i++) // передаем их в функцию 
    {
        threads[i] = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
        if (threads[i] == NULL)
        {
            printf("Не удалось создать поток %d\n", i);

            for (int j = 0; j < i; j++)
            {
                WaitForSingleObject(threads[j], INFINITE);
                CloseHandle(threads[j]);
            }

            free(threads);
            CloseHandle(start_event);
            CloseHandle(task_sem);
            DeleteCriticalSection(&task_cs);
            free(global_Array_An);
            return 1;
        }
    }

    // ---- старт замера времени ----
    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    SetEvent(start_event); // отпускаем потоки

    WaitForMultipleObjects(count_threads, threads, TRUE, INFINITE); // ожидание

    QueryPerformanceCounter(&t_end);

    double elapsed_ms = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / freq.QuadPart;
   

    // запись output.txt
    FILE* out = fopen("output.txt", "w");
    if (out != NULL)
    {
        fprintf(out, "%d\n", count_threads);
        fprintf(out, "%d\n", global_count_An);
        fprintf(out, "%d\n", global_result);
        fclose(out);
    }

    // запись time.txt
    FILE* t = fopen("time.txt", "w");
    if (t != NULL)
    {
        fprintf(t, "%.3f", elapsed_ms);
        fclose(t);
    }

    for (int i = 0; i < count_threads; i++) // освобождение потоков
    {
        CloseHandle(threads[i]);
    }

    free(threads);
    CloseHandle(start_event);
    CloseHandle(task_sem);
    DeleteCriticalSection(&task_cs);

    printf("%d\n", global_result);

    free(global_Array_An);
    return 0;
}