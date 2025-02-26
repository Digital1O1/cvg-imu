#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <mqueue.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#define IIO_DIR "/sys/bus/iio/devices/"
#define QUEUE_NAME "/sensor_data"
#define PI 3.14159265358979323846

// Sensor data structure
typedef struct {
    float roll;
    float pitch;
    float yaw;
} SensorData;

// Complementary filter parameters
#define ALPHA 0.96
#define DT 0.01  // 100Hz sample rate

// Function prototypes
int find_iio_device(char *device_dir, size_t max_len);
int read_sensor_data(const char *device_dir, float *accel, float *gyro, float *mag);
void calculate_orientation(float *accel, float *gyro, float *mag, SensorData *data);
int read_sysfs_float(const char *dir, const char *file, float *value);
int read_scale_and_offset(const char *device_dir, const char *channel, float *scale, float *offset);

int main() {
    float accel[3] = {0}, gyro[3] = {0}, mag[3] = {0};
    SensorData data;
    mqd_t mq;
    struct mq_attr attr;
    char device_dir[256];
    
    // Find IIO device
    if (find_iio_device(device_dir, sizeof(device_dir)) < 0) {
        printf("Failed to find MPU9250 IIO device\n");
        return -1;
    }
    
    printf("Found IIO device: %s\n", device_dir);

    // Initialize message queue
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(SensorData);
    attr.mq_curmsgs = 0;

    mq = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        return -1;
    }

    printf("Starting sensor readings...\n");

    while (1) {
        // Read raw sensor data via IIO
        if (read_sensor_data(device_dir, accel, gyro, mag) < 0) {
            fprintf(stderr, "Error reading sensor data\n");
            usleep(100000);  // Wait a bit before retrying
            continue;
        }
        
        // Calculate orientation
        calculate_orientation(accel, gyro, mag, &data);
        
        // Print data (for testing)
        printf("Roll: %.2f°, Pitch: %.2f°, Yaw: %.2f°\n",
               data.roll, data.pitch, data.yaw);
        
        // Send data through message queue
        if (mq_send(mq, (char *)&data, sizeof(SensorData), 0) == -1) {
            perror("mq_send");
        }
        
        usleep(10000);  // 100Hz update rate
    }

    // Cleanup
    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    
    return 0;
}

int find_iio_device(char *device_dir, size_t max_len) {
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char name[64];
    FILE *nameFile;
    int found = 0;
    
    // Open IIO directory
    dir = opendir(IIO_DIR);
    if (!dir) {
        perror("Cannot open IIO directory");
        return -1;
    }
    
    // Look for MPU9250 device
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) == 0) {
            // Check if this is our device by reading the name
            snprintf(path, sizeof(path), IIO_DIR "%s/name", entry->d_name);
            nameFile = fopen(path, "r");
            if (nameFile) {
                if (fgets(name, sizeof(name), nameFile) != NULL) {
                    // Remove trailing newline
                    char *newline = strchr(name, '\n');
                    if (newline) *newline = '\0';
                    
                    // Check if it's our device (MPU9250 or AK8963)
                    if (strstr(name, "mpu9250") || strstr(name, "mpu925") || 
                        strstr(name, "ak8963") || strstr(name, "ak896")) {
                        snprintf(device_dir, max_len, IIO_DIR "%s", entry->d_name);
                        found = 1;
                    }
                }
                fclose(nameFile);
                
                if (found) break;
            }
        }
    }
    
    closedir(dir);
    return found ? 0 : -1;
}

int read_sysfs_float(const char *dir, const char *file, float *value) {
    FILE *f;
    char path[512];
    char buf[64];
    
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    f = fopen(path, "r");
    if (!f) {
        // Don't print error for non-existent files - many IIO attributes are optional
        if (errno != ENOENT) {
            fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        }
        return -1;
    }
    
    if (fgets(buf, sizeof(buf), f) == NULL) {
        fclose(f);
        return -1;
    }
    
    *value = atof(buf);
    fclose(f);
    return 0;
}

int read_scale_and_offset(const char *device_dir, const char *channel, float *scale, float *offset) {
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

int read_sensor_value(const char *device_dir, const char *channel, float *value) {
    char raw_path[64];
    float raw_value, scale, offset;
    
    snprintf(raw_path, sizeof(raw_path), "in_%s_raw", channel);
    
    if (read_sysfs_float(device_dir, raw_path, &raw_value) < 0) {
        return -1;
    }
    
    // Get scale and offset
    read_scale_and_offset(device_dir, channel, &scale, &offset);
    
    // Apply scale and offset
    *value = raw_value * scale + offset;
    
    return 0;
}

int read_sensor_data(const char *device_dir, float *accel, float *gyro, float *mag) {
    // Read accelerometer data
    if (read_sensor_value(device_dir, "accel_x", &accel[0]) < 0 ||
        read_sensor_value(device_dir, "accel_y", &accel[1]) < 0 ||
        read_sensor_value(device_dir, "accel_z", &accel[2]) < 0) {
        fprintf(stderr, "Failed to read accelerometer data\n");
        return -1;
    }
    
    // Read gyroscope data
    if (read_sensor_value(device_dir, "anglvel_x", &gyro[0]) < 0 ||
        read_sensor_value(device_dir, "anglvel_y", &gyro[1]) < 0 ||
        read_sensor_value(device_dir, "anglvel_z", &gyro[2]) < 0) {
        fprintf(stderr, "Failed to read gyroscope data\n");
        return -1;
    }
    
    // Read magnetometer data
    // Note: IIO might use magn instead of magnet for magnetometer channels
    if (read_sensor_value(device_dir, "magn_x", &mag[0]) < 0) {
        // Try alternative naming
        if (read_sensor_value(device_dir, "magnet_x", &mag[0]) < 0) {
            fprintf(stderr, "Failed to read magnetometer x-axis data\n");
            // Continue anyway, we'll use what we have
        }
    }
    
    if (read_sensor_value(device_dir, "magn_y", &mag[1]) < 0) {
        if (read_sensor_value(device_dir, "magnet_y", &mag[1]) < 0) {
            fprintf(stderr, "Failed to read magnetometer y-axis data\n");
        }
    }
    
    if (read_sensor_value(device_dir, "magn_z", &mag[2]) < 0) {
        if (read_sensor_value(device_dir, "magnet_z", &mag[2]) < 0) {
            fprintf(stderr, "Failed to read magnetometer z-axis data\n");
        }
    }
    
    return 0;
}

void calculate_orientation(float *accel, float *gyro, float *mag, SensorData *data) {
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
    while (yaw < 0) yaw += 360;
    while (yaw >= 360) yaw -= 360;
    
    // Store results
    data->roll = roll;
    data->pitch = pitch;
    data->yaw = yaw;
}