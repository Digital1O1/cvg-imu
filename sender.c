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
#define AK8963_WHO_AM_I    0x00
#define AK8963_INFO       0x01
#define AK8963_ST1        0x02
#define AK8963_XOUT_L     0x03
#define AK8963_XOUT_H     0x04
#define AK8963_YOUT_L     0x05
#define AK8963_YOUT_H     0x06
#define AK8963_ZOUT_L     0x07
#define AK8963_ZOUT_H     0x08
#define AK8963_ST2        0x09
#define AK8963_CNTL1      0x0A
#define AK8963_CNTL2      0x0B
#define AK8963_ASAX       0x10
#define AK8963_ASAY       0x11
#define AK8963_ASAZ       0x12

float mag_sensitivity_adj[3] = {0}; // Sensitivity adjustment values

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

    // Enable I2C bypass to access AK8963
    buf[0] = 0x37;  // INT_PIN_CFG
    buf[1] = 0x02;  // Enable I2C bypass
    if (write(fd, buf, 2) != 2) {
        perror("Failed to enable I2C bypass");
        close(fd);
        return -1;
    }

    // Initialize AK8963
    if (ioctl(fd, I2C_SLAVE, AK8963_ADDR) < 0) {
        perror("Failed to set AK8963 as slave");
        close(fd);
        return -1;
    }

    // Reset AK8963
    buf[0] = AK8963_CNTL2;
    buf[1] = 0x01;  // Reset
    write(fd, buf, 2);
    usleep(1000);  // Wait for reset

    // Enter Fuse ROM access mode
    buf[0] = AK8963_CNTL1;
    buf[1] = 0x0F;  // Fuse ROM access mode
    write(fd, buf, 2);
    usleep(1000);

    // Read sensitivity adjustment values
    buf[0] = AK8963_ASAX;
    write(fd, buf, 1);
    read(fd, buf, 3);
    
    // Calculate sensitivity adjustment values
    mag_sensitivity_adj[0] = (float)(buf[0] - 128) / 256.0f + 1.0f;
    mag_sensitivity_adj[1] = (float)(buf[1] - 128) / 256.0f + 1.0f;
    mag_sensitivity_adj[2] = (float)(buf[2] - 128) / 256.0f + 1.0f;

    // Enter continuous measurement mode 2 (100 Hz)
    buf[0] = AK8963_CNTL1;
    buf[1] = 0x16;  // 16-bit output, Continuous mode 2
    write(fd, buf, 2);
    usleep(1000);
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

    if (ioctl(fd, I2C_SLAVE, AK8963_ADDR) < 0) {
        perror("Failed to select AK8963");
        return;
    }

    //Check data ready
    buf[0] = AK8963_ST1;
    write(fd, buf, 1);
    read(fd, buf, 1);

    if(bug[0] & 0x01) {
        // Read magnetometer data
        buf[0] = AK8963_XOUT_L;
        write(fd, buf, 1);
        read(fd, buf, 7);

        // Convert raw data to float values
        // Magnetometer (16-bit output)
        mag[0] = (float)((short)(buf[1] << 8 | buf[0])) * mag_sensitivity_adj[0];
        mag[1] = (float)((short)(buf[3] << 8 | buf[2])) * mag_sensitivity_adj[1];
        mag[2] = (float)((short)(buf[5] << 8 | buf[4])) * mag_sensitivity_adj[2];
    }
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