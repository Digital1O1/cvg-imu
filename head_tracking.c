#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <iio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>

#define GRAVITY_CHANNELS 3
#define BUF_SAMPLES 1          // Set to 1 to process every sample instantly and minimize latency
#define CONTEXT_TIMEOUT_MS 2000

static const char *GRAVITY_NAMES[GRAVITY_CHANNELS] = {"gravity_x_raw", "gravity_y_raw", "gravity_z_raw"};
static float GRAVITY_OFFSET[3] = {1.279940f, 0.227644f, -0.084857f};

static volatile sig_atomic_t g_running = 1;
static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

static void send_laser_state(int laser_on) {
    int pipe_fd = open("/tmp/hmdop_laser_pipe", O_WRONLY | O_NONBLOCK);
    if (pipe_fd < 0) return;
    if (laser_on) {
        printf("Within range LASER ON\r\n");
        write(pipe_fd, "LASER_ON\n", 9);
    } else {
        printf("---------- Outside range LASER OFF---------- \r\n");
        write(pipe_fd, "\nLASER_OFF\n", 11);
    }
    close(pipe_fd);
}

static void ensure_trigger(struct iio_context *ctx, struct iio_device *dev) {
    struct iio_device *trigger = NULL;
    if (iio_device_get_trigger(dev, &trigger) == 0 && trigger != NULL) {
        printf("Trigger already set: %s\n", iio_device_get_name(trigger) ?: iio_device_get_id(trigger));
        return;
    }

    unsigned int ndev = iio_context_get_devices_count(ctx);
    for (unsigned int i = 0; i < ndev; i++) {
        struct iio_device *candidate = iio_context_get_device(ctx, i);
        if (iio_device_is_trigger(candidate)) {
            if (iio_device_set_trigger(dev, candidate) == 0) {
                printf("Assigned trigger: %s\n", iio_device_get_name(candidate) ?: iio_device_get_id(candidate));
                return;
            }
        }
    }
    printf("No trigger assigned (none found or device doesn't require one).\n");
}

int main() {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    const char *path = "/sys/class/leds/led0::channel0/brightness";
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror("Failed to open brightness file");
        return 1;
    }

    if (access("/tmp/hmdop_laser_pipe", F_OK) == -1) {
        if (mkfifo("/tmp/hmdop_laser_pipe", 0666) != 0) {
            perror("Failed to create FIFO pipe");
            fclose(fp);
            return 1;
        }
        printf("Pipe exists\r\n");
    }

    // Pointers initialized to NULL so we can safely manage them dynamically
    struct iio_context *ctx = NULL;
    struct iio_device *dev = NULL;
    struct iio_buffer *buf = NULL;
    
    int was_in_range = 1;
    int consecutive_failures = 0;

    while (g_running) {
        // --- HARDWARE RECOVERY / INITIALIZATION BLOCK ---
        if (!ctx) {
            printf("Attempting to connect to IIO context...\n");
            ctx = iio_create_default_context();
            if (!ctx) {
                fprintf(stderr, "Failed to create IIO context. Power drop might be active. Retrying...\n");
                sleep(1);
                continue;
            }
            iio_context_set_timeout(ctx, CONTEXT_TIMEOUT_MS);
            printf("IIO Context created.\n");

            dev = iio_context_find_device(ctx, "gravity");
            if (!dev) {
                fprintf(stderr, "Could not find gravity device. Device may have brown-outed. Cleaning up...\n");
                iio_context_destroy(ctx);
                ctx = NULL;
                sleep(1);
                continue;
            }
            printf("Gravity device found.\n");

            ensure_trigger(ctx, dev);

            unsigned int num_channels = iio_device_get_channels_count(dev);
            for (unsigned int i = 0; i < num_channels; i++) {
                struct iio_channel *ch = iio_device_get_channel(dev, i);
                iio_channel_enable(ch);
            }
            printf("Gravity channels enabled.\n");

            buf = iio_device_create_buffer(dev, BUF_SAMPLES, false);
            if (!buf) {
                fprintf(stderr, "Could not create buffer. Cleaning up...\n");
                iio_context_destroy(ctx);
                ctx = NULL;
                dev = NULL;
                sleep(1);
                continue;
            }
            printf("\nInitialization successful. Starting stream.\n");
            consecutive_failures = 0;
        }
        // ------------------------------------------------

        // Attempt to pull data from the sensor
        ssize_t nbytes = iio_buffer_refill(buf);
        if (nbytes < 0) {
            char errbuf[256];
            iio_strerror((int)-nbytes, errbuf, sizeof(errbuf));
            consecutive_failures++;
            fprintf(stderr, "Buffer refill failed (%d in a row): %s\n", consecutive_failures, errbuf);

            // Turn off laser immediately on failure to preserve power and prevent garbage actions
            if (was_in_range) {
                send_laser_state(0);
                was_in_range = 0;
            }
            fflush(stdout);

            // Tear down the entire stack. A low-voltage event often drops the USB device entirely,
            // meaning the context and device handles are stale or invalid.
            printf("Tearing down IIO stack due to error to prepare for hardware reset...\n");
            if (buf) { iio_buffer_destroy(buf); buf = NULL; }
            if (ctx) { iio_context_destroy(ctx); ctx = NULL; }
            dev = NULL;

            // Wait 500ms before trying to reconnect, giving the Pi's USB hub time to re-enumerate the device
            usleep(500000); 
            continue;
        }
        consecutive_failures = 0;

        float gravity[3] = {0};
        unsigned int num_channels = iio_device_get_channels_count(dev);
        for (unsigned int i = 0, g = 0; i < num_channels && g < 3; i++) {
            struct iio_channel *ch = iio_device_get_channel(dev, i);
            if (!iio_channel_is_enabled(ch)) continue;
            
            void *data = iio_buffer_first(buf, ch);
            if (data) {
                int32_t value = *(int32_t *)data;
                gravity[g] = value * 0.0000001f - GRAVITY_OFFSET[g];
            }
            g++;
        }

        float gmag = sqrtf(gravity[0]*gravity[0] + gravity[1]*gravity[1] + gravity[2]*gravity[2]);
        
        // Avoid division by zero if hardware returns all zeros during a transient brownout state
        if (gmag < 0.0001f) gmag = 0.0001f; 

        float forward[3] = {0, 0, 1};
        float fmag = 1.0f;
        float dot = (gravity[0]*forward[0] + gravity[1]*forward[1] + gravity[2]*forward[2]) / (gmag * fmag);
        if (dot > 1.0f) dot = 1.0f;
        if (dot < -1.0f) dot = -1.0f;
        
        float angle_rad = acosf(dot);
        float angle_deg = angle_rad * 180.0f / (float)M_PI;
        int in_range = (angle_deg <= 50.0f);

        if (in_range != was_in_range) {
            send_laser_state(in_range);
        }
        was_in_range = in_range;

        fflush(stdout);
        usleep(10000); // 10 ms pacing (100Hz target)
    }

    printf("\nShutting down, forcing laser OFF.\n");
    send_laser_state(0);
    fclose(fp);
    
    if (buf) iio_buffer_destroy(buf);
    if (ctx) iio_context_destroy(ctx);
    
    return 0;
}
