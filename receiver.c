#include <stdio.h> // printnf(), perror(), fflush()
#include <stdlib.h> // exit(), system()
#include <string.h> // strlen()
#include <fcntl.h> // open(), O_READONLY, O_WRONLY, O_NONBLOCK
// #include <sys/stat.h>  For file permission constants, probably unneeded test without
#include <unistd.h> // close(), read(), write(), usleep(), STDIN_FILENO
#include <signal.h> // signal(), SIGINT, SIGTERM
#include "sensor_common.h" // duh

// Global variables
int shm_fd = -1;
SharedData *shared_data = NULL;

void handle_signal(int sig)
{
    if (shared_data)
    {
        munmap(shared_data, sizeof(SharedData));
    }
    if (shm_fd >= 0)
    {
        close(shm_fd);
    }
    exit(sig);
}

int main()
{
    int last_update_count = -1;

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Open shared memory
    shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        printf("Make sure the sender program is running first!\n");
        exit(1);
    }

    // Map shared memory
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap");
        close(shm_fd);
        exit(1);
    }

    printf("Sensor data receiver started. Press 'c' to calibrate, 'q' to quit.\n");
    printf("Waiting for sensor data...\n");

    // Set terminal to non-canonical mode
    system("stty raw");

    while (1)
    {
        // Check for keyboard input
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) > 0)
        {
            if (c == 'c' || c == 'C')
            {
                printf("\rCalibrating sensors...\n");
                request_sensor_calibration();
            }
            else if (c == 'q' || c == 'Q')
            {
                printf("\rExiting...\n");
                break;
            }
        }

        // Check if new data is available
        if (shared_data->updated != last_update_count)
        {
            last_update_count = shared_data->updated;

            // Process the data
            printf("\rRoll: %.2f°, Pitch: %.2f°, Yaw: %.2f°    ",
                   shared_data->roll, shared_data->pitch, shared_data->yaw);
            fflush(stdout);
        }

        usleep(10000); // 10ms sleep
    }

    // Restore terminal
    system("stty cooked");

    // Cleanup
    munmap(shared_data, sizeof(SharedData));
    close(shm_fd);

    return 0;
}

void request_sensor_calibration()
{
    int fifo_fd;

    // Open pipe for writing
    fifo_fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fifo_fd < 0)
    {
        perror("Failed to open calibration FIFO");
        printf("Make sure the sender program is running!\n");
        return;
    }

    // Send calibration command
    write(fifo_fd, CALIBRATE_CMD, strlen(CALIBRATE_CMD));
    close(fifo_fd);
}