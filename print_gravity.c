/* Z AXIS IS STRAIGHT*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <math.h>

volatile sig_atomic_t stop = 0;
void handle_sigint(int sig) { stop = 1; }

// Helper to read a float from sysfs
int read_sysfs_float(const char *dir, const char *file, float *value) {
    char path[512], buf[64];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    FILE *f = fopen(path, "r"); 
    if (!f) return -1;
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    *value = atof(buf);
    fclose(f);
    return 0;
}

// Helper to read a raw gravity value (no scaling)
int read_gravity_raw(const char *dev_dir, const char *axis, int *raw_out) {
    char raw_file[64];
    snprintf(raw_file, sizeof(raw_file), "in_gravity_%s_raw", axis);
    char path[512], buf[64];
    snprintf(path, sizeof(path), "%s/%s", dev_dir, raw_file);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    *raw_out = atoi(buf);
    fclose(f);
    return 0;
}

// Find the gravity IIO device directory
void find_gravity_device_dir(char *gravity_dir, size_t gravity_len) {
    DIR *dir = opendir("/sys/bus/iio/devices/");
    if (!dir) return;
    struct dirent *entry;
    char path[512];
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) == 0) {
            snprintf(path, sizeof(path), "/sys/bus/iio/devices/%s/in_gravity_scale", entry->d_name);
            if (access(path, R_OK) == 0 && gravity_dir[0] == '\0') {
                snprintf(gravity_dir, gravity_len, "/sys/bus/iio/devices/%s", entry->d_name);
            }
        }
    }
    closedir(dir);
}

int main() {
    signal(SIGINT, handle_sigint);
    char gravity_dir[256] = "";
    find_gravity_device_dir(gravity_dir, sizeof(gravity_dir));
    if (gravity_dir[0] == '\0') {
        fprintf(stderr, "Gravity IIO device not found.\n");
        return 1;
    }
    float gravity_scale = 0.0000001, gravity_offset = 0;
    read_sysfs_float(gravity_dir, "in_gravity_offset", &gravity_offset);
    int raw_gravity[3];
    const char *axes[3] = {"x", "y", "z"};
    float gravity[3];
    printf("Gravity vector (x y z):\n");
    while (!stop) {
        for (int j = 0; j < 3; j++) {
            read_gravity_raw(gravity_dir, axes[j], &raw_gravity[j]);
            gravity[j] = raw_gravity[j] * gravity_scale + gravity_offset;
        }
        float mag = sqrt(gravity[0]*gravity[0] + gravity[1]*gravity[1] + gravity[2]*gravity[2]);
        printf("\rGravity: [%.4f %.4f %.4f] | Magnitude: %.4f   ", gravity[0], gravity[1], gravity[2], mag);
        fflush(stdout);
        usleep(10000); // 10 ms (100Hz)
    }
    printf("\nStopping.\n");
    return 0;
} 