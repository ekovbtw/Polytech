#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <locale.h>
#include <stdbool.h>
#include <time.h>
#define SUM_MAX (2147483647LL + 99999LL)
#define SUM_MIN (-2147483648LL - 99999LL)
#define LL long long
#define DEBUG_PRINT 0
#define MAX_THREADS 64

CRITICAL_SECTION task_cs;
HANDLE start_sem;

LL global_next_task_index = 0;
LL global_count_combination = 0;
int global_result = 0;
int global_count_An = 0;
int* global_Array_An = NULL;
LL global_S = 0;

LL global_checked[MAX_THREADS];
LL global_found[MAX_THREADS];


bool init_combination()
{
    int count_signs = global_count_An - 1;

    if (count_signs <= 0)
    {
        printf("Недостаточно чисел\n");
        return false;
    }

    global_count_combination = 1LL << count_signs;
    return true;
}


bool check_combination(LL task_index)
{
    int count_signs = global_count_An - 1;
    LL sum = (LL)global_Array_An[0];

    for (int i = 0; i < count_signs; i++)
    {
        int digit = (task_index >> (count_signs - 1 - i)) & 1;
        if (digit == 1)
            sum += (LL)global_Array_An[i + 1];
        else
            sum -= (LL)global_Array_An[i + 1];

        if (sum > SUM_MAX || sum < SUM_MIN)
            return false;
    }

    if (sum == global_S)
    {
#if DEBUG_PRINT
        EnterCriticalSection(&task_cs);
        printf("Решение: %d", global_Array_An[0]);
        for (int i = 0; i < count_signs; i++)
        {
            int digit = (task_index >> (count_signs - 1 - i)) & 1;
            printf(" %c %d", digit ? '+' : '-', global_Array_An[i + 1]);
        }
        printf(" = %lld\n", global_S);
        LeaveCriticalSection(&task_cs);
#endif
        return true;
    }
    return false;
}


DWORD WINAPI worker_thread(LPVOID param)
{
    int idx = (int)(size_t)param;
    int local_result = 0;
    LL local_checked = 0;

    WaitForSingleObject(start_sem, INFINITE);

    while (1)
    {
        LL task_index;

        EnterCriticalSection(&task_cs);

        if (global_next_task_index >= global_count_combination)
        {
            LeaveCriticalSection(&task_cs);
            break;
        }

        task_index = global_next_task_index;
        global_next_task_index++;

        LeaveCriticalSection(&task_cs);

        if (check_combination(task_index))
            local_result++;

        local_checked++;
    }

    EnterCriticalSection(&task_cs);
    global_result += local_result;
    LeaveCriticalSection(&task_cs);

    global_checked[idx] = local_checked;
    global_found[idx] = local_result;

    printf("Поток %d завершился: проверено %lld, найдено %d\n",
        idx, local_checked, local_result);

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

    int count_threads = 0;

    fscanf(file, "%d", &count_threads);
    fscanf(file, "%d", &global_count_An);

    global_Array_An = (int*)malloc(global_count_An * sizeof(int));
    

    for (int i = 0; i < global_count_An; i++)
    {
        fscanf(file, "%d", &global_Array_An[i]);
    }

    fscanf(file, "%lld", &global_S);
    fclose(file);

    if (!init_combination())
    {
        free(global_Array_An);
        return 1;
    }

    InitializeCriticalSection(&task_cs);

    start_sem = CreateSemaphore(NULL, 0, count_threads, NULL);

    HANDLE* threads = (HANDLE*)malloc(count_threads * sizeof(HANDLE));

    for (int i = 0; i < count_threads; i++)
    {
        threads[i] = CreateThread(NULL, 0, worker_thread, (LPVOID)(size_t)i, 0, NULL);
    }

    clock_t start_time = clock();

    ReleaseSemaphore(start_sem, count_threads, NULL);

    while (1)
    {
        DWORD res = WaitForMultipleObjects(count_threads, threads, TRUE, 1000);
        if (res != WAIT_TIMEOUT)
            break;

        EnterCriticalSection(&task_cs);
        //printf("Прогресс: %lld / %lld\n", global_next_task_index, global_count_combination);
        LeaveCriticalSection(&task_cs);
    }

    clock_t end_time = clock();
    double elapsed_ms = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;

    printf("\n--- Статистика потоков ---\n");
    for (int i = 0; i < count_threads; i++)
    {
        printf("Поток %d: проверено %lld, найдено %lld\n",
            i, global_checked[i], global_found[i]);
    }
    printf("Итого решений: %d\n", global_result);
    printf("Время: %.3f мс\n", elapsed_ms);

    FILE* out = fopen("output.txt", "w");
    if (out != NULL)
    {
        fprintf(out, "%d\n", count_threads);
        fprintf(out, "%d\n", global_count_An);
        fprintf(out, "%d\n", global_result);
        fclose(out);
    }
    else
    {
		printf("Не удалось открыть output.txt для записи\n");
        fclose(out);
    }


    FILE* t = fopen("time.txt", "w");
    if (t != NULL)
    {
        fprintf(t, "%.3f", elapsed_ms);
        fclose(t);
    }
    else
    {
        printf("Не удалось открыть time.txt для записи\n");
        fclose(t);
    }

    for (int i = 0; i < count_threads; i++)
    {
        CloseHandle(threads[i]);
    }

    free(threads);
    CloseHandle(start_sem);
    DeleteCriticalSection(&task_cs);
    free(global_Array_An);
    return 0;
}