// ran for ~3 hours
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <iio.h>
#include <time.h>

static float GRAVITY_OFFSET[3] = {1.279940f, 0.227644f, -0.084857f};

int main() {
    // --- libiio setup ---
    struct iio_context *ctx = iio_create_default_context();
    if (!ctx) {
        fprintf(stderr, "Failed to create IIO context.\n");
        return 1;
    }
    struct iio_device *dev = iio_context_find_device(ctx, "gravity");
    if (!dev) {
        fprintf(stderr, "Could not find gravity device.\n");
        iio_context_destroy(ctx);
        return 1;
    }
    // Enable all gravity channels
    unsigned int num_channels = iio_device_get_channels_count(dev);
    for (unsigned int i = 0; i < num_channels; i++) {
        struct iio_channel *ch = iio_device_get_channel(dev, i);
        iio_channel_enable(ch);
    }
    // Create buffer
    size_t buf_size = 1; // number of samples
    struct iio_buffer *buf = iio_device_create_buffer(dev, buf_size, false);
    if (!buf) {
        fprintf(stderr, "Could not create buffer.\n");
        iio_context_destroy(ctx);
        return 1;
    }
    FILE *csv = fopen("rocking_peaks.csv", "w");
    if (!csv) {
        perror("Failed to open CSV file");
        iio_buffer_destroy(buf);
        iio_context_destroy(ctx);
        return 1;
    }
    fprintf(csv, "timestamp_ms,angle_deg,raw_gravity_x,raw_gravity_y,raw_gravity_z,gravity_x,gravity_y,gravity_z,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,magn_x,magn_y,magn_z\n");
    fflush(csv);
    int32_t raw_gravity[3] = {0};
    float gravity[3] = {0};
    // --- Find and setup accel_3d, gyro_3d, magn_3d devices ---
    struct iio_device *accel_dev = iio_context_find_device(ctx, "accel_3d");
    struct iio_device *gyro_dev  = iio_context_find_device(ctx, "gyro_3d");
    struct iio_device *magn_dev  = iio_context_find_device(ctx, "magn_3d");
    struct iio_channel *accel_ch[3] = {NULL}, *gyro_ch[3] = {NULL}, *magn_ch[3] = {NULL};
    struct iio_buffer *accel_buf = NULL, *gyro_buf = NULL, *magn_buf = NULL;
    float accel[3] = {0}, gyro[3] = {0}, magn[3] = {0};
    // Helper macro to setup device, channels, and buffer
    #define SETUP_3D_DEV(dev, ch_arr, buf) \
        if (dev) { \
            for (int j = 0; j < 3; j++) { \
                char chname[32]; \
                snprintf(chname, sizeof(chname), "%s_%s", \
                    strstr(iio_device_get_name(dev), "accel") ? "accel" : \
                    strstr(iio_device_get_name(dev), "gyro") ? "anglvel" : "magn", \
                    (j == 0 ? "x" : (j == 1 ? "y" : "z"))); \
                ch_arr[j] = iio_device_find_channel(dev, chname, false); \
                if (ch_arr[j]) iio_channel_enable(ch_arr[j]); \
            } \
            buf = iio_device_create_buffer(dev, 1, false); \
        }
    SETUP_3D_DEV(accel_dev, accel_ch, accel_buf);
    SETUP_3D_DEV(gyro_dev,  gyro_ch,  gyro_buf);
    SETUP_3D_DEV(magn_dev,  magn_ch,  magn_buf);
    struct timespec last_log_time = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &last_log_time);
    while (1) {
        ssize_t nbytes = iio_buffer_refill(buf);
        if (nbytes < 0) {
            fprintf(stderr, "Buffer refill failed: %zd\n", nbytes);
            break;
        }
        for (unsigned int i = 0, g = 0; i < num_channels && g < 3; i++) {
            struct iio_channel *ch = iio_device_get_channel(dev, i);
            if (!iio_channel_is_enabled(ch)) continue;
            void *data = iio_buffer_first(buf, ch);
            int32_t value = *(int32_t *)data;
            raw_gravity[g] = value;
            gravity[g] = value * 0.0000001f - GRAVITY_OFFSET[g];
            g++;
        }
        // Read accel, gyro, magn scaled values
        #define READ_3D_BUF(dev, buf, ch_arr, arr) \
            if (dev && buf && iio_buffer_refill(buf) >= 0) { \
                for (int j = 0; j < 3; j++) { \
                    if (ch_arr[j]) { \
                        void *data = iio_buffer_first(buf, ch_arr[j]); \
                        int32_t raw = *(int32_t *)data; \
                        double scale = 1, offset = 0; \
                        iio_channel_attr_read_double(ch_arr[j], "scale", &scale); \
                        iio_channel_attr_read_double(ch_arr[j], "offset", &offset); \
                        arr[j] = raw * scale + offset; \
                    } else { arr[j] = 0; } \
                } \
            } else { arr[0]=arr[1]=arr[2]=0; }
        READ_3D_BUF(accel_dev, accel_buf, accel_ch, accel);
        READ_3D_BUF(gyro_dev,  gyro_buf,  gyro_ch,  gyro);
        READ_3D_BUF(magn_dev,  magn_buf,  magn_ch,  magn);
        // Log every 100 ms
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - last_log_time.tv_sec) * 1000 + (now.tv_nsec - last_log_time.tv_nsec) / 1000000;
        if (elapsed_ms >= 100) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            long long timestamp_ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
            float gmag = sqrt(gravity[0]*gravity[0] + gravity[1]*gravity[1] + gravity[2]*gravity[2]);
            float dot = gravity[2] / gmag;
            if (dot > 1.0f) dot = 1.0f;
            if (dot < -1.0f) dot = -1.0f;
            float angle_rad = acosf(dot);
            float angle_deg = angle_rad * 180.0f / M_PI;
            fprintf(csv, "%lld,%.2f,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", timestamp_ms, angle_deg, raw_gravity[0], raw_gravity[1], raw_gravity[2], gravity[0], gravity[1], gravity[2], accel[0], accel[1], accel[2], gyro[0], gyro[1], gyro[2], magn[0], magn[1], magn[2]);
            fflush(csv);
            last_log_time = now;
        }
        usleep(10000); // 10 ms (100Hz)
    }
    fclose(csv);
    iio_buffer_destroy(buf);
    iio_context_destroy(ctx);
    if (accel_buf) iio_buffer_destroy(accel_buf);
    if (gyro_buf)  iio_buffer_destroy(gyro_buf);
    if (magn_buf)  iio_buffer_destroy(magn_buf);
    printf("\nStopping.\n");
    return 0;
} 
