#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>
#include <mqueue.h>
#include <string.h>
#include <time.h>

#define MPU9250_ADDR 0x68
#define AK8963_ADDR 0x0C
#define I2C_BUS "/dev/i2c-1"
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
int init_i2c(void);
void read_sensor_data(int fd, float *accel, float *gyro, float *mag);
void calculate_orientation(float *accel, float *gyro, float *mag, SensorData *data);

int main() {
    int fd;
    float accel[3], gyro[3], mag[3];
    SensorData data;
    mqd_t mq;
    struct mq_attr attr;
    
    // Initialize I2C
    fd = init_i2c();
    if (fd < 0) {
        printf("Failed to initialize I2C\n");
        return -1;
    }

    // Initialize message queue
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(SensorData);
    attr.mq_curmsgs = 0;

    mq = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        close(fd);
        return -1;
    }

    printf("Starting sensor readings...\n");

    while (1) {
        // Read raw sensor data
        read_sensor_data(fd, accel, gyro, mag);
        
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
    close(fd);
    return 0;
}

int init_i2c(void) {
    int fd;
    
    // Open I2C bus
    fd = open(I2C_BUS, O_RDWR);
    if (fd < 0) {
        perror("Failed to open I2C bus");
        return -1;
    }

    // Set MPU9250 as slave device
    if (ioctl(fd, I2C_SLAVE, MPU9250_ADDR) < 0) {
        perror("Failed to set MPU9250 as slave");
        close(fd);
        return -1;
    }

    // Initialize MPU9250 (basic configuration)
    // Write to power management register to wake up
    char buf[2] = {0x6B, 0x00};
    if (write(fd, buf, 2) != 2) {
        perror("Failed to wake up MPU9250");
        close(fd);
        return -1;
    }

    return fd;
}

void read_sensor_data(int fd, float *accel, float *gyro, float *mag) {
    unsigned char buf[14];
    
    // Read accelerometer and gyroscope data
    buf[0] = 0x3B;  // Starting register for accel data
    write(fd, buf, 1);
    read(fd, buf, 14);

    // Convert raw data to float values
    // Accelerometer (±8g scale)
    accel[0] = (float)((short)(buf[0] << 8 | buf[1])) / 4096.0;
    accel[1] = (float)((short)(buf[2] << 8 | buf[3])) / 4096.0;
    accel[2] = (float)((short)(buf[4] << 8 | buf[5])) / 4096.0;

    // Gyroscope (±1000°/s scale)
    gyro[0] = (float)((short)(buf[8] << 8 | buf[9])) / 32.8;
    gyro[1] = (float)((short)(buf[10] << 8 | buf[11])) / 32.8;
    gyro[2] = (float)((short)(buf[12] << 8 | buf[13])) / 32.8;

    // Simplified magnetometer reading (in practice, you'd need more complex initialization and reading)
    mag[0] = mag[1] = mag[2] = 0;  // Placeholder
}

void calculate_orientation(float *accel, float *gyro, float *mag, SensorData *data) {
    static float roll = 0, pitch = 0, yaw = 0;
    
    // Calculate roll and pitch from accelerometer
    float accel_roll = atan2f(accel[1], sqrtf(accel[0] * accel[0] + accel[2] * accel[2])) * 180.0 / PI;
    float accel_pitch = atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2])) * 180.0 / PI;
    
    // Integrate gyroscope data
    roll = roll + gyro[0] * DT;
    pitch = pitch + gyro[1] * DT;
    yaw = yaw + gyro[2] * DT;
    
    // Complementary filter
    roll = ALPHA * roll + (1 - ALPHA) * accel_roll;
    pitch = ALPHA * pitch + (1 - ALPHA) * accel_pitch;
    
    // Store results
    data->roll = roll;
    data->pitch = pitch;
    data->yaw = yaw;
}