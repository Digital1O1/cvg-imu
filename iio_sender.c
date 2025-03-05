#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include "sensor_common.h"

#define IIO_DIR "/sys/bus/iio/devices/"
#define PI 3.14159265358979323846

// Complementary filter parameters
#define ALPHA 0.96
#define DT 0.01 // 100Hz sample rate

// Calibration parameters
#define CALIBRATION_SAMPLES 100
#define CALIBRATION_DELAY_MS 10

// Function prototypes
int find_iio_device(char *device_dir, size_t max_len);
int read_sensor_data(const char *device_dir, float *accel, float *gyro, float *mag);
void calculate_orientation(float *accel, float *gyro, float *mag, SharedData *data);
int read_sysfs_float(const char *dir, const char *file, float *value);
int read_scale_and_offset(const char *device_dir, const char *channel, float *scale, float *offset);
void calibrate_sensor(const char *device_dir);
void setup_calibration_pipe();
void check_calibration_request();

// Global variables
int shm_fd;
SharedData *shared_data;
int calibration_fifo_fd = -1;
int calibration_requested = 0;
float gyro_offsets[3] = {0};
float mag_offsets[3] = {0};

// Signal handler for clean shutdown
void handle_signal(int sig)
{
    if (shared_data)
    {
        munmap(shared_data, sizeof(SharedData));
    }
    if (shm_fd >= 0)
    {
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }
    if (calibration_fifo_fd >= 0)
    {
        close(calibration_fifo_fd);
        unlink(FIFO_PATH);
    }
    exit(sig);
}

int main()
{
    float accel[3] = {0}, gyro[3] = {0}, mag[3] = {0};
    char device_dir[256];

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Find IIO device
    if (find_iio_device(device_dir, sizeof(device_dir)) < 0)
    {
        printf("Failed to find MPU9250 IIO device\n");
        return -1;
    }

    printf("Found IIO device: %s\n", device_dir);

    // Initialize shared memory
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        return -1;
    }

    // Set size of shared memory segment
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1)
    {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    // Map shared memory
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    // Initialize shared data
    shared_data->roll = 0;
    shared_data->pitch = 0;
    shared_data->yaw = 0;
    shared_data->updated = 0;

    // Set up calibration pipe
    setup_calibration_pipe();

    // Initial calibration
    calibrate_sensor(device_dir);

    printf("Starting sensor readings...\n");

    while (1)
    {
        // Check for calibration requests
        check_calibration_request();

        if (calibration_requested)
        {
            calibrate_sensor(device_dir);
            calibration_requested = 0;
        }

        // Read raw sensor data via IIO
        if (read_sensor_data(device_dir, accel, gyro, mag) < 0)
        {
            fprintf(stderr, "Error reading sensor data\n");
            usleep(100000); // Wait a bit before retrying
            continue;
        }

        // Apply calibration offsets
        for (int i = 0; i < 3; i++)
        {
            gyro[i] -= gyro_offsets[i];
            mag[i] -= mag_offsets[i];
        }

        // Calculate orientation
        calculate_orientation(accel, gyro, mag, shared_data);

        // Set updated flag to indicate new data
        shared_data->updated = 1;

        // Print data (for testing)
        printf("Roll: %.2f°, Pitch: %.2f°, Yaw: %.2f°\n",
               shared_data->roll, shared_data->pitch, shared_data->yaw);

        usleep(10000); // 100Hz update rate
    }

    // Cleanup - should not reach here normally
    munmap(shared_data, sizeof(SharedData));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    close(calibration_fifo_fd);

    return 0;
}

void setup_calibration_pipe()
{
    // Create named pipe if it doesn't exist
    mkfifo(FIFO_PATH, 0666);

    // Open pipe for non-blocking reads
    calibration_fifo_fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (calibration_fifo_fd < 0)
    {
        perror("Failed to open calibration FIFO");
    }
}

void check_calibration_request()
{
    char buffer[32];
    int bytes_read;

    if (calibration_fifo_fd < 0)
        return;

    bytes_read = read(calibration_fifo_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        if (strstr(buffer, CALIBRATE_CMD) != NULL)
        {
            printf("Calibration requested\n");
            calibration_requested = 1;
        }
    }
}

void calibrate_sensor(const char *device_dir)
{
    float accel[3], gyro[3], mag[3];
    float gyro_sum[3] = {0}, mag_sum[3] = {0};
    int valid_samples = 0;

    printf("Starting sensor calibration. Keep the device stationary...\n");

    // Reset offsets
    for (int i = 0; i < 3; i++)
    {
        gyro_offsets[i] = 0;
        mag_offsets[i] = 0;
    }

    // Collect samples
    for (int i = 0; i < CALIBRATION_SAMPLES; i++)
    {
        if (read_sensor_data(device_dir, accel, gyro, mag) == 0)
        {
            for (int j = 0; j < 3; j++)
            {
                gyro_sum[j] += gyro[j];
                mag_sum[j] += mag[j];
            }
            valid_samples++;
        }
        usleep(CALIBRATION_DELAY_MS * 1000);
    }

    // Calculate average offsets if we got valid samples
    if (valid_samples > 0)
    {
        for (int i = 0; i < 3; i++)
        {
            gyro_offsets[i] = gyro_sum[i] / valid_samples;
            mag_offsets[i] = mag_sum[i] / valid_samples;
        }
    }

    printf("Calibration complete. Gyro offsets: [%.4f, %.4f, %.4f]\n",
           gyro_offsets[0], gyro_offsets[1], gyro_offsets[2]);
}

int find_iio_device(char *device_dir, size_t max_len)
{
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char name[64];
    FILE *nameFile;
    int found = 0;

    // Open IIO directory
    dir = opendir(IIO_DIR);
    if (!dir)
    {
        perror("Cannot open IIO directory");
        return -1;
    }

    // Look for MPU9250 device
    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "iio:device", 10) == 0)
        {
            // Check if this is our device by reading the name
            snprintf(path, sizeof(path), IIO_DIR "%s/name", entry->d_name);
            nameFile = fopen(path, "r");
            if (nameFile)
            {
                if (fgets(name, sizeof(name), nameFile) != NULL)
                {
                    // Remove trailing newline
                    char *newline = strchr(name, '\n');
                    if (newline)
                        *newline = '\0';

                    // Check if it's our device (MPU9250 or AK8963)
                    if (strstr(name, "mpu9250") || strstr(name, "mpu925") ||
                        strstr(name, "ak8963") || strstr(name, "ak896"))
                    {
                        snprintf(device_dir, max_len, IIO_DIR "%s", entry->d_name);
                        found = 1;
                    }
                }
                fclose(nameFile);

                if (found)
                    break;
            }
        }
    }

    closedir(dir);
    return found ? 0 : -1;
}

int read_sysfs_float(const char *dir, const char *file, float *value)
{
    FILE *f;
    char path[512];
    char buf[64];

    snprintf(path, sizeof(path), "%s/%s", dir, file);
    f = fopen(path, "r");
    if (!f)
    {
        // Don't print error for non-existent files - many IIO attributes are optional
        if (errno != ENOENT)
        {
            fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        }
        return -1;
    }

    if (fgets(buf, sizeof(buf), f) == NULL)
    {
        fclose(f);
        return -1;
    }

    *value = atof(buf);
    fclose(f);
    return 0;
}

int read_scale_and_offset(const char *device_dir, const char *channel, float *scale, float *offset)
{
    char scale_path[64], offset_path[64];

    // Set default values in case files don't exist
    *scale = 1.0f;
    *offset = 0.0f;

    snprintf(scale_path, sizeof(scale_path), "%s_scale", channel);
    snprintf(offset_path, sizeof(offset_path), "%s_offset", channel);

    // Try to read scale and offset (they may not exist for all channels)
    read_sysfs_float(device_dir, scale_path, scale);
    read_sysfs_float(device_dir, offset_path, offset);

    return 0;
}

int read_sensor_value(const char *device_dir, const char *channel, float *value)
{
    char raw_path[64];
    float raw_value, scale, offset;

    snprintf(raw_path, sizeof(raw_path), "in_%s_raw", channel);

    if (read_sysfs_float(device_dir, raw_path, &raw_value) < 0)
    {
        return -1;
    }

    // Get scale and offset
    read_scale_and_offset(device_dir, channel, &scale, &offset);

    // Apply scale and offset
    *value = raw_value * scale + offset;

    return 0;
}

int read_sensor_data(const char *device_dir, float *accel, float *gyro, float *mag)
{
    // Read accelerometer data
    if (read_sensor_value(device_dir, "accel_x", &accel[0]) < 0 ||
        read_sensor_value(device_dir, "accel_y", &accel[1]) < 0 ||
        read_sensor_value(device_dir, "accel_z", &accel[2]) < 0)
    {
        fprintf(stderr, "Failed to read accelerometer data\n");
        return -1;
    }

    // Read gyroscope data
    if (read_sensor_value(device_dir, "anglvel_x", &gyro[0]) < 0 ||
        read_sensor_value(device_dir, "anglvel_y", &gyro[1]) < 0 ||
        read_sensor_value(device_dir, "anglvel_z", &gyro[2]) < 0)
    {
        fprintf(stderr, "Failed to read gyroscope data\n");
        return -1;
    }

    // Read magnetometer data
    // Note: IIO might use magn instead of magnet for magnetometer channels
    if (read_sensor_value(device_dir, "magn_x", &mag[0]) < 0)
    {
        // Try alternative naming
        if (read_sensor_value(device_dir, "magnet_x", &mag[0]) < 0)
        {
            fprintf(stderr, "Failed to read magnetometer x-axis data\n");
            // Continue anyway, we'll use what we have
        }
    }

    if (read_sensor_value(device_dir, "magn_y", &mag[1]) < 0)
    {
        if (read_sensor_value(device_dir, "magnet_y", &mag[1]) < 0)
        {
            fprintf(stderr, "Failed to read magnetometer y-axis data\n");
        }
    }

    if (read_sensor_value(device_dir, "magn_z", &mag[2]) < 0)
    {
        if (read_sensor_value(device_dir, "magnet_z", &mag[2]) < 0)
        {
            fprintf(stderr, "Failed to read magnetometer z-axis data\n");
        }
    }

    return 0;
}

void calculate_orientation(float *accel, float *gyro, float *mag, SharedData *data)
{
    static float roll = 0, pitch = 0, yaw = 0;

    // Calculate roll and pitch from accelerometer
    float accel_roll = atan2f(accel[1], sqrtf(accel[0] * accel[0] + accel[2] * accel[2])) * 180.0 / PI;
    float accel_pitch = atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2])) * 180.0 / PI;

    // Calculate yaw from magnetometer
    float mag_x = mag[0] * cosf(pitch * PI / 180.0) +
                  mag[2] * sinf(pitch * PI / 180.0);
    float mag_y = mag[0] * sinf(roll * PI / 180.0) * sinf(pitch * PI / 180.0) +
                  mag[1] * cosf(roll * PI / 180.0) -
                  mag[2] * sinf(roll * PI / 180.0) * cosf(pitch * PI / 180.0);
    float mag_yaw = atan2f(mag_y, mag_x) * 180.0 / PI;

    // Integrate gyroscope data
    roll = roll + gyro[0] * DT;
    pitch = pitch + gyro[1] * DT;
    yaw = yaw + gyro[2] * DT;

    // Complementary filter
    roll = ALPHA * roll + (1 - ALPHA) * accel_roll;
    pitch = ALPHA * pitch + (1 - ALPHA) * accel_pitch;
    yaw = ALPHA * yaw + (1 - ALPHA) * mag_yaw;

    // Normalize yaw to 0-360 degrees
    while (yaw < 0)
        yaw += 360;
    while (yaw >= 360)
        yaw -= 360;

    // Store results
    data->roll = roll;
    data->pitch = pitch;
    data->yaw = yaw;
}