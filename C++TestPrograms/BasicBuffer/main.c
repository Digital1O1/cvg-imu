/*
    BASIC IIO BUFFER EXAMPLE FOR BUSTER / BOOKWORM

    Purpose:
    - Show how Linux IIO buffers work
    - Read 3 gravity channels using libiio
    - Very small/simple example

    Compile:
        gcc basic_buffer.c -o basic_buffer -liio

    Run:
        ./basic_buffer
*/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <iio.h>
#include <errno.h>
int main(void)
{
    /* ------------------------------------ */
    /* Create IIO context                   */
    /* ------------------------------------ */
    struct iio_context *ctx =
        iio_create_default_context();

    if (!ctx) {
        printf("Failed to create context\n");
        return 1;
    }

    printf("Context created\n");

    /* ------------------------------------ */
    /* Find gravity device                  */
    /* ------------------------------------ */
    //struct iio_device *dev =
    //    iio_context_find_device(ctx, 1);
   
    /* use iio:device1 directly */
    struct iio_device *dev =
        iio_context_get_device(ctx,3);

    if (!dev) {
        printf("Gravity device not found\n");
        iio_context_destroy(ctx);
        return 1;
    }

    printf("Gravity device found\n");

    /* ------------------------------------ */
    /* Enable all channels                  */
    /* ------------------------------------ */
    unsigned int count =
        iio_device_get_channels_count(dev);

    int enabled_count =0;
    for (unsigned int i = 0; i < count; i++) {

        struct iio_channel *ch =
            iio_device_get_channel(dev, i);
    if (!iio_channel_is_scan_element(ch)) {
        printf("Skipping non-scan-element channel %d\n", i);
        continue;
    }
        iio_channel_enable(ch);
        enabled_count++;
        printf("Enabled channel: %s\n", iio_channel_get_id(ch));
    }

    //printf("Channels enabled\n");
    printf("%d scan-element channels enabled\n", enabled_count);

if (enabled_count == 0) {
    printf("No scan-element channels found — cannot create buffer\n");
    iio_context_destroy(ctx);
    return 1;
}

    /* ------------------------------------ */
    /* Create buffer                        */
    /* ------------------------------------ */
    struct iio_buffer *buf =
        iio_device_create_buffer(dev, 1, false);

    if (!buf) {
        //printf("Buffer creation failed\n");
        printf("Buffer creation failed: %s (errno %d)\n", strerror(errno), errno);
        iio_context_destroy(ctx);
        return 1;
    }

    printf("Buffer created\n");

    /* ------------------------------------ */
    /* Read forever                         */
    /* ------------------------------------ */
    while (1) {

        /* Ask kernel for new sample */
        ssize_t nbytes =
            iio_buffer_refill(buf);

        if (nbytes < 0) {
            //printf("Refill failed\n");
            printf("Refill failed: %s (errno %d)\n", strerror(-nbytes), (int)-nbytes);

            break;
        }

        float gravity[3] = {0};
        int g = 0;

        /* Read each enabled channel */
        for (unsigned int i = 0; i < count && g < 3; i++) {

            struct iio_channel *ch =
                iio_device_get_channel(dev, i);

            if (!iio_channel_is_enabled(ch))
                continue;

            /* Pointer to data */
            void *p =
                iio_buffer_first(buf, ch);

            /* Raw 32-bit value */
            int32_t raw =
                *(int32_t *)p;

            gravity[g] =
                raw * 0.0000001f;

            g++;
        }

        printf("\rX:%8.4f  Y:%8.4f  Z:%8.4f",
               gravity[0],
               gravity[1],
               gravity[2]);

        fflush(stdout);

        usleep(10000);
    }

    /* ------------------------------------ */
    /* Cleanup                              */
    /* ------------------------------------ */
    iio_buffer_destroy(buf);
    iio_context_destroy(ctx);

    return 0;
}
