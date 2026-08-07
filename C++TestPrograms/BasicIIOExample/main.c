#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <iio.h>
// Compile : gcc main-c -o BasicIIOExample -liio
// Note : must use sudo to read data : Example sudo ./BasicIIOExample

int main()
{
    struct iio_context *ctx =
        iio_create_default_context();

    if (!ctx) return 1;

    /* use iio:deviceX directly */
    int iioDevice = 3;
    struct iio_device *dev = iio_context_get_device(ctx, iioDevice );

    if (!dev) {
        printf("No device1\n");
        return 1;
        }

    printf("Using device1\n");

    unsigned int count = iio_device_get_channels_count(dev);

    for (unsigned int i=0;i<count;i++) {
        struct iio_channel *ch =
        iio_device_get_channel(dev,i);
        iio_channel_enable(ch);
            }

    struct iio_buffer *buf = iio_device_create_buffer(dev,1,false);

    if (!buf) {
        printf("Buffer failed\n");
        return 1;
    }

    while(1)
    {
        iio_buffer_refill(buf);
        for (unsigned int i=0;i<count;i++) {

        struct iio_channel *ch =
        iio_device_get_channel(dev,i);

        void *p = iio_buffer_first(buf,ch);

        int32_t raw =*(int32_t*)p;

        printf("%d ", raw);
        }

        printf("\n");

        usleep(100000);
  }
}
