#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define QUEUE_NAME "/sensor_data"
#define MAX_MSG_SIZE 64

typedef struct {
    float roll;
    float pitch;
    float yaw;
} SensorData;

int main() {
    mqd_t mq;
    struct mq_attr attr;
    char buffer[MAX_MSG_SIZE];
    SensorData data;

    // Set up message queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    // Open message queue
    mq = mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        exit(1);
    }

    printf("Waiting for sensor data...\n");

    while (1) {
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        
        if (bytes_read >= 0) {
            // Unpack the data
            memcpy(&data, buffer, sizeof(SensorData));
            
            // Process the data
            printf("Roll: %.2f°, Pitch: %.2f°, Yaw: %.2f°\n",
                   data.roll, data.pitch, data.yaw);
        }
    }

    // Cleanup
    mq_close(mq);
    mq_unlink(QUEUE_NAME);

    return 0;
}