#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Usage: ./copy_stdio src dst buffer_size_in_bytes */
int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s src dst buffer_size\n", argv[0]);
        return 1;
    }

    size_t bufsize = strtoul(argv[3], NULL, 10);
    if (bufsize == 0) {
        fprintf(stderr, "buffer size must be greater than 0\n");
        return 1;
    }

    char *buf = malloc(bufsize);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        perror("fopen source");
        return 1;
    }
    FILE *out = fopen(argv[2], "wb");
    if (!out) {
        perror("fopen destination");
        return 1;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    size_t n;
    while ((n = fread(buf, 1, bufsize, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            perror("fwrite");
            return 1;
        }
    }
    if (ferror(in)) {
        perror("fread");
        return 1;
    }

    /* fclose flushes the last partial buffer, so keep it inside the timing */
    if (fclose(out) != 0) {
        perror("fclose destination");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    fclose(in);
    free(buf);

    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("elapsed: %.6f s\n", elapsed);
    return 0;
}