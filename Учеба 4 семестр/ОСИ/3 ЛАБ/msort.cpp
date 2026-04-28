#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
typedef struct
{
    int left;
    int right;
    bool mode[2]; // 1 - normal task 2 - ready for merge 
} Task;


/* declaration function */
void add_task(int left, int right);
Task get_task();
void* worker_thread(void* param);
void output_to_file();

int count_add_task = 0; // количество вызовов функции добавления задач
int current_size_buffer_array = 2; // first size for buffer_array
int global_count_threads = 0; // количество потоков
int N = 0; // количество элементов массива 
int* global_array = NULL; // массив где хранятся числа для быстрой сортировки


pthread_mutex_t mutex; // мьютекс для синхронизации доступа к глобальному массиву
pthread_cond_t cond;// cond for complete task
pthread_cond_t done_cond; // событие для уведомления о завершении работы потоков
volatile int active_threads = 0; // количество активных потоков


Task* task_array = NULL; // очередь задач для сортировки
volatile int head = 0; // индекс головы очереди задач
volatile int tail = 0; // индекс хвоста очереди задач
volatile int count_task_array = 0; // счетчик задач в массиве задач 
volatile bool merge_done = false; // if true ==> work is complete

void close_all(pthread_t* threads, int count_threads_created) // iz Qsort
{
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    pthread_cond_destroy(&done_cond);
    free(global_array);
    free(task_array);
    free(threads);
}


void update_memory(int count) // iz Qsort
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

    struct timespec start_time, end_time;

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
    fclose(file);

    pthread_cond_init(&done_cond, NULL); // создание события для уведомления потоков о том, появилась задача
    pthread_cond_init(&cond, NULL); // создание события для уведомления о завершении работы потоков
    pthread_mutex_init(&mutex, NULL); // создание мьютекса для синхронизации доступа к глобальному массиву


    int count_threads_created = 0; // счетчик созданных потоков
    pthread_t* threads = (pthread_t*)malloc(global_count_threads * sizeof(pthread_t));
    if (threads == NULL)
    {
        printf("Не удалось выделить память под потоки\n");
        close_all(threads, count_threads_created);
        return 1;
    }

    for (int i = 0; i < global_count_threads; i++)
    {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
        count_threads_created++;
    }
    clock_gettime(CLOCK_MONOTONIC, &start_time); // шаг 7 - начало отсчета времени
    for (current_size_buffer_array;current_size_buffer_array / 2 < N;)
    {
        head = 0;
        tail = 0;
        count_add_task = 0;
        int counter = N / 2;
        for (int i = 0; i < N; i += current_size_buffer_array)
        {
            if (i + current_size_buffer_array - 1 <= N - 1)
            {
                add_task(i, i + current_size_buffer_array - 1);
            }
            else
            {
                add_task(i, N - 1);
            }
        }
        pthread_mutex_lock(&mutex);
        pthread_cond_broadcast(&cond);
        while (active_threads > 0 || head != tail)
        {
            pthread_cond_wait(&done_cond, &mutex);
        }
        pthread_mutex_unlock(&mutex);
        current_size_buffer_array *= 2;
    }
    merge_done = true;
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < global_count_threads; i++) // wait full stop active threads
    {
        pthread_join(threads[i], NULL);
    }

    //WaitForMultipleObjects(global_count_threads, threads, true, INFINITE); // ждем завершения всех потоков, если они не завершились, то завершаем их принудительно
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    output_to_file();
    close_all(threads, count_threads_created);


    FILE* file_time = fopen("time.txt", "w");
    if (file_time == NULL)
    {
        printf("Не удалось открыть time.txt\n");
        return 1;
    }
    //double time_taken = (double)(end_time - start_time) / CLOCKS_PER_SEC; // шаг 7 - расчет времени
    //fprintf(file_time, "%f", time_taken);
    unsigned long long diff = (end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
    fprintf(file_time, "%llu", diff);
    fclose(file_time);
    return 0;
}



bool need_doubling = false;

void doubling_buffer_size() // doubling buff_array
{
    current_size_buffer_array *= 2;
}


void merge_sort_2(int left, int right, int mid, int size) // step C4
{
    if (left >= right)
    {
        printf("left>=right\n");
        return;
    }

    if (size <= 2)
    {
        if (global_array[left] > global_array[right])
        {
            int t = global_array[left];
            global_array[left] = global_array[right];
            global_array[right] = t;
        }
    }

    else if (size > 2)
    {
        int size_temp = right - left + 1;
        int* temp = (int*)malloc((size_temp) * sizeof(int));
        int idx = 0;
        int i = left;
        int j = mid + 1;
        for (i = left, j = mid + 1; i <= mid && j <= right; )
        {
            if (global_array[i] < global_array[j])
            {
                temp[idx] = global_array[i];
                i++;
                idx++;
            }
            else if (global_array[i] > global_array[j])
            {
                temp[idx] = global_array[j];
                j++;
                idx++;
            }
            else
            {
                temp[idx] = global_array[i];
                i++;
                idx++;
                temp[idx] = global_array[j];
                j++;
                idx++;
            }
        }

        for (i; i <= mid; i++)
        {
            temp[idx] = global_array[i];
            idx++;
        }
        for (j; j < right + 1; j++)
        {
            temp[idx] = global_array[j];
            idx++;
        }


        int index = 0;
        for (int k = left; k < right + 1; k++)
        {
            global_array[k] = temp[index];
            index++;
        }
        free(temp);
    }

}




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
    task_array[index].mode[0] = true;  // norm
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
        buf.mode[0] = false;
        return buf;
    }
    Task current = task_array[head];
    current.mode[0] = true;
    head++;
    return current;
}




int ret_index_pivot(int index_pivot, int left, int right) // musor
{
    int t = 0;
    t = global_array[right];
    global_array[right] = global_array[index_pivot];
    global_array[index_pivot] = t;
    index_pivot = right;

    int result = left;
    for (int i = left; i < right; i++)
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


void qsortir(int left, int right) // musor
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

void destroy_task(int left, int right) // musor
{
    int mid = left + (right - left) / 2;
    int idx_pivot = ret_index_pivot(mid, left, right);
    //WaitForSingleObject(mutex, INFINITE);
    add_task(left, idx_pivot - 1);
    add_task(idx_pivot + 1, right);
    //ReleaseMutex(mutex);
    //SetEvent(event);
    //SetEvent(event);
}



void* worker_thread(void* param) // step C5
{
    while (!merge_done)
    {
        pthread_mutex_lock(&mutex);
        while (head == tail && !merge_done)
        {
            pthread_cond_wait(&cond, &mutex);
        }
        if (merge_done)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        Task buff = get_task(); // берем задачу
        bool complete = false;
        if (buff.mode[0] == true) // если задача нормальная
        {
            active_threads++;
            int size = current_size_buffer_array;
            int mid = buff.left + size / 2 - 1;
            if (mid >= buff.right) mid = buff.right - 1;
            pthread_mutex_unlock(&mutex);
            int counter = buff.right - buff.left;
            merge_sort_2(buff.left, buff.right, mid, size);

            complete = true;
        }
        if (buff.mode[0] == false)
        {
            pthread_cond_wait(&cond, &mutex);
            complete = false;
            pthread_mutex_unlock(&mutex);
        }
        if (complete) // если все прошло
        {
            pthread_mutex_lock(&mutex);
            active_threads--;
            if (head == tail && active_threads == 0) // проверка на конец
            {
                pthread_cond_signal(&done_cond);
            }
            pthread_mutex_unlock(&mutex);
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