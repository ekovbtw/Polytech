#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main()
{
    int threads = 1;
    int n = 25; // 1 миллион элементов

    FILE* f = fopen("input.txt", "w");
    fprintf(f, "%d\n", threads);
    fprintf(f, "%d\n", n);

    srand((unsigned)time(NULL));
    for (int i = 0; i < n; i++)
    {
        fprintf(f, "%d ", rand());
    }
    fclose(f);
    return 0;
}