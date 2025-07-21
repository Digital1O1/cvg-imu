#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define FIFO_PATH "/tmp/hmdop_laser_pipe"
#define BUFFER_SIZE 256

int main() {
    FILE *csv = fopen("consistency_test.csv", "w");
    if (!csv) {
        perror("Failed to open CSV file");
        return 1;
    }
    fprintf(csv, "timestamp_ms\n");
    fflush(csv);

    int fd = open(FIFO_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open FIFO pipe");
        fclose(csv);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    while (1) {
        ssize_t n = read(fd, buffer, BUFFER_SIZE - 1);
        if (n > 0) {
            buffer[n] = '\0';
            char *ptr = buffer;
            while ((ptr = strstr(ptr, "LASER_OFF")) != NULL) {
                struct timeval now;
                gettimeofday(&now, NULL);
                long long ms = (long long)now.tv_sec * 1000LL + now.tv_usec / 1000LL;
                fprintf(csv, "%lld\n", ms);
                fflush(csv);
                ptr += strlen("LASER_OFF");
            }
        } else if (n == 0) {
            // EOF, wait for more data
            usleep(10000); // 10 ms
        } else {
            perror("Read error");
            break;
        }
    }
    close(fd);
    fclose(csv);
    return 0;
} 
