#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>

// Set your GPIO pin number here (BCM numbering)
#define GPIO_PIN 17  // Change this to your desired pin
#define GPIO_PATH_FMT "/sys/class/gpio/gpio%d/value"
#define GPIO_EXPORT_PATH "/sys/class/gpio/export"
#define GPIO_DIRECTION_FMT "/sys/class/gpio/gpio%d/direction"

int main() {
    char gpio_val_path[64];
    char gpio_dir_path[64];
    FILE *f;
    int val;
    struct timeval tv;
    long long ms;
    int last_state = 0;
    
    // Export the GPIO pin if not already exported
    snprintf(gpio_val_path, sizeof(gpio_val_path), GPIO_PATH_FMT, GPIO_PIN);
    if (access(gpio_val_path, F_OK) == -1) {
        f = fopen(GPIO_EXPORT_PATH, "w");
        if (!f) { perror("Export GPIO"); return 1; }
        fprintf(f, "%d", GPIO_PIN);
        fclose(f);
        usleep(100000); // Wait for sysfs to create files
    }
    // Set direction to input
    snprintf(gpio_dir_path, sizeof(gpio_dir_path), GPIO_DIRECTION_FMT, GPIO_PIN);
    f = fopen(gpio_dir_path, "w");
    if (!f) { perror("Set GPIO direction"); return 1; }
    fprintf(f, "in");
    fclose(f);

    // Prepare CSV file
    FILE *csv = fopen("testing/timing_test.csv", "a");
    if (!csv) { perror("Open CSV"); return 1; }
    // Write header if file is empty
    fseek(csv, 0, SEEK_END);
    if (ftell(csv) == 0) fprintf(csv, "timestamp_ms\n");
    fclose(csv);

    printf("Monitoring GPIO%d for HIGH signal. Logging to testing/timing_test.csv.\n", GPIO_PIN);

    while (1) {
        f = fopen(gpio_val_path, "r");
        if (!f) { perror("Read GPIO value"); return 1; }
        if (fscanf(f, "%d", &val) != 1) val = 0;
        fclose(f);
        if (val == 1 && last_state == 0) { // Rising edge
            gettimeofday(&tv, NULL);
            ms = (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
            csv = fopen("testing/timing_test.csv", "a");
            if (csv) {
                fprintf(csv, "%lld\n", ms);
                fclose(csv);
            }
            printf("HIGH detected at %lld ms\n", ms);
        }
        last_state = val;
        usleep(10000); // 10 ms
    }
    return 0;
} 