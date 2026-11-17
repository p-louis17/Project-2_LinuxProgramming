#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define LINE_BUF 65536

/* Shared state. Each shared resource has its own mutex. */
static const char *pattern;
static char      **files;
static int         nfiles;
static int         next_file = 0;            /* the work queue: index of next unprocessed file */
static pthread_mutex_t queue_lock;           /* protects next_file */

static FILE *report;
static pthread_mutex_t report_lock;          /* protects the report file */

static long grand_matches = 0;
static long grand_lines   = 0;
static int  files_done    = 0;
static pthread_mutex_t total_lock;           /* protects the grand totals */

typedef struct {
    int id;
} worker_arg_t;

/* Count total lines and matching lines in one file. Returns 0 on success. */
static int scan_file(const char *name, long *matches, long *lines)
{
    FILE *fp = fopen(name, "r");
    if (!fp) {
        perror(name);
        return -1;
    }

    static __thread char buf[LINE_BUF];
    long m = 0, l = 0;
    int at_start = 1;      /* is the next chunk the start of a new line? */
    int matched = 0;       /* has the current line already matched? */

    while (fgets(buf, sizeof buf, fp)) {
        if (at_start) {    /* a line longer than the buffer arrives in several chunks */
            l++;
            matched = 0;
        }
        if (!matched && strstr(buf, pattern)) {
            m++;
            matched = 1;
        }
        size_t len = strlen(buf);
        at_start = (len > 0 && buf[len - 1] == '\n');
    }
    if (ferror(fp))
        perror(name);

    fclose(fp);
    *matches = m;
    *lines = l;
    return 0;
}

static void *worker(void *arg)
{
    int id = ((worker_arg_t *)arg)->id;

    for (;;) {
        /* Take the next file from the queue. */
        pthread_mutex_lock(&queue_lock);
        int idx = next_file < nfiles ? next_file++ : -1;
        pthread_mutex_unlock(&queue_lock);

        if (idx < 0)
            break;                           /* queue is empty */

        long matches, lines;
        if (scan_file(files[idx], &matches, &lines) < 0)
            continue;                        /* error already reported, take the next file */

        /* One complete line per file, so lines never interleave. */
        pthread_mutex_lock(&report_lock);
        fprintf(report, "%s: %ld matching lines out of %ld (thread %d)\n",
                files[idx], matches, lines, id);
        fflush(report);
        pthread_mutex_unlock(&report_lock);

        pthread_mutex_lock(&total_lock);
        grand_matches += matches;
        grand_lines   += lines;
        files_done++;
        pthread_mutex_unlock(&total_lock);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s pattern report.txt threads file1.log file2.log ...\n", argv[0]);
        return 1;
    }

    pattern = argv[1];
    int nthreads = atoi(argv[3]);
    if (nthreads < 1) {
        fprintf(stderr, "number of threads must be at least 1\n");
        return 1;
    }
    files  = &argv[4];
    nfiles = argc - 4;

    report = fopen(argv[2], "w");
    if (!report) {
        perror(argv[2]);
        return 1;
    }

    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&report_lock, NULL);
    pthread_mutex_init(&total_lock, NULL);

    pthread_t    *tid  = malloc(nthreads * sizeof(pthread_t));
    worker_arg_t *args = malloc(nthreads * sizeof(worker_arg_t));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < nthreads; i++) {
        args[i].id = i;
        if (pthread_create(&tid[i], NULL, worker, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed for thread %d\n", i);
            return 1;
        }
    }
    for (int i = 0; i < nthreads; i++)
        pthread_join(tid[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    /* Summary goes to the report file and to the screen. */
    fprintf(report, "Total matching lines across %d files: %ld\n", files_done, grand_matches);
    printf("Total matching lines across %d files: %ld\n", files_done, grand_matches);
    printf("Elapsed: %.6f s\n", elapsed);

    fclose(report);
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&report_lock);
    pthread_mutex_destroy(&total_lock);
    free(tid);
    free(args);
    return 0;
}