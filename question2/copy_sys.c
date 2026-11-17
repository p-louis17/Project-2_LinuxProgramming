#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

/* Usage: ./copy_sys src dst buffer_size_in_bytes */
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

    int in = open(argv[1], O_RDONLY);
    if (in < 0) {
        perror("open source");
        return 1;
    }
    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        perror("open destination");
        return 1;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    ssize_t n;
    while ((n = read(in, buf, bufsize)) > 0) {
        /* write() may write fewer bytes than asked, so loop until done */
        ssize_t done = 0;
        while (done < n) {
            ssize_t w = write(out, buf + done, n - done);
            if (w < 0) {
                perror("write");
                return 1;
            }
            done += w;
        }
    }
    if (n < 0) {
        perror("read");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    close(in);
    close(out);
    free(buf);

    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("elapsed: %.6f s\n", elapsed);
    return 0;
}