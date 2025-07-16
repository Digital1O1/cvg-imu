// tracks the direction the glasses are facing to turn the laser off when the glasses turn to look away from straight down

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <dirent.h> // Removed, not used
#include <math.h>
#include <fcntl.h>
#include <iio.h>
#include <sys/stat.h>

#define GRAVITY_CHANNELS 3
static const char *GRAVITY_NAMES[GRAVITY_CHANNELS] = {"gravity_x_raw", "gravity_y_raw", "gravity_z_raw"};
// Calibration: set to {0,0,0} initially, then update after calibration
static float GRAVITY_OFFSET[3] = {1.279940f, 0.227644f, -0.084857f};


void calibrate_gravity_offset(struct iio_device *dev) {
    const int samples = 100;
    float gravity[3] = {0, 0, 0};
    float sum[3] = {0, 0, 0};
    float scale[3] = {0.0000001f, 0.0000001f, 0.0000001f};
    float offset[3] = {0, 0, 0};
    // Find gravity channels inside the function
    struct iio_channel *gravity_ch[GRAVITY_CHANNELS];
    for (int j = 0; j < GRAVITY_CHANNELS; j++) {
        gravity_ch[j] = iio_device_find_channel(dev, GRAVITY_NAMES[j], false);
        if (!gravity_ch[j]) {
            fprintf(stderr, "Could not find channel %s\n", GRAVITY_NAMES[j]);
            return;
        }
    }
    // Only read offset for each channel, not scale
    for (int j = 0; j < GRAVITY_CHANNELS; j++) {
        double o = 0;
        if (iio_channel_attr_read_double(gravity_ch[j], "offset", &o) < 0) o = 0.0;
        offset[j] = (float)o;
    }
    printf("\nCalibration: Point the glasses straight down and press Enter.\n");
    getchar();
    printf("Calibrating... Please hold still.\n");
    fflush(stdout);
    for (int i = 0; i < samples; i++) {
        for (int j = 0; j < GRAVITY_CHANNELS; j++) {
            double raw = 0;
            if (iio_channel_attr_read_double(gravity_ch[j], "raw", &raw) < 0) {
                fprintf(stderr, "Error reading raw for %s\n", GRAVITY_NAMES[j]);
                raw = 0;
            }
            gravity[j] = (float)(raw * scale[j] + offset[j]);
            sum[j] += gravity[j];
        }
        usleep(20000); // 20 ms
    }
    float avg[3];
    for (int j = 0; j < 3; j++) avg[j] = sum[j] / samples;
    // Expected gravity vector is [0, 0, 10] (down)
    float expected[3] = {0.0f, 0.0f, 10.0f};
    float out_offset[3];
    for (int j = 0; j < 3; j++) out_offset[j] = avg[j] - expected[j];
    printf("\nCalibration complete. Use this for GRAVITY_OFFSET in your code:\n");
    printf("static float GRAVITY_OFFSET[3] = {%.6ff, %.6ff, %.6ff};\n", out_offset[0], out_offset[1], out_offset[2]);
    printf("\nExiting calibration.\n");
    exit(0);
}

int main() {
    // Ensure the named pipe exists
    if (access("/tmp/hmdop_laser_pipe", F_OK) == -1) {
        if (mkfifo("/tmp/hmdop_laser_pipe", 0666) != 0) {
            perror("Failed to create FIFO pipe");
            return 1;
        }
    }
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
    int was_in_range = 1;
    // Uncomment the following line to run gravity offset calibration, then copy the output to GRAVITY_OFFSET and comment it again.
    // calibrate_gravity_offset(dev);
    while (1) {
        ssize_t nbytes = iio_buffer_refill(buf);
        if (nbytes < 0) {
            fprintf(stderr, "Buffer refill failed: %zd\n", nbytes);
            break;
        }
        float gravity[3] = {0};
        for (unsigned int i = 0, g = 0; i < num_channels && g < 3; i++) {
            struct iio_channel *ch = iio_device_get_channel(dev, i);
            if (!iio_channel_is_enabled(ch)) continue;
            void *data = iio_buffer_first(buf, ch);
            int32_t value = *(int32_t *)data;
            gravity[g] = value * 0.0000001f - GRAVITY_OFFSET[g]; // Use scale as before
	    g++;
        }
        float gmag = sqrt(gravity[0]*gravity[0] + gravity[1]*gravity[1] + gravity[2]*gravity[2]);
        float forward[3] = {0, 0, 1};
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
        fflush(stdout);
        usleep(10000); // 10 ms (100Hz)
    }
    iio_buffer_destroy(buf);
    iio_context_destroy(ctx);
    printf("\nStopping.\n");
    return 0;
} 
