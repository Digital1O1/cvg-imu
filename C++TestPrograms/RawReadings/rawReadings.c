#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Use to compile : gcc rawReadings.c -o rawReadings
// Also make sure to check file path to ensure you're reading from the right files 
int read_val(char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) 
        {
            printf("Can't read path \r\n");
            return 0;
            exit(1);
        }
        
       //return 0;

    int val = 0;
    fscanf(fp, "%d", &val);
    fclose(fp);
    return val;
}

int main()
{
    while(1)
    {
            int x = read_val("/sys/bus/iio/devices/iio:device1/in_anglvel_x_raw");
            int y = read_val("/sys/bus/iio/devices/iio:device1/in_anglvel_y_raw");
            int z = read_val("/sys/bus/iio/devices/iio:device1/in_anglvel_z_raw");

            printf("\rX:%d  Y:%d  Z:%d", x,y,z);
            fflush(stdout);

            usleep(10000);
        }

        return 0;
}
