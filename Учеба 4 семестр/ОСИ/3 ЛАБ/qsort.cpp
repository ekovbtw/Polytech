#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>
typedef struct
{
    int left; // левая граница подмассива (index)
    int right; // правая граница подмассива (index)
    bool normal; // ДОБАВИЛ НА 3 ШАГЕ (для того чтобы вернуть не норм, если очередь пустая)
} Task;



void add_task(int left, int right);
Task get_task();
DWORD WINAPI worker_thread(void* param);
void output_to_file(); // шаг 7


int global_count_threads = 0; // количество потоков
int N = 0; // количество элементов массива 
int* global_array = NULL; // массив где хранятся числа для быстрой сортировки


HANDLE mutex; // мьютекс для синхронизации доступа к глобальному массиву
HANDLE event; // событие для уведомления потоков о том, что массив отсортирован
HANDLE done_event; // событие для уведомления о завершении работы потоков
volatile int active_threads = 0; // количество активных потоков


Task* task_array = NULL; // очередь задач для сортировки
volatile int head = 0; // индекс головы очереди задач
volatile int tail = 0; // индекс хвоста очереди задач
volatile int count_task_array = 0; // счетчик задач в массиве задач 


void close_all(HANDLE* threads, int count_threads_created) // шаг 4 добавил
{
    CloseHandle(mutex);
    CloseHandle(event);
    CloseHandle(done_event);
    free(global_array);
    free(task_array);
    for (int i = 0; i < count_threads_created; i++) // шаг 7
    {
        CloseHandle(threads[i]);
    }
    free(threads);
}


void update_memory(int count)
{
    Task* buff = (Task*)realloc(task_array, count * sizeof(Task));
    if (buff == NULL)
    {
        // обработка ошибки — старый task_array остался целым
        printf("Не удалось перевыделить память\n");
        return;
    }
    task_array = buff;
}


int main()
{
    setlocale(LC_ALL, "Russian");
    FILE* file = fopen("input.txt", "r");

    //clock_t start_time; // шаг 7
    //clock_t end_time;
    ULONGLONG  start_time = 0;
    ULONGLONG  end_time = 0;

    if (file == NULL)
    {
        printf("Не удалось открыть input.txt\n");
        return 1;
    }

    fscanf(file, "%d", &global_count_threads);
    fscanf(file, "%d", &N);
    global_array = (int*)malloc(N * sizeof(int)); // массив, выделение памяти
    if (global_array == NULL)
    {
        printf("Не удалось выделить память под массив чисел\n");
        fclose(file);
        return 1;
    }

    task_array = (Task*)malloc(N * sizeof(Task)); // очередь задач для сортировки
    if (task_array == NULL)
    {
        printf("Не удалось выделить память под очередь задач\n");
        free(global_array);
        fclose(file);
        return 1;
    }
    count_task_array = N;

    for (int i = 0; i < N; i++) // сканируем числа для быстрой сортировки
    {
        fscanf(file, "%d", &global_array[i]);
    }
    fclose(file); // поменял в шаге 7

    event = CreateEvent(NULL, FALSE, FALSE, NULL); // создание события для уведомления потоков о том, появилась задача
    done_event = CreateEvent(NULL, TRUE, FALSE, NULL); // создание события для уведомления о завершении работы потоков
    mutex = CreateMutex(NULL, FALSE, NULL); // создание мьютекса для синхронизации доступа к глобальному массиву

    task_array[0].left = 0; // добавление первой задачи
    task_array[0].right = N - 1;
    task_array[0].normal = true;
    tail++;

    int count_threads_created = 0; // счетчик созданных потоков
    HANDLE* threads = (HANDLE*)malloc(global_count_threads * sizeof(HANDLE)); // создаем потоки
    if (threads == NULL)
    {
        printf("Не удалось выделить память под потоки\n");
        close_all(threads, count_threads_created);
        return 1;
    }


    for (int i = 0; i < global_count_threads; i++) // передаем их в функцию 
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

            close_all(threads, count_threads_created);
            return 1;
        }
        count_threads_created++;
    }
    start_time = GetTickCount64(); // шаг 7 - начало отсчета времени
    SetEvent(event);


    WaitForSingleObject(done_event, INFINITE); // шаг 7 ждем завершения сортировки
    WaitForMultipleObjects(global_count_threads, threads, true, INFINITE); // ждем завершения всех потоков, если они не завершились за 200 мс, то завершаем их принудительно
    end_time = GetTickCount64(); // шаг 7 - конец отсчета времени

    output_to_file(); // шаг 7 - вывод в файл
    close_all(threads, count_threads_created); // закрываем все дескрипторы и освобождаем память


    FILE* file_time = fopen("time.txt", "w"); // шаг 7 - открываем файл для записи времени
    if (file_time == NULL)
    {
        printf("Не удалось открыть time.txt\n");
        return 1;
    }
    //double time_taken = (double)(end_time - start_time) / CLOCKS_PER_SEC; // шаг 7 - расчет времени
    //fprintf(file_time, "%f", time_taken);
    fprintf(file_time, "%llu", end_time - start_time);
    fclose(file_time);
    return 0;
}


int count_add_task = 0; // количество вызовов функции добавления задач


void add_task(int left, int right) // функция добавления задачи в массив
{
    count_add_task++;
    if (count_task_array < count_add_task) // проверка есть ли место
    {
        update_memory(count_add_task);
    }
    int index = tail; // выбираем последнее место для задачи
    task_array[index].left = left;
    task_array[index].right = right;
    task_array[index].normal = true; // обновил в шаге 4 
    tail++; // сдвиг хвоста
}


Task get_task() // функция получения задачи потоком
{
    if (head == tail)
    {
        printf("Очередь пуста\n");
        Task buf;
        buf.left = 0;
        buf.right = 0;
        buf.normal = false;
        return buf;
    }
    Task current = task_array[head];
    current.normal = true;
    head++;
    return current;
}

int ret_index_pivot(int index_pivot, int left, int right) // шаг 6 (возврат итоговой позиции pivot) 
{
    int t = 0; // меняем местами опорный элемент с последним
    t = global_array[right];
    global_array[right] = global_array[index_pivot];
    global_array[index_pivot] = t;
    index_pivot = right;

    int result = left;
    for (int i = left; i < right; i++) // проходимся по массиву и смотрим кто меньше pivot
    {
        if (global_array[i] < global_array[index_pivot])
        {
            t = global_array[i];
            global_array[i] = global_array[result];
            global_array[result] = t;
            result++;
        }
    }

    t = global_array[result];
    global_array[result] = global_array[right];
    global_array[right] = t;
    return result;
}


void qsortir(int left, int right)
{
    if (left >= right)
    {
        return;
    }
    int mid = left + (right - left) / 2;
    int pivot = mid;
    int index_pivot = ret_index_pivot(pivot, left, right);
    qsortir(left, index_pivot - 1);
    qsortir(index_pivot + 1, right);
}

void destroy_task(int left, int right) // если массив большой - его нужно разделить на два подмассива (шаг 6) 
{
    int mid = left + (right - left) / 2;
    int idx_pivot = ret_index_pivot(mid, left, right);
    WaitForSingleObject(mutex, INFINITE);
    add_task(left, idx_pivot - 1);
    add_task(idx_pivot + 1, right);
    ReleaseMutex(mutex);
    SetEvent(event);
    SetEvent(event);
}

DWORD WINAPI worker_thread(void* param) // шаг 4 - 5
{
    while (1)
    {
        WaitForSingleObject(mutex, INFINITE); // берем мьютекс
        Task buff = get_task(); // берем задачу
        bool complete = false;
        if (buff.normal == true) // если задача нормальная
        {
            active_threads++;
            ReleaseMutex(mutex);
            /*потом вызов функции qsort*/
            int counter = buff.right - buff.left;
            if (counter > 10000) // шаг 6    
            {
                destroy_task(buff.left, buff.right);
            }
            else
            {
                qsortir(buff.left, buff.right);
            }
            complete = true;
        }
        if (buff.normal == false)
        {
            ReleaseMutex(mutex);
            WaitForSingleObject(event, 50); // каждые 50 мс проверяем есть ли задача 
            complete = false;
        }
        if (complete) // если все прошло
        {
            WaitForSingleObject(mutex, INFINITE);
            active_threads--;
            if (head == tail && active_threads == 0) // проверка на конец
            {
                SetEvent(done_event);
            }
            ReleaseMutex(mutex);
        }
        if (WaitForSingleObject(done_event, 0) == WAIT_OBJECT_0)
        {
            break;
        }
    }
    return 0;
}

void output_to_file() // шаг 7 - вывод в файл
{
    FILE* file = fopen("output.txt", "w");
    if (file == NULL)
    {
        printf("Не удалось открыть output.txt\n");
        return;
    }
    fprintf(file, "%d\n", global_count_threads);
    fprintf(file, "%d\n", N);
    for (int i = 0; i < N; i++)
    {
        fprintf(file, "%d ", global_array[i]);
    }
    fclose(file);
}