#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

#define PI 3.14159265358979323846
#define DT 0.01f // 100Hz
#define ALPHA 0.85
#define RAD_TO_DEG 57.2958f  // 180 / PI

volatile sig_atomic_t stop = 0;
void handle_sigint(int sig) { stop = 1; }

typedef struct {
    float accel[3]; // m/s²
    float gyro[3];  // rad/s
    float mag[3];   // Gauss
} SensorData;

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

// Helper to read a raw sensor value (no scaling)
int read_sensor_raw(const char *dev_dir, const char *prefix, const char *axis, int *raw_out) {
    char raw_file[64];
    snprintf(raw_file, sizeof(raw_file), "in_%s_%s_raw", prefix, axis);
    char path[512], buf[64];
    snprintf(path, sizeof(path), "%s/%s", dev_dir, raw_file);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    *raw_out = atoi(buf);
    fclose(f);
    return 0;
}

// Update function signature to accept device directories
SensorData read_sensors(const char *accel_dir, const char *gyro_dir, const char *mag_dir,
                       float accel_scale, float gyro_scale, float mag_scale, float accel_offset, float gyro_offset, float mag_offset,
                       int raw_accel[3], int raw_gyro[3], int raw_mag[3]) {
    SensorData data = {{0}};
    const char *axes[3] = {"x", "y", "z"};
    for (int j = 0; j < 3; j++) {
        read_sensor_raw(accel_dir, "accel", axes[j], &raw_accel[j]);
        read_sensor_raw(gyro_dir,  "anglvel", axes[j], &raw_gyro[j]);
        read_sensor_raw(mag_dir,   "magn", axes[j], &raw_mag[j]);
        data.accel[j] = raw_accel[j] * accel_scale + accel_offset;
        data.gyro[j]  = raw_gyro[j]  * gyro_scale  + gyro_offset;
        data.mag[j]   = raw_mag[j]   * mag_scale   + mag_offset;
    }
    return data;
}

// Assumes dt is in seconds
void complementary_filter(float accel_x, float accel_y, float accel_z,
                          float gyro_x, float gyro_y, float gyro_z,
                          float mag_x, float mag_y, float mag_z,
                          float dt,
                          float *roll, float *pitch, float *yaw) {
    // --- 1. Compute roll and pitch from accelerometer (in radians) ---
    float roll_acc = atan2f(accel_y, accel_z);
    float pitch_acc = atanf(-accel_x / sqrtf(accel_y * accel_y + accel_z * accel_z));

    // --- 2. Integrate gyroscope values (convert to degrees) ---
    float roll_gyro  = *roll + gyro_x * dt * RAD_TO_DEG;
    float pitch_gyro = *pitch + gyro_y * dt * RAD_TO_DEG;
    float yaw_gyro   = *yaw + gyro_z * dt * RAD_TO_DEG;

    // --- 3. Apply complementary filter to roll and pitch ---
    *roll  = ALPHA * roll_gyro  + (1.0f - ALPHA) * (roll_acc * RAD_TO_DEG);
    *pitch = ALPHA * pitch_gyro + (1.0f - ALPHA) * (pitch_acc * RAD_TO_DEG);

    // --- 4. Tilt-compensated magnetometer readings ---
    float roll_rad = *roll / RAD_TO_DEG;
    float pitch_rad = *pitch / RAD_TO_DEG;

    float mag_x_comp = mag_x * cosf(pitch_rad) + mag_y * sinf(roll_rad) * sinf(pitch_rad) + mag_z * cosf(roll_rad) - mag_z * sinf(pitch_rad);
    float mag_y_comp = mag_y * cosf(roll_rad) - mag_z * sinf(roll_rad);
    
    // --- 5. Yaw from magnetometer ---
    float yaw_mag = atan2f(-mag_y_comp, mag_x_comp) * RAD_TO_DEG;

    // Remove normalization: allow yaw_mag to be negative or positive
    // if (yaw_mag < 0) yaw_mag += 360.0f;

    // --- 6. Complementary filter for yaw ---
    *yaw = ALPHA * yaw_gyro + (1.0f - ALPHA) * yaw_mag;

    // Remove normalization: allow yaw to be negative or positive
    // if (*yaw < 0) *yaw += 360.0f;
    // if (*yaw >= 360.0f) *yaw -= 360.0f;
}

// --- Calibration data ---
static float roll_offset = 0, pitch_offset = 0, yaw_offset = 0;
static int calibrated = 0;

// --- Calibration function ---
void calibrate_orientation(float roll, float pitch, float yaw) {
    roll_offset = roll;
    pitch_offset = pitch;
    yaw_offset = yaw;
    calibrated = 1;
}

// --- Post-processing function (no normalization) ---
void apply_calibration(float *roll, float *pitch, float *yaw) {
    if (calibrated) {
        *roll  = *roll  - roll_offset;
        *pitch = *pitch - pitch_offset;
        *yaw   = *yaw   - yaw_offset;
    }
}

// --- Accelerometer offset calibration data ---
static float accel_calib_offset[3] = {0, 0, 0};

// --- Helper to apply accelerometer offset ---
void apply_accel_offset(float *accel) {
    for (int i = 0; i < 3; i++) {
        accel[i] -= accel_calib_offset[i];
    }
}

// --- Accelerometer calibration function ---
void calibrate_accelerometer(const char *accel_dir, float accel_scale, float accel_offset, int samples, int raw_accel[3]) {
    float sum[3] = {0, 0, 0};
    printf("\nHold the device still in all 3 axes for accelerometer calibration...\n");
    fflush(stdout);
    for (int i = 0; i < samples; i++) {
        // Only read accelerometer
        for (int j = 0; j < 3; j++) {
            read_sensor_raw(accel_dir, "accel", (j == 0 ? "x" : (j == 1 ? "y" : "z")), &raw_accel[j]);
            float val = raw_accel[j] * accel_scale + accel_offset;
            sum[j] += val;
        }
        usleep(10000); // 10 ms
    }
    for (int j = 0; j < 3; j++) {
        accel_calib_offset[j] = sum[j] / samples;
    }
    printf("Accelerometer calibration complete. Offsets: [%.3f %.3f %.3f]\n", accel_calib_offset[0], accel_calib_offset[1], accel_calib_offset[2]);
}

// Initialization function to find device directories for accel, gyro, mag
void find_sensor_device_dirs(char *accel_dir, size_t accel_len, char *gyro_dir, size_t gyro_len, char *mag_dir, size_t mag_len) {
    DIR *dir = opendir("/sys/bus/iio/devices/");
    if (!dir) return;
    struct dirent *entry;
    char path[512];
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) == 0) {
            snprintf(path, sizeof(path), "/sys/bus/iio/devices/%s/in_accel_scale", entry->d_name);
            if (access(path, R_OK) == 0 && accel_dir[0] == '\0') {
                snprintf(accel_dir, accel_len, "/sys/bus/iio/devices/%s", entry->d_name);
            }
            snprintf(path, sizeof(path), "/sys/bus/iio/devices/%s/in_anglvel_scale", entry->d_name);
            if (access(path, R_OK) == 0 && gyro_dir[0] == '\0') {
                snprintf(gyro_dir, gyro_len, "/sys/bus/iio/devices/%s", entry->d_name);
            }
            snprintf(path, sizeof(path), "/sys/bus/iio/devices/%s/in_magn_scale", entry->d_name);
            if (access(path, R_OK) == 0 && mag_dir[0] == '\0') {
                snprintf(mag_dir, mag_len, "/sys/bus/iio/devices/%s", entry->d_name);
            }
        }
    }
    closedir(dir);
}

int main() {
    signal(SIGINT, handle_sigint);
    char accel_dir[256] = "", gyro_dir[256] = "", mag_dir[256] = "";
    find_sensor_device_dirs(accel_dir, sizeof(accel_dir), gyro_dir, sizeof(gyro_dir), mag_dir, sizeof(mag_dir));
    float accel_scale = 1, gyro_scale = 1, mag_scale = 1;
    float accel_offset = 0, gyro_offset = 0, mag_offset = 0;
    read_sysfs_float(accel_dir, "in_accel_scale", &accel_scale);
    read_sysfs_float(gyro_dir,  "in_anglvel_scale", &gyro_scale);
    read_sysfs_float(mag_dir,   "in_magn_scale", &mag_scale);
    read_sysfs_float(accel_dir, "in_accel_offset", &accel_offset);
    read_sysfs_float(gyro_dir,  "in_anglvel_offset", &gyro_offset);
    read_sysfs_float(mag_dir,   "in_magn_offset", &mag_offset);
    int raw_accel[3], raw_gyro[3], raw_mag[3];
    // --- Accelerometer calibration step ---
    calibrate_accelerometer(accel_dir, accel_scale, accel_offset, 100, raw_accel);
    printf("Roll      Pitch     Yaw\n");
    float roll = 0, pitch = 0, yaw = 0;
    // --- Calibration step: wait for user to press Enter ---
    printf("\nPlace the device in the desired reference orientation and press Enter to calibrate...\n");
    SensorData data = read_sensors(accel_dir, gyro_dir, mag_dir,
                                  accel_scale, gyro_scale, mag_scale, accel_offset, gyro_offset, mag_offset,
                                  raw_accel, raw_gyro, raw_mag);
    // Apply accelerometer offset before use
    apply_accel_offset(data.accel);
    complementary_filter(data.accel[0], data.accel[1], data.accel[2],
                        data.gyro[0], data.gyro[1], data.gyro[2],
                        data.mag[0], data.mag[1], data.mag[2],
                        DT,
                        &roll, &pitch, &yaw);
    getchar();
    calibrate_orientation(roll, pitch, yaw);
    printf("Calibrated. Starting output...\n");
    // --- Main loop ---
    while (!stop) {
        data = read_sensors(accel_dir, gyro_dir, mag_dir,
                                      accel_scale, gyro_scale, mag_scale, accel_offset, gyro_offset, mag_offset,
                                      raw_accel, raw_gyro, raw_mag);
        // Apply accelerometer offset before use
        apply_accel_offset(data.accel);
        printf("\rA[%.2f %.2f %.2f]\nG[%.2f %.2f %.2f]\nM[%.2f %.2f %.2f]\n",
            data.accel[0], data.accel[1], data.accel[2],
            data.gyro[0], data.gyro[1], data.gyro[2],
            data.mag[0], data.mag[1], data.mag[2]);
        complementary_filter(data.accel[0], data.accel[1], data.accel[2],
                            data.gyro[0], data.gyro[1], data.gyro[2],
                            data.mag[0], data.mag[1], data.mag[2],
                            DT,
                            &roll, &pitch, &yaw);
        apply_calibration(&roll, &pitch, &yaw);
        printf("RPY[%8.2f %8.2f %8.2f]\n", roll, pitch, yaw);
        fflush(stdout);
        usleep(10000); // 10 ms
    }
    printf("\nStopping.\n");
    return 0;
} 
