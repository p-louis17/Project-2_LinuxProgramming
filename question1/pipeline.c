#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define OUTFILE "result.txt"
#define NSTAGES 3

/* Close every end of every pipe in the calling process. */
static void close_all(int p[NSTAGES][2])
{
    for (int i = 0; i < NSTAGES; i++) {
        close(p[i][0]);
        close(p[i][1]);
    }
}

/* Write the whole buffer, handling partial writes and EINTR. */
static int write_all(int fd, const char *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, buf + done, len - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

/*
 * Fork one stage.
 * Stage idx reads from p[idx-1][0] (except stage 0) and writes to p[idx][1].
 */
static pid_t spawn_stage(int idx, int p[NSTAGES][2], char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        if (idx > 0 && dup2(p[idx - 1][0], STDIN_FILENO) < 0) {
            perror("dup2 stdin");
            _exit(1);
        }
        if (dup2(p[idx][1], STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            _exit(1);
        }
        /* The originals are now duplicated, so close all pipe fds. */
        close_all(p);
        execvp(argv[0], argv);
        perror("execvp");   /* only reached if exec fails */
        _exit(127);
    }
    return pid;
}

int main(void)
{
    int p[NSTAGES][2];
    pid_t pids[NSTAGES];

    char *cmd0[] = {"cat", "/etc/passwd", NULL};
    char *cmd1[] = {"cut", "-d:", "-f7", NULL};
    char *cmd2[] = {"sort", "-u", NULL};
    char **cmds[NSTAGES] = {cmd0, cmd1, cmd2};

    for (int i = 0; i < NSTAGES; i++) {
        if (pipe(p[i]) < 0) {
            perror("pipe");
            return 1;
        }
    }

    for (int i = 0; i < NSTAGES; i++)
        pids[i] = spawn_stage(i, p, cmds[i]);

    /* Parent keeps only p[2][0]. Closing p[2][1] is what lets read() see EOF. */
    close(p[0][0]); close(p[0][1]);
    close(p[1][0]); close(p[1][1]);
    close(p[2][1]);

    /* Read the final stage output and save it to result.txt. */
    int out = open(OUTFILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        perror("open result.txt");
        return 1;
    }

    char buf[4096];
    for (;;) {
        ssize_t n = read(p[2][0], buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read pipe");
            return 1;
        }
        if (n == 0) break;                  /* EOF: all writers closed */
        if (write_all(out, buf, (size_t)n) < 0) {
            perror("write result.txt");
            return 1;
        }
    }
    close(p[2][0]);
    close(out);

    /* Re-open the file, read it back, show first 5 lines and total count. */
    int in = open(OUTFILE, O_RDONLY);
    if (in < 0) {
        perror("reopen result.txt");
        return 1;
    }
    struct stat st;
    if (fstat(in, &st) < 0) {
        perror("fstat");
        return 1;
    }
    char *data = malloc((size_t)st.st_size + 1);
    if (!data) {
        perror("malloc");
        return 1;
    }
    size_t total = 0;
    while (total < (size_t)st.st_size) {
        ssize_t n = read(in, data + total, (size_t)st.st_size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read result.txt");
            return 1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    close(in);

    int lines = 0;
    size_t shown_end = 0;
    for (size_t i = 0; i < total; i++) {
        if (data[i] == '\n') {
            lines++;
            if (lines == 5) shown_end = i + 1;
        }
    }
    if (total > 0 && data[total - 1] != '\n') lines++;   /* unterminated last line */
    if (lines < 5) shown_end = total;

    printf("First 5 lines of %s:\n", OUTFILE);
    fflush(stdout);
    write_all(STDOUT_FILENO, data, shown_end);
    printf("Total lines: %d\n", lines);
    free(data);

    /* Collect all three children and report their exit status. */
    const char *names[NSTAGES] = {"cat", "cut", "sort"};
    for (int i = 0; i < NSTAGES; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) < 0) {
            perror("waitpid");
            continue;
        }
        if (WIFEXITED(status))
            printf("child %d (%s) pid %d exited with status %d\n",
                   i, names[i], (int)pids[i], WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("child %d (%s) pid %d killed by signal %d\n",
                   i, names[i], (int)pids[i], WTERMSIG(status));
    }
    return 0;
}