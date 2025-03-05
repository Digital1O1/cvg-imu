#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHM_NAME "/sensor_orientation_data"
#define FIFO_PATH "/tmp/sensor_calibration_fifo"
#define CALIBRATE_CMD "CALIBRATE"

// Shared memory structure
typedef struct
{
    float roll;
    float pitch;
    float yaw;
    int updated; // Flag to indicate new data is available
} SharedData;

#endif // SENSOR_COMMON_H