#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#define COUNT_VERTEX 41
#define MAX_DAY 20074
void main_algorithm_1(); // прямой ход
void main_algorithm_2(); // обратный ход
int done_vertex_array1[COUNT_VERTEX] = { 0 }; // массив для отслеживания выполненных вершин в прямом ходе
int done_vertex_array2[COUNT_VERTEX] = { 0 }; // массив для отслеживания выполненных вершин в обратном ходе

typedef struct Description
{
    int number_vertex;
    int day;
	int array_before_vertex[COUNT_VERTEX]; // массив для хранения вершин, от которых зависит текущая вершина
	int array_after_vertex[COUNT_VERTEX]; // массив для хранения вершин, которые зависят от текущей вершины
    int count_before_vertex;
    int count_after_vertex;
    int ebeg;
    int efin;
    int lbeg;
    int lfin;

} Description;

Description vertex_array[COUNT_VERTEX];

void printf_result()
{
    FILE* out = fopen("job_Var7.out", "w");
    if (out == NULL)
    {
        printf("Cannot create output file\n");
        return;
    }

    int max_efin = 0;
    for (int i = 0; i < COUNT_VERTEX; i++)
    {
        if (vertex_array[i].efin > max_efin)
        {
            max_efin = vertex_array[i].efin;
        }
    }
    printf("%d\n", max_efin);
    fprintf(out, "%d\n", max_efin);

    for (int num = 1; num <= COUNT_VERTEX; num++)
    {
        for (int i = 0; i < COUNT_VERTEX; i++)
        {
            if (vertex_array[i].number_vertex == num)
            {
                printf("%d: %d, %d, %d, %d, %d\n",
                    vertex_array[i].number_vertex,
                    vertex_array[i].ebeg,
                    vertex_array[i].efin,
                    vertex_array[i].lbeg,
                    vertex_array[i].lfin,
                    vertex_array[i].lbeg - vertex_array[i].ebeg);

                fprintf(out, "%d: %d, %d, %d, %d, %d\n",
                    vertex_array[i].number_vertex,
                    vertex_array[i].ebeg,
                    vertex_array[i].efin,
                    vertex_array[i].lbeg,
                    vertex_array[i].lfin,
                    vertex_array[i].lbeg - vertex_array[i].ebeg);
            }
        }
    }

    fclose(out);
}

void find_after_vertex(Description* vertex) // функция для поиска вершин, которые зависят от текущей вершины, и заполнения массива array_after_vertex
{
    int number = vertex->number_vertex;
    int count = 0;
    for (int i = 0; i < COUNT_VERTEX; i++)
    {
        for (int j = 0; j < vertex_array[i].count_before_vertex; j++)
        {
            if (vertex_array[i].array_before_vertex[j] == number)
            {
                vertex->array_after_vertex[count] = vertex_array[i].number_vertex;
                count++;
                break;
            }
        }
    }
    vertex->count_after_vertex = count;
}

int main()
{

    setlocale(LC_ALL, "Russian");
    FILE* file = fopen("job_Var7.in", "r");
    if (file == NULL)
    {
        printf("No find file\n");
        return 1;
    }
    char buffer[1024]; // пропустили
    fgets(buffer, sizeof(buffer), file);

    for (int i = 0; i < COUNT_VERTEX; i++) // проходимся по вершинам (строкам)
    {
        fscanf(file, "%d", &vertex_array[i].number_vertex); // номер вершины
        fscanf(file, "%d", &vertex_array[i].day); // день

        int index = 0;

        int c;
        while ((c = fgetc(file)) == ' '); // пропускаем пробелы
        if (c == '*')
        {
            while (1)
            {
                int ch = fgetc(file);
                if (ch == '\n' || ch == EOF) break;
            }
        }
		else // ищем тех от кого зависит вершина
        {
            ungetc(c, file);
            while (1) // массив после вершины
            {
                fscanf(file, "%d", &vertex_array[i].array_before_vertex[index]);
                index++;
                do {
                    c = fgetc(file);
                } while (c == ' ');

                if (c == '\n' || c == '\r' || c == EOF) break;

                // это была цифра — возвращаем её обратно
                ungetc(c, file);
            }
        }
        vertex_array[i].count_before_vertex = index;
    }

	for (int i = 0; i < COUNT_VERTEX; i++) // запускаем поиск тех, кто зависит от вершины, для каждой вершины
    {
		find_after_vertex(&vertex_array[i]);
    }

   
    while (1)
    {
        main_algorithm_1();
        int done_count = 0;
        for (int i = 0; i < COUNT_VERTEX; i++)
        {
            if (done_vertex_array1[i] == 1) done_count++;
        }
        //printf("Прямой ход: посчитано %d из %d\n", done_count, COUNT_VERTEX);

        if (done_count == COUNT_VERTEX) break;

    }
    while (1)
    {
        main_algorithm_2();
        int done_count = 0;
        for (int i = 0; i < COUNT_VERTEX; i++)
        {
            if (done_vertex_array2[i] == 1)
            {
                done_count++;
            }
        }
        if (done_count == COUNT_VERTEX)
        {
            break;
        }
    }

    printf_result();
    fclose(file);
    return 0;
}



void find_without_before_vertex()
{
    for (int i = 0; i < COUNT_VERTEX; i++)
    {
        if (vertex_array[i].count_before_vertex == 0)
        {
            vertex_array[i].ebeg = 0;
            vertex_array[i].efin = vertex_array[i].day;
            done_vertex_array1[i] = 1; // помечаем вершину как выполненную
        }
    }
}


void find_ebeg_efin(Description* vertex) // ищем ebeg и efin для вершины, которая имеет зависимости
{
    int max_efin = 0;
    for (int i = 0; i < vertex->count_before_vertex; i++) // проходимся по массиву зависимостей вершины
    {
        int before = vertex->array_before_vertex[i]; // номер вершины, от которой зависит текущая вершина
        for (int j = 0; j < COUNT_VERTEX; j++) // проходимся по всем вершинам, чтобы найти вершину, от которой зависит текущая вершина
        {
            if (vertex_array[j].number_vertex == before)
            {
                if (vertex_array[j].efin > max_efin)
                {
                    max_efin = vertex_array[j].efin; // обновляем efin, если он больше текущего максимального

                }
                break;
            }
        }
    }
    vertex->ebeg = max_efin;
    vertex->efin = max_efin + vertex->day;
}

void main_algorithm_1() //  прямой ход
{
    find_without_before_vertex();
    for (int i = 0; i < COUNT_VERTEX; i++) // проходимся по вершинам
    {
        if (vertex_array[i].count_before_vertex > 0 && done_vertex_array1[i] == 0) // проверяем только те, у которых есть зависимости и которые еще не выполнены
        {
            int counter = 0; // счетчик для проверки, выполнены ли все зависимости

            for (int j = 0; j < vertex_array[i].count_before_vertex; j++) // проходимся по массиву зависимостей текущей вершины
            {
                for (int k = 0; k < COUNT_VERTEX; k++) // проходимся по массиву done для проверки, выполнена ли зависимость
                {
                    if (done_vertex_array1[k] == 1 && vertex_array[k].number_vertex == vertex_array[i].array_before_vertex[j])
                    {
                        counter++;
                        break;
                    }
                }
            }
            if (counter == vertex_array[i].count_before_vertex)
            {
                find_ebeg_efin(&vertex_array[i]);
                done_vertex_array1[i] = 1; // помечаем вершину как выполненную
            }
        }
    }
}



void find_without_after_vertex()
{
    for (int i = 0; i< COUNT_VERTEX; i++)
    {
        if (vertex_array[i].array_after_vertex[0] == 0) // если массив после вершины пустой, значит от нее никто не зависит
        {
            vertex_array[i].lfin = MAX_DAY;
            vertex_array[i].lbeg = vertex_array[i].lfin - vertex_array[i].day;
            done_vertex_array2[i] = 1; // помечаем вершину как выполненную
        }
	}
}


void find_lbeg_lfin(Description* vertex)
{
    int lbeg_min = MAX_DAY;
    for (int i = 0; i < vertex->count_after_vertex; i++)
    {
        int after_number = vertex->array_after_vertex[i];
        for (int j = 0; j < COUNT_VERTEX; j++)
        {
            if (vertex_array[j].number_vertex == after_number)
            {
                if (vertex_array[j].lbeg < lbeg_min)
                {
                    lbeg_min = vertex_array[j].lbeg;
                }
                break;
            }
        }
    }
    vertex->lfin = lbeg_min;
    vertex->lbeg = vertex->lfin - vertex->day;
}

void main_algorithm_2() // обратный ход
{
    find_without_after_vertex();
    for (int i = 0; i < COUNT_VERTEX; i++)
    {
        if (vertex_array[i].count_after_vertex > 0 && done_vertex_array2[i] == 0)
        {
            int counter = 0;
            for (int j = 0; j < vertex_array[i].count_after_vertex; j++)
            {
                int after_number = vertex_array[i].array_after_vertex[j];
                // ищем последователя по номеру
                for (int k = 0; k < COUNT_VERTEX; k++)
                {
                    if (vertex_array[k].number_vertex == after_number)
                    {
                        if (done_vertex_array2[k] == 1)
                        {
                            counter++;
                        }
                        break;
                    }
                }
            }
            if (counter == vertex_array[i].count_after_vertex)
            {
                find_lbeg_lfin(&vertex_array[i]);
                done_vertex_array2[i] = 1;
            }
        }
    }
}