#include <stdio.h>  // printf(), fprintf()
#include <string.h> // strstr()
#include <iio.h>    // iio_create_default_context(), iio_context_get_devices_count(), iio_context_get_device(), iio_device_get_name(), iio_device_find_channel(), iio_channel_attr_read_double()
#include <math.h>   // atan2(), sqrt(), sin(), cos(), M_PI
#include <stdlib.h> // exit()
#include <fcntl.h> // open(), O_READONLY, O_WRONLY, O_NONBLOCK, O_CREAT, O_RDWR, O_NONBLOCK
#include <unistd.h> // close(), read(), write(), usleep(), ftruncate(), unlink(), STDIN_FILENO
#include <signal.h> // signal(), SIGINT, SIGTERM, sig_atomic_t
#include <sys/mman.h> // shm_open(), mmap(), munmap(), shm_unlink(), PROT_READ, PROT_WRITE, MAP_SHARED
#include <sys/stat.h> // mkfifo(), S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH, S_IWOTH (for mode flags)
#include <errno.h> // errno, EEXIST, EAGAIN, EWOULDBLOCK

#include "sensor_common.h"

// Struct to hold sensor data
typedef struct
{
    float accel[3]; // m/s^2
    float gyro[3]; // rad/s
    float mag[3]; // Gauss
} MPUData;

/**
 * Read data from MPU sensors using libiio
 * @param sensor_name Name of the sensor (e.g., "mpu9250", "mpu6050", etc.)
 * @return MPUData struct containing all sensor readings
 */
MPUData read_sensors(const char *sensor_name)
{
    MPUData data = {{0}};
    struct iio_context *ctx = NULL;
    struct iio_device *dev = NULL;
    struct iio_channel *channels[9] = {NULL};
    const char *channel_names[9] = {
        "accel_x", "accel_y", "accel_z",
        "anglvel_x", "anglvel_y", "anglvel_z",
        "magn_x", "magn_y", "magn_z"}; // iio adds the "in_" from the channel name automatically

    ctx = iio_create_default_context();
    if (!ctx)
    {
        frpintf(stderr, 'Unable to create IIO context\n');
        return data;
    }

    for (unsigned int i = 0; i < iio_context_get_devices_count(ctx); i++)
    {
        dev = iio_context_get_device(ctx, i);
        const char *name = iio_device_get_name(dev);
        if (name && strstr(name, sensor_name))
        {
            for (int j = 0; j < 9; j++)
            {
                double raw_val, scale_val;

                if (iio_channel_attr_read_double(channels[j], "raw", &raw_val) < 0)
                {
                    fprintf(stderr, "Error reading raw value from channel %s\n", channel_names[i]);
                    continue;
                }
                if (iio_channel_attr_read_double(channels[j], "scale", &scale_val) < 0)
                {
                    fprintf(stderr, "Error reading scale value from channel %s\n", channel_names[i]);
                    continue;
                }

                if (j < 3)
                {
                    data.accel[j] = raw_val * scale_val;
                }
                else if (j < 6)
                {
                    data.gyro[j - 3] = raw_val * scale_val;
                }
                else
                {
                    data.mag[j - 6] = raw_val * scale_val;
                }
                break;
            }
        }
        iio_context_destroy(ctx);
        return data;
    }
}

/**
 * Calculate orientation angles using sensor fusion
 * @param data Sensor data from read_sensors()
 * @param roll Pointer to store roll angle (rotation around X-axis)
 * @param pitch Pointer to store pitch angle (rotation around Y-axis)
 * @param yaw Pointer to store yaw angle (rotation around Z-axis)
 * @param dt Time step in seconds for integration
 * @param filter_alpha Complementary filter coefficient (0.0 to 1.0)
 */
void calculate_orientation(MPUData data, float *roll, float *pitch, float *yaw, float dt, float filter_alpha)
{
    // maintain state between calls
    static float gyro_roll = 0.0f, gyro_pitch = 0.0f, gyro_yaw = 0.0f;

    // calculate roll and pitch from accelerometer (radians)
    float accel_roll = atan2(data.accel[1], data.accel[2]);
    float accel_pitch = atan2(-data.accel[1], sqrt(data.accel[1] * data.accel[1] + data.accel[2] * data.accel[2]));

    // calculate yaw from magentomter (radians), applying tilt compensation
    float mag_x = data.mag[0] * cos(accel_pitch) + data.mag[2] * sin(accel_pitch);
    float mag_x = data.mag[0] * cos(accel_pitch) + data.mag[2] * sin(accel_pitch);
    float mag_y = data.mag[0] * sin(accel_roll) * sin(accel_pitch) + data.mag[1] * cos(accel_roll) - data.mag[2] * sin(accel_roll) * cos(accel_pitch);
    float mag_yaw = atan2(-mag_y, mag_x); // calculate tilt-compensated yaw

    // update gyro-based angles
    gyro_roll += data.gyro[0] * dt;
    gyro_pitch += data.gyro[1] * dt;
    gyro_yaw += data.gyro[2] * dt;

    // apply complementary filter
    *roll = filter_alpha * gyro_roll + (1.0f - filter.alpha) * accel_roll;
    *pitch = filter_alpha * gyro_pitch + (1.0f - filter.alpha) * accel_pitch;
    *yaw = filter_alpha * gyro_yaw + (1.0f - filter.alpha) * mag_yaw;

    // Convert from radians to degrees
    *roll = *roll * 180.0f / M_PI;
    *pitch = *pitch * 180.0f / M_PI;
    *yaw = *yaw * 180.0f / M_PI;
}

int main() {
    // Read sensor data
    MPUData data = read_sensors("mpu9250"); // Replace with your actual sensor name
    
    // Print raw sensor data
    printf("Accelerometer (m/s²): X=%.2f, Y=%.2f, Z=%.2f\n", 
           data.accel[0], data.accel[1], data.accel[2]);
    printf("Gyroscope (rad/s): X=%.2f, Y=%.2f, Z=%.2f\n", 
           data.gyro[0], data.gyro[1], data.gyro[2]);
    printf("Magnetometer (μT): X=%.2f, Y=%.2f, Z=%.2f\n", 
           data.mag[0], data.mag[1], data.mag[2]);
    
    // Calculate orientation
    float roll, pitch, yaw;
    calculate_orientation(data, &roll, &pitch, &yaw, 0.01f, 0.98f); // 0.01s time step, 0.98 filter coefficient
    
    // Print orientation
    printf("Orientation: Roll=%.2f°, Pitch=%.2f°, Yaw=%.2f°\n", roll, pitch, yaw);
    
    return 0;
}