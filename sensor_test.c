#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define QUEUE_NAME "/sensor_data"
#define PI 3.14159265358979323846

typedef struct {
    float roll;
    float pitch;
    float yaw;
} SensorData;

int main() {
    mqd_t mq;
    struct mq_attr attr;
    SensorData data;
    float t = 0.0;

    // Initialize message queue
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(SensorData);
    attr.mq_curmsgs = 0;

    mq = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        return -1;
    }

    printf("Starting test data generation...\n");

    while (1) {
        // Generate test data (simulated motion)
        data.roll = 45.0 * sin(t);
        data.pitch = 30.0 * cos(t);
        data.yaw = 180.0 * sin(t/2);

        // Print data
        printf("Roll: %.2f°, Pitch: %.2f°, Yaw: %.2f°\n",
               data.roll, data.pitch, data.yaw);
        
        // Send data through message queue
        if (mq_send(mq, (char *)&data, sizeof(SensorData), 0) == -1) {
            perror("mq_send");
        }
        
        t += 0.1;  // Increment time
        usleep(100000);  // 10Hz update rate
    }

    // Cleanup
    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    return 0;
}