#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <math.h>

#define GRAVITY_SCALE 0.0000001f  // Use this scale instead of sysfs value
// Calibration: set to {0,0,0} initially, then update after calibration
static float GRAVITY_OFFSET[3] = {1.279940f, 0.227644f, -0.084857f};

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

void calibrate_gravity_offset(const char *gravity_dir) {
    const int samples = 100;
    int raw_gravity[3];
    const char *axes[3] = {"x", "y", "z"};
    float gravity[3] = {0, 0, 0};
    float sum[3] = {0, 0, 0};
    float gravity_scale = GRAVITY_SCALE;
    float gravity_offset = 0;
    read_sysfs_float(gravity_dir, "in_gravity_offset", &gravity_offset);
    printf("\nCalibration: Point the glasses straight down and press Enter.\n");
    getchar();
    printf("Calibrating... Please hold still.\n");
    fflush(stdout);
    for (int i = 0; i < samples; i++) {
        for (int j = 0; j < 3; j++) {
            read_gravity_raw(gravity_dir, axes[j], &raw_gravity[j]);
            gravity[j] = raw_gravity[j] * gravity_scale + gravity_offset;
            sum[j] += gravity[j];
        }
        usleep(10000); // 10 ms
    }
    float avg[3];
    for (int j = 0; j < 3; j++) avg[j] = sum[j] / samples;
    // Expected gravity vector is [0, 0, 10] (down)
    float expected[3] = {0.0f, 0.0f, 10.0f};
    float offset[3];
    for (int j = 0; j < 3; j++) offset[j] = avg[j] - expected[j];
    printf("\nCalibration complete. Use this for GRAVITY_OFFSET in your code:\n");
    printf("static float GRAVITY_OFFSET[3] = {%.6ff, %.6ff, %.6ff};\n", offset[0], offset[1], offset[2]);
    printf("\nExiting calibration.\n");
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);
    char gravity_dir[256] = "";
    find_gravity_device_dir(gravity_dir, sizeof(gravity_dir));
    if (gravity_dir[0] == '\0') {
        fprintf(stderr, "Gravity IIO device not found.\n");
        return 1;
    }
    // --- Calibration step: uncomment to calibrate, then copy offset and remove ---
    // calibrate_gravity_offset(gravity_dir);
    float gravity_scale = GRAVITY_SCALE;
    float gravity_offset = 0;
    // Do not read sysfs scale, only offset if present
    read_sysfs_float(gravity_dir, "in_gravity_offset", &gravity_offset);
    int raw_gravity[3];
    const char *axes[3] = {"x", "y", "z"};
    float gravity[3];
    // Forward vector (0,0,1)
    float forward[3] = {0, 0, 1};
    while (!stop) {
        for (int j = 0; j < 3; j++) {
            read_gravity_raw(gravity_dir, axes[j], &raw_gravity[j]);
            gravity[j] = raw_gravity[j] * gravity_scale + gravity_offset - GRAVITY_OFFSET[j];
        }
        // Normalize vectors
        float gmag = sqrt(gravity[0]*gravity[0] + gravity[1]*gravity[1] + gravity[2]*gravity[2]);
        float fmag = 1.0f; // already unit vector
        float dot = (gravity[0]*forward[0] + gravity[1]*forward[1] + gravity[2]*forward[2]) / (gmag * fmag);
        // Clamp dot to [-1, 1] to avoid NaN from acos
        if (dot > 1.0f) dot = 1.0f;
        if (dot < -1.0f) dot = -1.0f;
        float angle_rad = acosf(dot);
        float angle_deg = angle_rad * 180.0f / M_PI;
        printf("\rGravity: [%.4f %.4f %.4f] | Magnitude: %.4f | Angle: %.2f deg   ", gravity[0], gravity[1], gravity[2], gmag, angle_deg);
        if (angle_deg <= 50.0f) {
            printf("within range   ");
        } else {
            printf("outside range   ");
        }
        fflush(stdout);
        usleep(10000); // 10 ms (100Hz)
    }
    printf("\nStopping.\n");
    return 0;
} 