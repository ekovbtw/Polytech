#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_VERTICES 100
#define INF INT_MAX

typedef struct Edge {
    int to;
    int capacity;
    int flow;
    struct Edge* reverse;
} Edge;

typedef struct Node {
    Edge* edge;
    struct Node* next;
} Node;

Node* graph[MAX_VERTICES];
int n;
int source_count = 0;
int sink_count = 0;
int sources[MAX_VERTICES];
int sinks[MAX_VERTICES];
int level[MAX_VERTICES];
int ptr[MAX_VERTICES];
int iteration_flows[MAX_VERTICES];
int iteration_count = 0;


Edge* create_edge(int to, int capacity) {
    Edge* edge = (Edge*)malloc(sizeof(Edge));
    edge->to = to;
    edge->capacity = capacity;
    edge->flow = 0;
    edge->reverse = NULL;
    return edge;
}


void add_edge(int from, int to, int capacity) {

    Node* node_from = (Node*)malloc(sizeof(Node));
    node_from->edge = create_edge(to, capacity);
    node_from->next = graph[from];
    graph[from] = node_from;


    Node* node_to = (Node*)malloc(sizeof(Node));
    node_to->edge = create_edge(from, 0);
    node_to->next = graph[to];
    graph[to] = node_to;


    node_from->edge->reverse = node_to->edge;
    node_to->edge->reverse = node_from->edge;
}


void read_input(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Cannot open input file: %s\n", filename);
        exit(1);
    }

    int variant;
    fscanf(file, "%d %d", &n, &variant);


    for (int i = 0; i < n; i++) {
        graph[i] = NULL;
    }


    char token[10];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            fscanf(file, "%s", token);
            if (strcmp(token, "*") != 0) {
                int weight = atoi(token);
                if (weight > 0) {
                    add_edge(i, j, weight);
                }
            }
        }
    }

    fclose(file);
}


void find_sources_and_sinks() {
    int in_degree[MAX_VERTICES] = { 0 };
    int out_degree[MAX_VERTICES] = { 0 };


    for (int i = 0; i < n; i++) {
        Node* current = graph[i];
        while (current != NULL) {
            if (current->edge->capacity > 0) {
                out_degree[i]++;
                in_degree[current->edge->to]++;
            }
            current = current->next;
        }
    }


    source_count = 0;
    for (int i = 0; i < n; i++) {
        if (in_degree[i] == 0 && out_degree[i] > 0) {
            sources[source_count++] = i;
        }
    }


    sink_count = 0;
    for (int i = 0; i < n; i++) {
        if (out_degree[i] == 0 && in_degree[i] > 0) {
            sinks[sink_count++] = i;
        }
    }
}


int bfs(int source, int sink) {
    for (int i = 0; i < n; i++) {
        level[i] = -1;
    }

    int queue[MAX_VERTICES];
    int front = 0, rear = 0;

    level[source] = 0;
    queue[rear++] = source;

    while (front < rear) {
        int v = queue[front++];

        Node* current = graph[v];
        while (current != NULL) {
            Edge* edge = current->edge;
            if (level[edge->to] == -1 && edge->capacity - edge->flow > 0) {
                level[edge->to] = level[v] + 1;
                queue[rear++] = edge->to;
            }
            current = current->next;
        }
    }

    return level[sink] != -1;
}


int dfs(int v, int sink, int flow) {
    if (v == sink || flow == 0) {
        return flow;
    }

    Node* current = graph[v];
    while (current != NULL) {
        Edge* edge = current->edge;

        if (level[edge->to] == level[v] + 1) {
            int pushed = dfs(edge->to, sink,
                (flow < edge->capacity - edge->flow) ? flow : edge->capacity - edge->flow);

            if (pushed > 0) {
                edge->flow += pushed;
                edge->reverse->flow -= pushed;
                return pushed;
            }
        }
        current = current->next;
    }

    return 0;
}

int dinic(int source, int sink) {
    int max_flow = 0;

    while (bfs(source, sink)) {
        for (int i = 0; i < n; i++) {
            ptr[i] = 0;
        }

        int flow;
        while ((flow = dfs(source, sink, INF)) > 0) {
            max_flow += flow;
        }
    }

    return max_flow;
}


int dinic_with_iterations(int source, int sink) {
    int max_flow = 0;
    iteration_count = 0;

    while (bfs(source, sink)) {
        for (int i = 0; i < n; i++) {
            ptr[i] = 0;
        }

        int flow;
        int iteration_flow = 0;

        while ((flow = dfs(source, sink, INF)) > 0) {
            iteration_flow += flow;
        }

        if (iteration_flow > 0) {
            iteration_flows[iteration_count++] = iteration_flow;
            max_flow += iteration_flow;
        }
    }

    return max_flow;
}

int solve_max_flow() {
    if (source_count == 0 || sink_count == 0) {
        return 0;
    }


    if (source_count == 1 && sink_count == 1) {
        return dinic_with_iterations(sources[0], sinks[0]);
    }


    int super_source = n;
    int super_sink = n + 1;
    n += 2;


    for (int i = 0; i < source_count; i++) {
        add_edge(super_source, sources[i], INF);
    }


    for (int i = 0; i < sink_count; i++) {
        add_edge(sinks[i], super_sink, INF);
    }

    int max_flow = dinic_with_iterations(super_source, super_sink);


    n -= 2;

    return max_flow;
}


void get_flow_matrix(int flow_matrix[MAX_VERTICES][MAX_VERTICES]) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            flow_matrix[i][j] = 0;
        }
    }

    for (int i = 0; i < n; i++) {
        Node* current = graph[i];
        while (current != NULL) {
            Edge* edge = current->edge;
            if (edge->capacity > 0 && edge->flow > 0) {
                flow_matrix[i][edge->to] = edge->flow;
            }
            current = current->next;
        }
    }
}

void write_output(const char* filename) {
    char output_filename[256];
    strcpy(output_filename, filename);


    char* dot = strrchr(output_filename, '.');
    if (dot && strcmp(dot, ".in") == 0) {
        strcpy(dot, ".out");
    }
    else {
        strcat(output_filename, ".out");
    }

    FILE* file = fopen(output_filename, "w");
    if (!file) {
        printf("Cannot open output file: %s\n", output_filename);
        exit(1);
    }


    int max_flow = solve_max_flow();


    for (int i = 0; i < source_count; i++) {
        fprintf(file, "%d", sources[i] + 1);
        if (i < source_count - 1) fprintf(file, " ");
    }
    fprintf(file, "\n");


    for (int i = 0; i < sink_count; i++) {
        fprintf(file, "%d", sinks[i] + 1);
        if (i < sink_count - 1) fprintf(file, " ");
    }
    fprintf(file, "\n");


    for (int i = 0; i < iteration_count; i++) {
        fprintf(file, "%d", iteration_flows[i]);
        if (i < iteration_count - 1) fprintf(file, ", ");
    }
    fprintf(file, "\n");

    int flow_matrix[MAX_VERTICES][MAX_VERTICES];
    get_flow_matrix(flow_matrix);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            fprintf(file, "%d", flow_matrix[i][j]);
            if (j < n - 1) fprintf(file, " ");
        }
        fprintf(file, "\n");
    }


    fprintf(file, "%d\n", max_flow);

    fclose(file);
}

int main() {
    const char* input_file = "job_Var7.in";


    read_input(input_file);


    find_sources_and_sinks();


    write_output(input_file);

    printf("Solution completed. Check output file.\n");

    return 0;
}