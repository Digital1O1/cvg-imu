#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#define PIPE_PATH "/tmp/hmdop_laser_pipe"
#define MESSAGE "LASER_OFF\n"
#define INTERVAL_MS 200

int main() {
    int pipe_fd;
    FILE *logf = fopen("./send_time.csv", "a");
    fseek(logf, 0, SEEK_END);
    if (ftell(logf) == 0) fprintf(logf, "timestamp_ms\n");
    fclose(logf);

    while (1) {
        pipe_fd = open(PIPE_PATH, O_WRONLY | O_NONBLOCK);
        if (pipe_fd >= 0) {
            write(pipe_fd, MESSAGE, strlen(MESSAGE));
            close(pipe_fd);
            printf("Wrote LASER_OFF to pipe\n");
        } else {
            perror("Failed to open pipe");
        }
        // Log timestamp to send_time.csv for testing purposes
        struct timeval tv;
        gettimeofday(&tv, NULL);
        long long ms = (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
        *logf = fopen("./send_time.csv", "a");
        if (logf) {
            fprintf(logf, "%lld\n", ms);
            fclose(logf);
        }
        usleep(INTERVAL_MS * 1000); // 200 ms
    }
    return 0;
} 