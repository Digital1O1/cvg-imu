#include <stdio.h>
#include <string.h>
#include <iio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t stop = 0;
void handle_sigint(int sig) { stop = 1; }

typedef struct {
    float accel[3]; // m/s²
    float gyro[3];  // rad/s
    float mag[3];   // Gauss
} MPUData;

MPUData read_sensors(struct iio_device *dev)
{
    MPUData data = {{0}};
    const char *channel_names[9] = {
        "accel_x", "accel_y", "accel_z",
        "anglvel_x", "anglvel_y", "anglvel_z",
        "magn_x", "magn_y", "magn_z"
    };

    struct iio_channel *ch;
    for (int j = 0; j < 9; j++)
    {
        ch = iio_device_find_channel(dev, channel_names[j], false);
        if (!ch) {
            fprintf(stderr, "Could not find channel %s\n", channel_names[j]);
            continue;
        }

        double raw = 0, scale = 1;
        if (iio_channel_attr_read_double(ch, "raw", &raw) < 0)
        {
            fprintf(stderr, "Error reading raw for %s\n", channel_names[j]);
            continue;
        }
        if (iio_channel_attr_read_double(ch, "scale", &scale) < 0)
        {
            scale = 1.0;
        }

        double val = raw * scale;

        if (j < 3) data.accel[j] = (float)val;
        else if (j < 6) data.gyro[j - 3] = (float)val;
        else data.mag[j - 6] = (float)val;
    }

    return data;
}

void calculate_orientation(MPUData data, float *roll, float *pitch, float *yaw, float dt, float filter_alpha, float gyro_bias[3])
{
    static float gyro_roll = 0, gyro_pitch = 0, gyro_yaw = 0;

    // Accelerometer angles
    float accel_roll = atan2(data.accel[1], data.accel[2]);
    float accel_pitch = atan2(-data.accel[0], sqrt(data.accel[1]*data.accel[1] + data.accel[2]*data.accel[2]));

    // Magnetometer yaw (with tilt compensation)
    float mag_x = data.mag[0]*cos(accel_pitch) + data.mag[2]*sin(accel_pitch);
    float mag_y = data.mag[0]*sin(accel_roll)*sin(accel_pitch) + data.mag[1]*cos(accel_roll)
                  - data.mag[2]*sin(accel_roll)*cos(accel_pitch);
    float mag_yaw = atan2(-mag_y, mag_x);

    // Integrate gyro (bias-corrected)
    gyro_roll  += (data.gyro[0] - gyro_bias[0]) * dt;
    gyro_pitch += (data.gyro[1] - gyro_bias[1]) * dt;
    gyro_yaw   += (data.gyro[2] - gyro_bias[2]) * dt;

    // Complementary filter
    *roll  = filter_alpha * gyro_roll + (1.0f - filter_alpha) * accel_roll;
    *pitch = filter_alpha * gyro_pitch + (1.0f - filter_alpha) * accel_pitch;
    *yaw   = filter_alpha * gyro_yaw + (1.0f - filter_alpha) * mag_yaw;

    // Convert to degrees
    *roll  *= 180.0f / M_PI;
    *pitch *= 180.0f / M_PI;
    *yaw   *= 180.0f / M_PI;
}

int main()
{
    signal(SIGINT, handle_sigint);

    struct iio_context *ctx = iio_create_default_context();
    if (!ctx)
    {
        fprintf(stderr, "Failed to create IIO context.\n");
        return 1;
    }

    struct iio_device *dev = NULL;
    unsigned int dev_count = iio_context_get_devices_count(ctx);
    for (unsigned int i = 0; i < dev_count; i++)
    {
        dev = iio_context_get_device(ctx, i);
        const char *name = iio_device_get_name(dev);
        if (name && strstr(name, "mpu9250")) {
            printf("Using device: %s\n", name);
            break;
        }
        dev = NULL;
    }

    if (!dev) {
        fprintf(stderr, "No matching device found.\n");
        iio_context_destroy(ctx);
        return 1;
    }

    // Calibrate gyro bias
    float gyro_bias[3] = {0};
    printf("Calibrating gyro bias... keep sensor still for 2 seconds.\n");
    for (int i = 0; i < 200; i++) {
        MPUData d = read_sensors(dev);
        gyro_bias[0] += d.gyro[0];
        gyro_bias[1] += d.gyro[1];
        gyro_bias[2] += d.gyro[2];
        usleep(10000);
    }
    gyro_bias[0] /= 200.0f;
    gyro_bias[1] /= 200.0f;
    gyro_bias[2] /= 200.0f;
    printf("Gyro bias: %.6f %.6f %.6f\n", gyro_bias[0], gyro_bias[1], gyro_bias[2]);

    printf("Starting orientation tracking. Ctrl+C to stop.\n");

    while (!stop)
    {
        MPUData data = read_sensors(dev);

        float roll, pitch, yaw;
        calculate_orientation(data, &roll, &pitch, &yaw, 0.01f, 0.90f, gyro_bias);

        printf("\rRoll: %7.2f°  Pitch: %7.2f°  Yaw: %7.2f°", roll, pitch, yaw);
        fflush(stdout);

        usleep(10000); // 10 ms
    }

    printf("\nStopping.\n");
    iio_context_destroy(ctx);
    return 0;
}
