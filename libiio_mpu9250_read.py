from datetime import datetime
import struct
import time

import iio

if __name__ == "__main__":
 
    ctx = iio.Context()
    imu = ctx.find_device("mpu9250")
#    ctx.set_timeout(50) # milliseconds

    buf_size = 1 # number of samples

#    for attr in imu.attrs:
#        print(f"{attr}: {imu.attrs[attr]}")
#    for attr in imu.buffer_attrs:
#        print(f"{attr}: {imu.buffer_attrs[attr]}")

    accel_scale = 0
    gyro_scale = 0
    magn_scale = 0
    # temp_scale_offset = [0, 0] # these values do not make sense. Is this a genuine part?

    for ch in imu.channels:
        ch.enabled = True
        print(ch.id)
        for attr in ch.attrs:
            print(f"\t{attr}: {ch.attrs[attr].value}")
        print()

        if ch.id == "accel_x":
            accel_scale = ch.attrs['scale']
        elif ch.id == "gyro_x":
            gyro_scale = ch.attrs['scale']
        elif ch.id  == "magn_x":
            magn_scale = ch.attrs['scale']


        # iio_info should have a function to decode this?
        # print(ch.data_format)
    input("Press enter to continue")

    buffer = iio.Buffer(imu, buf_size, cyclic=False)

 #   for attr in imu.buffer_attrs:
 #       print(f"{attr}: {imu.buffer_attrs[attr]}")
    
    # what do these mean?
    # print(buffer.poll_fd)
    # print(buffer.step)
    
    accel = [0,0,0] # short signed ints
    gyro = [0,0,0]
    magn = [0,0,0]
    temp = 0
    timestamp = 0 # longlong signed int

    while True:
        try:
            buffer.refill()
            fullbuf = buffer.read()
 
            print(fullbuf.hex(), len(fullbuf))
            (
                accel[0], accel[1], accel[2], 
                temp,
                gyro[0], gyro[1], gyro[2], 
                magn[0], magn[1], magn[2], 
            ) = struct.unpack('>10h', fullbuf[:20])
            # skip 4 bytes from unknown gyro channel
            timestamp = datetime.fromtimestamp(struct.unpack('<q', fullbuf[24:32])[0]/1e9)
            
            # TODO: read this scaling from /sys/bus/iio/devices/iio:deviceX/
            accel = [a * 5.98e-4 for a in accel]
            gyro = [g * 1.064724e-3 for g in gyro]
            magn = [m * 1.769e-3 for m in magn]

            # values copied from an arduino lib for mpu9250 (iio values do not make sense).
            # Is this IC genuine?
            temp = temp / 333.87 + 21 

            print(f"{imu.name}\t{temp:>.3f}C")
            print(f"{timestamp}")
            print(f"accel:\t{accel[0]: .3f}\t{accel[1]: .3f}\t{accel[2]: .3f}")
            print(f"gyro:\t{gyro[0]: .3f}\t{gyro[1]: .3f}\t{gyro[2]: .3f}")
            print(f"magn:\t{magn[0]: .3f}\t{magn[1]: .3f}\t{magn[2]: .3f}")

        except KeyboardInterrupt:
            print("exiting")
            break;
        except TimeoutError:
            print("timeour err: pass")
        finally:
            time.sleep(0.02)

    buffer.cancel()

            
