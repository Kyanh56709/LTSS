/*----------------------------------------------------
 * File:    mpi_Dijkstra.c
 *
 * Purpose: implement Dijkstra's shortest path algorithm
 *          for a weighted directed graph using mpi
 *
 * Compile: mpicc -g -Wall -o mpi_Dijkstra mpi_Dijkstra.c
 * Run:     mpiexec -n <number of processes> mpi_Dijkstra
 *
 * Input:   n: the number of vertices
 *          mat: the matrix where mat[i][j] is the length
 *               from vertex i to j
 *
 * Output:  length of the shortest path from vertex 0 to vertex v
 *          Shortest path to each vertex v from vertex 0
 *
 * Algorithm: the matrix mat is partioned by columns so that each
 *            process gets n / p columns. In each iteration each
 *            process finds its local vertex with the shortest distance
 *            from the source vertex 0. A global minimum vertex u of the found
 *            shortest distances is computed and then each process updates
 *            its local distance array if there's a shorter path that goes through u
 *
 * Note:    1. This program assumes n is evenly divisible by
 *             p (number of processes)
 *          2. Edge weights should be nonnegative
 *          3. If there is no edge between any two vertices the weight is the constant
 *             INFINITY
 *          4. The cost of traveling to itself is 0
 *          5. The adjacency matrix is stored as an 1-dimensional array and subscripts
 *             are computed using A[n * i + j] to get A[i][j] in the 2-dimensional case
 *
 * Author: Henrik Lehmann
 *-----------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#define INFINITY 1000000

int Read_n(int my_rank, MPI_Comm comm);
MPI_Datatype Build_blk_col_type(int n, int loc_n);
void Read_matrix(int loc_mat[], int n, int loc_n, MPI_Datatype blk_col_mpi_t,
                 int my_rank, MPI_Comm comm);
void Dijkstra_Init(int loc_mat[], int loc_pred[], int loc_dist[], int loc_known[],
                   int my_rank, int loc_n);
void Dijkstra(int loc_mat[], int loc_dist[], int loc_pred[], int loc_n, int n,
              MPI_Comm comm);
int Find_min_dist(int loc_dist[], int loc_known[], int loc_n);
void Print_matrix(int global_mat[], int rows, int cols);
void Print_dists(int global_dist[], int n, FILE *output_file);
void Print_paths(int global_pred[], int n, FILE *output_file);

int main(int argc, char **argv)
{
    int *loc_mat, *loc_dist, *loc_pred, *global_dist = NULL, *global_pred = NULL;
    int my_rank, p, loc_n, n;
    MPI_Comm comm;
    MPI_Datatype blk_col_mpi_t;

    double start, end, comm_time, total_time;

    MPI_Init(NULL, NULL);
    comm = MPI_COMM_WORLD;
    MPI_Comm_rank(comm, &my_rank);
    MPI_Comm_size(comm, &p);
    // so luong mau dau vao
    n = Read_n(my_rank, comm);
    loc_n = n / p;
    loc_mat = malloc(n * loc_n * sizeof(int));
    loc_dist = malloc(loc_n * sizeof(int));
    loc_pred = malloc(loc_n * sizeof(int));
    if (loc_mat == NULL || loc_dist == NULL || loc_pred == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        MPI_Finalize();
        exit(-1);
    }

    blk_col_mpi_t = Build_blk_col_type(n, loc_n);

    if (my_rank == 0)
    {
        global_dist = malloc(n * sizeof(int));
        global_pred = malloc(n * sizeof(int));
    }
    Read_matrix(loc_mat, n, loc_n, blk_col_mpi_t, my_rank, comm);

    // Bat dau do thoi gian
    start = MPI_Wtime();
    Dijkstra(loc_mat, loc_dist, loc_pred, loc_n, n, comm);
    end = MPI_Wtime();
    // ket thuc

    total_time = end - start;

    /* Gather the results from Dijkstra */
    comm_time = 0;
    start = MPI_Wtime();
    MPI_Gather(loc_dist, loc_n, MPI_INT, global_dist, loc_n, MPI_INT, 0, comm);
    MPI_Gather(loc_pred, loc_n, MPI_INT, global_pred, loc_n, MPI_INT, 0, comm);
    end = MPI_Wtime();
    comm_time += end - start;

    FILE *output_file = NULL;
    if (my_rank == 0)
    {
        output_file = fopen("dijkstra_output.txt", "w");
        if (output_file == NULL)
        {
            fprintf(stderr, "Error opening output file\n");
            MPI_Finalize();
            exit(-1);
        }
        else
        {
            printf(" opening output file\n");
        }
    }

    FILE *dijkstra_graph_nT = NULL;
    if (my_rank == 0)
    {
        dijkstra_graph_nT = fopen("dijkstra_graph_nT.txt", "a");
        if (dijkstra_graph_nT == NULL)
        {
            fprintf(stderr, "Error opening output file\n");
            MPI_Finalize();
            exit(-1);
        }
        else
        {
            printf("opening output file\n");
        }
    }

    FILE *dijkstra_graph_nCPUT = NULL;
    if (my_rank == 0)
    {
        dijkstra_graph_nCPUT = fopen("dijkstra_graph_nCPUT.txt", "a");
        if (dijkstra_graph_nCPUT == NULL)
        {
            fprintf(stderr, "Error opening output file\n");
            MPI_Finalize();
            exit(-1);
        }
        else
        {
            printf("opening output file\n");
        }
    }

    /* Print results */
    if (my_rank == 0)
    {
        Print_dists(global_dist, n, output_file);
        Print_paths(global_pred, n, output_file);
        fprintf(output_file, "t_w_comm: %f s\n", total_time);
        fprintf(output_file, "t_wo_comm: %f s\n", total_time - comm_time);
        fclose(output_file);

        fprintf(dijkstra_graph_nT, "%d, ", n);                      // so luong mau
        fprintf(dijkstra_graph_nT, "%f, ", total_time);             // t_w_comm
        fprintf(dijkstra_graph_nT, "%f\n", total_time - comm_time); // t_wo_comm:
        fclose(dijkstra_graph_nT);

        fprintf(dijkstra_graph_nCPUT, "%d, ", p);                      // so luong cpu
        fprintf(dijkstra_graph_nCPUT, "%f, ", total_time);             // t_w_comm
        fprintf(dijkstra_graph_nCPUT, "%f\n", total_time - comm_time); // t_wo_comm:
        fclose(dijkstra_graph_nCPUT);

        free(global_dist);
        free(global_pred);
    }
    free(loc_mat);
    free(loc_pred);
    free(loc_dist);
    MPI_Type_free(&blk_col_mpi_t);
    MPI_Finalize();
    return 0;
}

int Read_n(int my_rank, MPI_Comm comm)
{
    int n;

    if (my_rank == 0)
        scanf("%d", &n);

    MPI_Bcast(&n, 1, MPI_INT, 0, comm);
    return n;
}

MPI_Datatype Build_blk_col_type(int n, int loc_n)
{
    MPI_Aint lb, extent;
    MPI_Datatype block_mpi_t;
    MPI_Datatype first_bc_mpi_t;
    MPI_Datatype blk_col_mpi_t;

    MPI_Type_contiguous(loc_n, MPI_INT, &block_mpi_t);
    MPI_Type_get_extent(block_mpi_t, &lb, &extent);

    MPI_Type_vector(n, loc_n, n, MPI_INT, &first_bc_mpi_t);

    MPI_Type_create_resized(first_bc_mpi_t, lb, extent, &blk_col_mpi_t);

    MPI_Type_commit(&blk_col_mpi_t);

    MPI_Type_free(&block_mpi_t);
    MPI_Type_free(&first_bc_mpi_t);

    return blk_col_mpi_t;
}

void Read_matrix(int loc_mat[], int n, int loc_n,
                 MPI_Datatype blk_col_mpi_t, int my_rank, MPI_Comm comm)
{
    int *mat = NULL;
    int i = 0;
    int j = 0;
    if (my_rank == 0)
    {
        mat = malloc(n * n * sizeof(int));
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++)
                scanf("%d", &mat[i * n + j]);
    }

    double start = MPI_Wtime();
    MPI_Scatter(mat, 1, blk_col_mpi_t, loc_mat, n * loc_n, MPI_INT, 0, comm);
    double end = MPI_Wtime();

    if (my_rank == 0)
    {
        free(mat);
    }
}

void Dijkstra_Init(int loc_mat[], int loc_pred[], int loc_dist[], int loc_known[],
                   int my_rank, int loc_n)
{
    int loc_v;

    if (my_rank == 0)
        loc_known[0] = 1;
    else
        loc_known[0] = 0;

    for (loc_v = 1; loc_v < loc_n; loc_v++)
        loc_known[loc_v] = 0;

    for (loc_v = 0; loc_v < loc_n; loc_v++)
    {
        loc_dist[loc_v] = loc_mat[0 * loc_n + loc_v];
        loc_pred[loc_v] = 0;
    }
}

void Dijkstra(int loc_mat[], int loc_dist[], int loc_pred[], int loc_n, int n,
              MPI_Comm comm)
{

    int i, loc_v, loc_u, glbl_u, new_dist, my_rank, dist_glbl_u;
    int *loc_known;
    int my_min[2];
    int glbl_min[2];
    double start, end;

    MPI_Comm_rank(comm, &my_rank);
    loc_known = malloc(loc_n * sizeof(int));

    Dijkstra_Init(loc_mat, loc_pred, loc_dist, loc_known, my_rank, loc_n);

    for (i = 0; i < n - 1; i++)
    {
        loc_u = Find_min_dist(loc_dist, loc_known, loc_n);

        if (loc_u != -1)
        {
            my_min[0] = loc_dist[loc_u];
            my_min[1] = loc_u + my_rank * loc_n;
        }
        else
        {
            my_min[0] = INFINITY;
            my_min[1] = -1;
        }

        start = MPI_Wtime();
        MPI_Allreduce(my_min, glbl_min, 1, MPI_2INT, MPI_MINLOC, comm);
        end = MPI_Wtime();
        loc_u = glbl_min[1] % loc_n;

        glbl_u = glbl_min[1];
        dist_glbl_u = glbl_min[0];

        if (glbl_min[1] == -1)
            break;

        loc_known[loc_u] = 1;

        for (loc_v = 0; loc_v < loc_n; loc_v++)
        {
            if (!loc_known[loc_v])
            {
                new_dist = dist_glbl_u + loc_mat[glbl_u * loc_n + loc_v];

                if (new_dist < loc_dist[loc_v])
                {
                    loc_dist[loc_v] = new_dist;
                    loc_pred[loc_v] = glbl_u;
                }
            }
        }
    }
    free(loc_known);
}

int Find_min_dist(int loc_dist[], int loc_known[], int loc_n)
{
    int loc_u, loc_v;
    int shortest_dist = INFINITY;
    loc_u = -1;

    for (loc_v = 0; loc_v < loc_n; loc_v++)
    {
        if (!loc_known[loc_v])
        {
            if (loc_dist[loc_v] < shortest_dist)
            {
                shortest_dist = loc_dist[loc_v];
                loc_u = loc_v;
            }
        }
    }

    return loc_u;
}

void Print_matrix(int global_mat[], int rows, int cols)
{
    int i, j;

    for (i = 0; i < rows; i++)
    {
        for (j = 0; j < cols; j++)
            printf("%d ", global_mat[i * cols + j]);
        printf("\n");
    }
}

void Print_dists(int global_dist[], int n, FILE *output_file)
{
    int v;
    fprintf(output_file, "    v     dist 0->v\n");
    fprintf(output_file, "  ----    ---------\n");
    for (v = 1; v < n; v++)
        fprintf(output_file, "    %d        %d\n", v, global_dist[v]);
    fprintf(output_file, "\n");
}

void Print_paths(int global_pred[], int n, FILE *output_file)
{
    int v, w, *path, count, i;

    path = malloc(n * sizeof(int));
    printf("  v     Path 0->v\n");
    printf("----    ---------\n");
    fprintf(output_file, "    v     Path 0->v\n");
    fprintf(output_file, "  ----    ---------\n");
    for (v = 1; v < n; v++)
    {
        printf("%3d:    ", v);
        fprintf(output_file, "    %d:    ", v);
        count = 0;
        w = v;
        while (w != 0)
        {
            path[count] = w;
            count++;
            w = global_pred[w];
        }
        printf("0 ");
        fprintf(output_file, "0 ");
        for (i = count - 1; i >= 0; i--){
            fprintf(output_file, "%d ", path[i]);
            printf("%d ", path[i]);
        }
        fprintf(output_file, "\n");
        printf("\n");
    }
    free(path);
    fprintf(output_file, "\n");
}