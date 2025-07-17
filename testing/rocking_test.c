// tracks the direction the glasses are facing to turn the laser off when the glasses turn to look away from straight down

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <iio.h>
#include <sys/stat.h>
#include <time.h>

static float GRAVITY_OFFSET[3] = {1.279940f, 0.227644f, -0.084857f};

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
    FILE *csv = fopen("rocking_peaks.csv", "w");
    if (!csv) {
        perror("Failed to open CSV file");
        iio_buffer_destroy(buf);
        iio_context_destroy(ctx);
        return 1;
    }
    fprintf(csv, "timestamp_ms,angle_deg,direction\n");
    fflush(csv);
    float angle_history[7] = {0};
    int history_idx = 0;
    int direction = 1; // 1 for right, -1 for left, alternates at each peak
    struct timespec last_peak_time = {0, 0};
    const long min_peak_interval_ms = 200; // 200 ms debounce
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
        // --- Peak detection logic (window of 7) ---
        angle_history[history_idx % 7] = angle_deg;
        history_idx++;
        int have_window = history_idx >= 7;
        int peak_detected = 0;
        if (have_window) {
            // Indices for 7-value window
            int c = (history_idx - 4) % 7; // center
            float center = angle_history[c];
            int is_peak = 1;
            for (int offset = -3; offset <= 3; offset++) {
                if (offset == 0) continue;
                int idx = (history_idx - 4 + offset + 7) % 7;
                if (center <= angle_history[idx]) {
                    is_peak = 0;
                    break;
                }
            }
            if (is_peak && center > 10.0f) {
                // Debounce: check time since last peak
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                long elapsed_ms = (now.tv_sec - last_peak_time.tv_sec) * 1000 + (now.tv_nsec - last_peak_time.tv_nsec) / 1000000;
                if (elapsed_ms > min_peak_interval_ms) {
                    peak_detected = 1;
                    last_peak_time = now;
                }
            }
        }
        if (peak_detected) {
            // Get timestamp in ms since epoch
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            long long timestamp_ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
            int c = (history_idx - 4) % 7;
            float peak_angle = angle_history[c];
            // format is milliseconds, degrees, direction
            fprintf(csv, "%lld,%.2f,%d\n", timestamp_ms, peak_angle, direction);
            fflush(csv);
            direction *= -1; // Alternate direction
        }
       usleep(10000); // 10 ms (100Hz)
    }
    fclose(csv);
    iio_buffer_destroy(buf);
    iio_context_destroy(ctx);
    printf("\nStopping.\n");
    return 0;
} 
