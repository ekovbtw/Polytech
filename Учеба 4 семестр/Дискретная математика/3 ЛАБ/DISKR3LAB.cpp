#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#define COUNT_VERTEX 9
void main_algorithm_1(); // прямой ход
void main_algorithm_2(); // обратный ход
int done_vertex_array[COUNT_VERTEX] = { 0 }; // массив для отслеживания выполненных вершин

typedef struct Description
{
	int number_vertex;
    int day; 
	int array_before_vertex[COUNT_VERTEX];
    int count_before_vertex;
    int ebeg;
    int efin;
    int lbeg;
	int lfin;

} Description;

Description vertex_array[COUNT_VERTEX];

void printf_result()
{
	int max_efin = 0;
    for (int i = 0; i < COUNT_VERTEX; i++)
    {
        if (vertex_array[i].efin > max_efin)
        {
            max_efin = vertex_array[i].efin;
        }
    }
	printf("%d\n", max_efin);


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
                    vertex_array[i].lbeg - vertex_array[i].ebeg);  // reserve
            }
        }
    }
}

int main()
{
	setlocale(LC_ALL, "Russian");
    FILE* file = fopen("Plan.in", "r");
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
            while (fgetc(file) != '\n'); // дочитываем строку до конца
        }
        else
        {
            ungetc(c, file); 
            while (1) // массив после вершины
            {
                fscanf(file, "%d", &vertex_array[i].array_before_vertex[index]);
                index++;
                c = fgetc(file);
                if (c == '\n' || c == EOF) // если конец строки или файла, выходим
                {
                    break;
				}
            }
        }

        vertex_array[i].count_before_vertex = index;
    }

    while (1)
    {
        main_algorithm_1();
        int done_count = 0;
        for (int i = 0; i < COUNT_VERTEX; i++)
        {
            if (done_vertex_array[i] == 1)
            {
                done_count++;
            }
        }
        if (done_count == COUNT_VERTEX) // если все вершины выполнены, выходим из цикла
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
			done_vertex_array[i] = 1; // помечаем вершину как выполненную
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
		if (vertex_array[i].count_before_vertex > 0 && done_vertex_array[i] == 0) // проверяем только те, у которых есть зависимости и которые еще не выполнены
        {
            int counter = 0; // счетчик для проверки, выполнены ли все зависимости

			for (int j = 0; j < vertex_array[i].count_before_vertex; j++) // проходимся по массиву зависимостей текущей вершины
            {
				for (int k = 0; k < COUNT_VERTEX; k++) // проходимся по массиву done для проверки, выполнена ли зависимость
                {
					if (done_vertex_array[k] == 1 && vertex_array[k].number_vertex == vertex_array[i].array_before_vertex[j])
                    {
                        counter++;
                        break;
                    }
                }
            }
            if (counter == vertex_array[i].count_before_vertex)
            {
                find_ebeg_efin(&vertex_array[i]);
				done_vertex_array[i] = 1; // помечаем вершину как выполненную
            }
        }
    }
}

void main_algorithm_2() // обратный ход
{

}