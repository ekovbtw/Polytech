#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>
#include <locale.h>
#define LL unsigned long long
int count_threads = 0; // кол-во потоков
int N = 0; // размер доски
int L = 0; // количество ладьей
int K = 0; // количество поставленных изначально
typedef struct KOOR
{
	int x;
	int y;
} KOOR;
KOOR* k_array = NULL;

void readfile()
{
	FILE* file = fopen("input.txt", "r");
	if (file == NULL) return;
	fscanf(file, "%d", &count_threads);
	
	fscanf(file, "%d", &N);
	fscanf(file, "%d", &L);
	fscanf(file, "%d", &K);
	k_array = (KOOR*)malloc(sizeof(KOOR) * K);
	for (int i = 0; i < K; i++)
	{
		fscanf(file, "%d", &k_array[i].x);
		fscanf(file, "%d", &k_array[i].y);
	}
	fclose(file);
}
int main()
{
	readfile();
	int* free_koor_str = NULL;
	int* free_koor_col = NULL;
	free_koor_str = (int*)malloc(sizeof(int) * N);
	free_koor_col = (int*)malloc(sizeof(int) * N);
	return 0;
}