#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define MAX_N     100000
#define NTHREADS  8
#define SEGMENT   (MAX_N / NTHREADS)   /* 12500 numbers per thread */
#define MAX_FOUND 64

/* What each thread receives: its range, plus a slot to report its own time. */
typedef struct {
    int    id;
    long   start;
    long   end;
    double elapsed;
} thread_arg_t;

/* Shared data. The mutex protects global_count and global_found. */
static pthread_mutex_t lock;
static int  global_count = 0;
static long global_found[MAX_FOUND];

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/*
 * Divisors come in pairs (i, n/i), so looping only up to sqrt(n) finds them all.
 * sum starts at 1 because 1 divides every n > 1. n itself is never added,
 * so only proper divisors are summed.
 */
static int is_perfect(long n)
{
    if (n < 2) return 0;
    long sum = 1;
    for (long i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            sum += i;
            if (i != n / i)        /* avoid counting a square root twice */
                sum += n / i;
        }
    }
    return sum == n;
}

static void *worker(void *arg)
{
    thread_arg_t *a = arg;
    long local_found[MAX_FOUND];
    int  local_count = 0;

    double t0 = now();

    /* No locking in this loop: everything here is thread-private. */
    for (long n = a->start; n <= a->end; n++) {
        if (is_perfect(n))
            local_found[local_count++] = n;
    }

    a->elapsed = now() - t0;

    /* One lock per thread: publish the count and the numbers together. */
    pthread_mutex_lock(&lock);
    int base = global_count;
    for (int i = 0; i < local_count; i++)
        global_found[base + i] = local_found[i];
    global_count += local_count;
    pthread_mutex_unlock(&lock);

    return NULL;
}

static int cmp_long(const void *a, const void *b)
{
    long x = *(const long *)a, y = *(const long *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    pthread_t    tid[NTHREADS];
    thread_arg_t args[NTHREADS];

    /* ---------- single-threaded version ---------- */
    long single_found[MAX_FOUND];
    int  single_count = 0;

    double s0 = now();
    for (long n = 1; n <= MAX_N; n++) {
        if (is_perfect(n))
            single_found[single_count++] = n;
    }
    double single_time = now() - s0;

    /* ---------- multi-threaded version ---------- */
    if (pthread_mutex_init(&lock, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        return 1;
    }

    double m0 = now();
    for (int t = 0; t < NTHREADS; t++) {
        args[t].id    = t;
        args[t].start = (long)t * SEGMENT + 1;
        args[t].end   = (long)(t + 1) * SEGMENT;
        if (pthread_create(&tid[t], NULL, worker, &args[t]) != 0) {
            fprintf(stderr, "pthread_create failed for thread %d\n", t);
            return 1;
        }
    }
    for (int t = 0; t < NTHREADS; t++)
        pthread_join(tid[t], NULL);
    double multi_time = now() - m0;

    pthread_mutex_destroy(&lock);

    /* Threads finish in any order, so sort before printing. */
    qsort(global_found, global_count, sizeof(long), cmp_long);

    printf("Perfect numbers found:");
    for (int i = 0; i < global_count; i++)
        printf(" %ld", global_found[i]);
    printf("\n");
    printf("The synchronized total number of perfect numbers between 1 and %d is %d\n",
           MAX_N, global_count);
    printf("Single-threaded time: %.6f seconds\n", single_time);
    printf("Multi-threaded time:  %.6f seconds\n", multi_time);

    /* Extra output for the report: sanity check and per-thread times. */
    int same = (single_count == global_count);
    for (int i = 0; same && i < single_count; i++)
        if (single_found[i] != global_found[i]) same = 0;
    printf("\nMatches single-threaded result: %s\n", same ? "yes" : "NO");
    for (int t = 0; t < NTHREADS; t++)
        printf("Thread %d (%6ld to %6ld): %.6f s\n",
               t, args[t].start, args[t].end, args[t].elapsed);

    return 0;
}
