#include <stdio.h>  // printf(), fprintf()
#include <string.h> // strstr()
#include <iio.h>    // iio_create_default_context(), iio_context_get_devices_count(), iio_context_get_device(), iio_device_get_name(), iio_device_find_channel(), iio_channel_attr_read_double()
#include <math.h>   // atan2(), sqrt(), sin(), cos(), M_PI

// Struct to hold sensor data
typedef struct
{
    float accel[3];
    float gyro[3];
    float mag[3];
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
    static float gyro_roll = 0.0f, gyro_pitch = 0.0f, gyro_yaw = 0.0f;
}