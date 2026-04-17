// tracks the direction the glasses are facing to turn the laser off
// when the glasses turn to look away from straight down
//
// SYSFS VERSION (no IIO buffers / no libiio required)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

#define DEVICE_PATH "/sys/bus/iio/devices/iio:device1"

#define RAW_SCALE 0.0000001f

// Calibration offsets
static float GRAVITY_OFFSET[3] = {
    1.279940f,
    0.227644f,
   -0.084857f
};

static const char *RAW_FILES[3] = {
    DEVICE_PATH "/gravity_x_raw",
    DEVICE_PATH "/gravity_y_raw",
    DEVICE_PATH "/gravity_z_raw"
};

/* -------------------------------------------------- */
/* Read integer from sysfs file                       */
/* -------------------------------------------------- */
int read_int_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }

    int value = 0;
    fscanf(fp, "%d", &value);
    fclose(fp);

    return value;
}

/* -------------------------------------------------- */
/* Optional calibration                               */
/* -------------------------------------------------- */
void calibrate_gravity_offset(void)
{
    const int samples = 100;
    float sum[3] = {0};

    printf("\nPoint glasses straight down and press ENTER...");
    getchar();

    printf("Calibrating...\n");

    for (int i = 0; i < samples; i++) {
        for (int j = 0; j < 3; j++) {
            int raw = read_int_file(RAW_FILES[j]);
            float val = raw * RAW_SCALE;
            sum[j] += val;
        }

        usleep(20000);
    }

    float avg[3];
    for (int j = 0; j < 3; j++)
        avg[j] = sum[j] / samples;

    float expected[3] = {0.0f, 0.0f, 10.0f};

    printf("\nUse these offsets:\n");
    printf("static float GRAVITY_OFFSET[3] = {%.6ff, %.6ff, %.6ff};\n",
           avg[0] - expected[0],
           avg[1] - expected[1],
           avg[2] - expected[2]);

    exit(0);
}

/* -------------------------------------------------- */
/* MAIN                                               */
/* -------------------------------------------------- */
int main(void)
{
    /* Create pipe if needed */
    if (access("/tmp/hmdop_laser_pipe", F_OK) == -1) {
        if (mkfifo("/tmp/hmdop_laser_pipe", 0666) != 0) {
            perror("mkfifo");
            return 1;
        }
    }

    printf("Using SYSFS gravity input\n");

    /* Uncomment to calibrate */
    // calibrate_gravity_offset();

    int was_in_range = 1;

    while (1) {

        float gravity[3];

        for (int i = 0; i < 3; i++) {
            int raw = read_int_file(RAW_FILES[i]);

            gravity[i] =
                (raw * RAW_SCALE) - GRAVITY_OFFSET[i];
        }

        /* Magnitude */
        float gmag =
            sqrtf(gravity[0]*gravity[0] +
                  gravity[1]*gravity[1] +
                  gravity[2]*gravity[2]);

        /* Forward vector */
        float forward[3] = {0,0,1};

        float dot =
            (gravity[0]*forward[0] +
             gravity[1]*forward[1] +
             gravity[2]*forward[2]) / gmag;

        if (dot > 1.0f)  dot = 1.0f;
        if (dot < -1.0f) dot = -1.0f;

        float angle_deg =
            acosf(dot) * 180.0f / M_PI;

        printf("\rGravity: [%.4f %.4f %.4f] "
               "| Mag: %.4f "
               "| Angle: %.2f deg   ",
               gravity[0],
               gravity[1],
               gravity[2],
               gmag,
               angle_deg);

        int in_range = (angle_deg <= 60.0f);

        if (in_range) {
            printf("Within range   ");
        } else {
            printf("Outside range  ");

            if (was_in_range) {

                int pipe_fd =
                    open("/tmp/hmdop_laser_pipe",
                         O_WRONLY | O_NONBLOCK);

                if (pipe_fd >= 0) {
                    const char *msg = "LASER_OFF\n";
                    write(pipe_fd, msg, strlen(msg));
                    close(pipe_fd);
                }
            }
        }

        was_in_range = in_range;

        fflush(stdout);

        usleep(10000);   // 100Hz
    }

    return 0;
}
