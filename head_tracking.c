#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <iio.h>

#define GRAVITY_CHANNELS 3
static const char *GRAVITY_NAMES[GRAVITY_CHANNELS] = {"gravity_x_raw", "gravity_y_raw", "gravity_z_raw"};
// Calibration: set to {0,0,0} initially, then update after calibration
static float GRAVITY_OFFSET[3] = {1.279940f, 0.227644f, -0.084857f};

// Remove sysfs helpers and directory search

void calibrate_gravity_offset(const char *gravity_dir) {
    const int samples = 100;
    int raw_gravity[3];
    const char *axes[3] = {"x", "y", "z"};
    float gravity[3] = {0, 0, 0};
    float sum[3] = {0, 0, 0};
    float gravity_scale = 0.0000001f; // Use this scale instead of sysfs value
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
    // --- libiio setup ---
    struct iio_context *ctx = iio_create_default_context();
    if (!ctx) {
        fprintf(stderr, "Failed to create IIO context.\n");
        return 1;
    }
    struct iio_device *dev = NULL;
    unsigned int dev_count = iio_context_get_devices_count(ctx);
    for (unsigned int i = 0; i < dev_count; i++) {
        dev = iio_context_get_device(ctx, i);
        // Look for device with gravity_x channel
        struct iio_channel *ch = iio_device_find_channel(dev, "gravity_x", false);
        if (ch) break;
        dev = NULL;
    }
    if (!dev) {
        fprintf(stderr, "No IIO device with gravity_x channel found.\n");
        iio_context_destroy(ctx);
        return 1;
    }
    // Find gravity channels
    struct iio_channel *gravity_ch[GRAVITY_CHANNELS];
    for (int j = 0; j < GRAVITY_CHANNELS; j++) {
        gravity_ch[j] = iio_device_find_channel(dev, GRAVITY_NAMES[j], false);
        if (!gravity_ch[j]) {
            fprintf(stderr, "Could not find channel %s\n", GRAVITY_NAMES[j]);
            iio_context_destroy(ctx);
            return 1;
        }
    }
    // --- Main loop ---
    float gravity[3];
    float scale[3] = {0.0000001f, 0.0000001f, 0.0000001f};
    float offset[3] = {0, 0, 0};
    // Only read offset for each channel, not scale
    for (int j = 0; j < GRAVITY_CHANNELS; j++) {
        double o = 0;
        if (iio_channel_attr_read_double(gravity_ch[j], "offset", &o) < 0) o = 0.0;
        offset[j] = (float)o;
    }
    float forward[3] = {0, 0, 1};
    int was_in_range = 1;
    while (1) {
        for (int j = 0; j < GRAVITY_CHANNELS; j++) {
            double raw = 0;
            if (iio_channel_attr_read_double(gravity_ch[j], "raw", &raw) < 0) {
                fprintf(stderr, "Error reading raw for %s\n", GRAVITY_NAMES[j]);
                raw = 0;
            }
            gravity[j] = (float)(raw * scale[j] + offset[j] - GRAVITY_OFFSET[j]);
        }
        float gmag = sqrt(gravity[0]*gravity[0] + gravity[1]*gravity[1] + gravity[2]*gravity[2]);
        float fmag = 1.0f;
        float dot = (gravity[0]*forward[0] + gravity[1]*forward[1] + gravity[2]*forward[2]) / (gmag * fmag);
        if (dot > 1.0f) dot = 1.0f;
        if (dot < -1.0f) dot = -1.0f;
        float angle_rad = acosf(dot);
        float angle_deg = angle_rad * 180.0f / M_PI;
        printf("\rGravity: [%.4f %.4f %.4f] | Magnitude: %.4f | Angle: %.2f deg   ", gravity[0], gravity[1], gravity[2], gmag, angle_deg);
        int in_range = (angle_deg <= 50.0f);
        if (in_range) {
            printf("Within range   ");
        } else {
            printf("Outside range   ");
            if (was_in_range) {
                int pipe_fd = open("/tmp/hmdop_laser_pipe", O_WRONLY | O_NONBLOCK);
                if (pipe_fd >= 0) {
                    const char *msg = "LASER_OFF\n";
                    write(pipe_fd, msg, strlen(msg));
                    close(pipe_fd);
                }
            }
        }
        was_in_range = in_range;
        usleep(10000); // 10 ms (100Hz)
    }
    iio_context_destroy(ctx);
    printf("\nStopping.\n");
    return 0;
} 
