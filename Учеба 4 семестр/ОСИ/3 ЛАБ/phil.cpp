#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
struct timespec start_time;

#define CONTROL 1
#define UN_CONTROL 0
#define EAT 1
#define SLEEP 0

#define FREE 1
#define BUSY 0 

#define COUNT_PHIL 5

#define STOP_PROGRAMM 0

static volatile int global_count_fork[5] = { FREE, FREE, FREE, FREE, FREE };
volatile int running = 1;

typedef struct
{
    bool was_control;
    bool was_do;
    int phil_fork_count;
    long wait_since; // D7 - время когда начал ждать
    sem_t phil_sema;
    bool has_forks;

}PHIL_struct;

PHIL_struct phil_array[5];
PHIL_struct control_phil;
sem_t control_sema;

void clear_phil_struct()
{
    control_phil.was_control = CONTROL;
    for (int i = 0; i < COUNT_PHIL; i++)
    {
        phil_array[i].phil_fork_count = 0;
        phil_array[i].was_do = SLEEP;
        phil_array[i].was_control = UN_CONTROL;
        phil_array[i].wait_since = 0; // D7
        phil_array[i].has_forks = false;
    }
}

#define TOTAL 0
#define PHIL 1
int array_for_total_and_phil[2] = { 0 };

void input_to_array_for_total_and_phil(int total, int phil)
{
    array_for_total_and_phil[TOTAL] = total;
    array_for_total_and_phil[PHIL] = phil;
}
sem_t print_sema;
void close_all(pthread_t* threads_phil, pthread_t* threads_control)
{
    pthread_join(*threads_control, NULL);
    for (int i = 0; i < COUNT_PHIL; i++)
    {
        pthread_join(threads_phil[i], NULL);
        sem_destroy(&phil_array[i].phil_sema);
    }
    sem_destroy(&control_phil.phil_sema);
    free(threads_phil);
    free(threads_control);
    sem_destroy(&control_sema);
    sem_destroy(&print_sema);
}


void print_state(int num, char old_state, char new_state)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed = (now.tv_sec - start_time.tv_sec) * 1000 +
        (now.tv_nsec - start_time.tv_nsec) / 1000000;
    sem_wait(&print_sema);
    printf("%ld:%d:%c->%c\n", elapsed, num, old_state, new_state);
    sem_post(&print_sema);
}

void* worker_control(void* param)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed = (now.tv_sec - start_time.tv_sec) * 1000 +
        (now.tv_nsec - start_time.tv_nsec) / 1000000;
    control_phil.was_do = SLEEP;
    while (elapsed < array_for_total_and_phil[TOTAL])
    {
        sem_wait(&control_sema);
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (now.tv_sec - start_time.tv_sec) * 1000 +
            (now.tv_nsec - start_time.tv_nsec) / 1000000;
        int count_eat = 0;
        int eat_ar[COUNT_PHIL] = { 0 };
        long wait_ar[COUNT_PHIL] = { 0 };
        int idx = 0;
        for (int i = 0; i < COUNT_PHIL; i++)
        {
            sem_wait(&control_phil.phil_sema);
            if (phil_array[i].was_do == EAT)
            {
                eat_ar[idx] = i;
                wait_ar[idx] = phil_array[i].wait_since;
                idx++;
                count_eat++;
            }
            else if (phil_array[i].was_do == SLEEP && phil_array[i].has_forks)
            {
                global_count_fork[i] = FREE;
                global_count_fork[(i + 1) % COUNT_PHIL] = FREE;
                phil_array[i].has_forks = false;
            }
            sem_post(&control_phil.phil_sema);
        }
        for (int i = 0; i < count_eat - 1; i++)
        {
            for (int j = i + 1; j < count_eat; j++)
            {
                if (wait_ar[j] < wait_ar[i])
                {
                    long tmp_w = wait_ar[i];
                    wait_ar[i] = wait_ar[j];
                    wait_ar[j] = tmp_w;
                    int tmp_e = eat_ar[i];
                    eat_ar[i] = eat_ar[j];
                    eat_ar[j] = tmp_e;
                }
            }
        }
        for (int i = 0; i < count_eat; i++)
        {
            int index = eat_ar[i];
            if (global_count_fork[index] == FREE &&
                global_count_fork[(index + 1) % COUNT_PHIL] == FREE)
            {
                global_count_fork[index] = BUSY;
                global_count_fork[(index + 1) % COUNT_PHIL] = BUSY;
                phil_array[index].has_forks = true;
                sem_post(&phil_array[index].phil_sema);
            }
        }
    }
    running = 0;
    for (int i = 0; i < COUNT_PHIL; i++)
    {
        sem_post(&phil_array[i].phil_sema);
    }
    return 0;
}

void* worker_phil(void* param)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int idx = (int)(size_t)param;
    long elapsed = (now.tv_sec - start_time.tv_sec) * 1000 +
        (now.tv_nsec - start_time.tv_nsec) / 1000000;
    while (elapsed < array_for_total_and_phil[TOTAL])
    {
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (now.tv_sec - start_time.tv_sec) * 1000 +
            (now.tv_nsec - start_time.tv_nsec) / 1000000;
        int idx = (int)(size_t)param;

        struct timespec ts;
        ts.tv_sec = array_for_total_and_phil[PHIL] / 1000;
        ts.tv_nsec = (array_for_total_and_phil[PHIL] % 1000) * 1000000;

        sem_wait(&control_phil.phil_sema);
        phil_array[idx].was_do = SLEEP;
        sem_post(&control_phil.phil_sema);
        nanosleep(&ts, NULL);

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (now.tv_sec - start_time.tv_sec) * 1000 +
            (now.tv_nsec - start_time.tv_nsec) / 1000000;
        if (elapsed > array_for_total_and_phil[TOTAL]) break;

        sem_wait(&control_phil.phil_sema);
        phil_array[idx].was_do = EAT;
        phil_array[idx].wait_since = elapsed; // D7
        sem_post(&control_phil.phil_sema);
        print_state(idx + 1, 'T', 'E');
        sem_post(&control_sema);

        sem_wait(&phil_array[idx].phil_sema);
        if (!running)
        {
            print_state(idx + 1, 'E', 'T');
            phil_array[idx].was_do = SLEEP;
            break;
        }

        nanosleep(&ts, NULL);

        sem_wait(&control_phil.phil_sema);
        phil_array[idx].was_do = SLEEP;
        sem_post(&control_phil.phil_sema);
        print_state(idx + 1, 'E', 'T');
        sem_post(&control_sema);

    }
    sem_wait(&control_phil.phil_sema);
    bool was_eating = (phil_array[idx].was_do == EAT);
    sem_post(&control_phil.phil_sema);
    if (was_eating)
    {
        print_state(idx + 1, 'E', 'T');
    }
    sem_post(&control_sema);
    return 0;
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "Russian");
    clear_phil_struct();
    if (argc != 3)
    {
        printf("Usage: phil TOTAL PHIL\n");
        return 1;
    }
    int total = atoi(argv[1]);
    int phil = atoi(argv[2]);
    input_to_array_for_total_and_phil(total, phil);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    pthread_t* threads_phil = (pthread_t*)malloc(COUNT_PHIL * sizeof(pthread_t));
    if (threads_phil == NULL)
    {
        printf("Не удалось выделить память под потоки\n");
        return 1;
    }

    pthread_t* threads_control = (pthread_t*)malloc(1 * sizeof(pthread_t));
    if (threads_control == NULL)
    {
        printf("Не удалось выделить память под потоки\n");
        free(threads_phil);
        return 1;
    }

    for (int i = 0; i < COUNT_PHIL; i++)
    {
        sem_init(&phil_array[i].phil_sema, 0, 0);
    }
    sem_init(&control_phil.phil_sema, 0, 1);
    sem_init(&control_sema, 0, 0);
    sem_init(&print_sema, 0, 1);

    pthread_create(threads_control, NULL, worker_control, NULL);
    for (int i = 0; i < COUNT_PHIL; i++)
    {
        pthread_create(&threads_phil[i], NULL, worker_phil, (void*)(size_t)i);
    }
    close_all(threads_phil, threads_control);
    return 0;
}