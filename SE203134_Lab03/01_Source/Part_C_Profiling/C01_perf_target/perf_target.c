/*
 * LAB 03 - PART C
 * System Call & CPU Profiling
 *
 * SE203134
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define ARRAY_SIZE 100000
#ifndef ITERATIONS
#define ITERATIONS 50
#endif

/* CPU-intensive computation */
__attribute__((noinline))
double heavy_compute(double *arr, int n)
{
    double result = 0.0;

    for (int i = 0; i < n; i++) {
        result += sqrt(arr[i]) * log(arr[i] + 1.0);
    }

    return result;
}

/* Linear search: O(n) */
__attribute__((noinline))
int linear_search(int *arr, int n, int target)
{
    for (int i = 0; i < n; i++) {
        if (arr[i] == target)
            return i;
    }

    return -1;
}

/* I/O-intensive operation */
void io_intensive(int count)
{
    char filename[128];

    snprintf(filename, sizeof(filename),
             "/tmp/perf_test_%ld.txt",
             (long)getpid());

    FILE *f = fopen(filename, "w");

    if (f == NULL) {
        perror("fopen");
        return;
    }

    for (int i = 0; i < count; i++) {
        fprintf(f, "Line %d: measurement data\n", i);
    }

    fclose(f);
}

int main(void)
{
    printf("=== LAB 03 - PERFORMANCE ANALYSIS ===\n");

    double *darr = malloc(sizeof(double) * ARRAY_SIZE);
    int *iarr = malloc(sizeof(int) * ARRAY_SIZE);

    if (darr == NULL || iarr == NULL) {
        perror("malloc");
        free(darr);
        free(iarr);
        return 1;
    }

    for (int i = 0; i < ARRAY_SIZE; i++) {
        darr[i] = (double)(i + 1);
        iarr[i] = i;
    }

    printf("[1] CPU-intensive computation...\n");

    double total = 0.0;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        total += heavy_compute(darr, ARRAY_SIZE);
    }

    printf("Result: %f\n", total);

    printf("[2] Linear search (1000 times)...\n");

    long search_total = 0;

    for (int i = 0; i < 1000; i++) {
        search_total += linear_search(
            iarr,
            ARRAY_SIZE,
            ARRAY_SIZE - 1
        );
    }

    printf("Search checksum: %ld\n", search_total);

    printf("[3] I/O intensive (10000 lines)...\n");

    io_intensive(10000);

    free(darr);
    free(iarr);

    printf("Done.\n");

    return 0;
}

