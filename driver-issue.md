

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

## Bullseye

```sh
$ sudo dmesg --follow
...
[   44.897479] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[   80.705330] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[   87.226775] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[  102.310126] inv-mpu6050-i2c 1-0068: failed to ack interrupt
[  102.450140] inv-mpu6050-i2c 1-0068: failed to ack interrupt
^C
pi@rpi:~ $ uname -a
Linux rpi 6.1.21-v8+ #1642 SMP PREEMPT Mon Apr  3 17:24:16 BST 2023 aarch64 GNU/Linux
```

## Buster
I didn't see any kernel messages recorded.
