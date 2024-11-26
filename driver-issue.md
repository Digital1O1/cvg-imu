

# inv-mpu6050-i2c driver issue
Iterrupt acknowledge issue with driver in bookworm. 

TODO: message mailing list <linux-iio@vger.kernel.org> with a patch set / question.

## Bookworm

```sh
$ sudo dmesg --follow
...
[  208.315049] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[  216.156336] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[  219.496830] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[  229.078519] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[  230.618801] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[  245.441444] inv-mpu6050-i2c 1-0068: failed to ack interrupt
^C
$ pi@rpi:~ $ uname -a
Linux rpi 6.6.51+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.6.51-1+rpt3 (2024-10-08) aarch64 GNU/Linux
```
