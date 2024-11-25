
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from pprint import pprint

import struct
import sys
import time


def find_mpu9250():
    """
        Search iio for device names mpu9250

        Return: list of path to mpu9250 iio channels
    """
    
    base_path = Path("/sys/bus/iio/devices")

    mpu9250_path = None
    channels = []

    for device in base_path.glob("iio:device*/name"):
        with open(device) as fh:
            if fh.read().strip() == "mpu9250":
                mpu9250_path = device.parent
                break

    if mpu9250_path is None:
        print("MPU9250 not found")
        return channels

    channels = sorted(mpu9250_path.glob("scan_elements/*en"))
    return mpu9250_path, channels

def enable_channels(channels, disable: bool=False):
    en = '0' if disable else '1'

    for ch in channels:
        enable = ch.parent / '_'.join((ch.name.rsplit("_", 1)[0], "en"))
        with open(enable, 'w') as fh:
            print(f"enable={enable} en={en}")
            fh.write(en)

def enable_buffer(imu_path):

    with open(imu_path / "buffer" / "length", 'w') as fh:
        fh.write('1')

    with open(imu_path / "buffer" / "watermark", 'w') as fh:
        fh.write('1')

    with open(imu_path / "buffer" / "enable", 'w') as fh:
        fh.write('1')

def disable_buffer(imu_path):
    with open(imu_path / "buffer" / "enable", 'w') as fh:
        fh.write('0')

HZ = 100
CYCLE_NS = 1e9 / HZ

def main():
    imu_path, channels = find_mpu9250()
    enable_channels(channels)
    enable_buffer(imu_path)
    
    input("Press any key to continue")

    char_buff = Path("/dev") / imu_path.name
    print(f"char_buff={char_buff}")
    
    accel = [0,0,0]
    gyro = [0,0,0]
    magn = [0,0,0]
    timestamp = 0
    temp = 0

    while True:
        try:
            with open(char_buff, 'rb') as fh:
                x = fh.read(32)

            print(x.hex(), len(x))
            (
                accel[0], accel[1], accel[2],
                temp,
                gyro[0], gyro[1], gyro[2],
                magn[0], magn[1], magn[2],
            ) = struct.unpack(">10h", x[:20])
            
            timestamp = datetime.fromtimestamp(struct.unpack('<q', x[24:])[0] / 1e9)

            accel = [a * 5.98e-4 for a in accel]
            gyro = [g * 1.064724e-3 for g in gyro]
            magn = [m * 1.769e-3 for m in magn]
            temp = temp / 333.87 + 21 # from arduino lib for IMU. Raises concern on whether this IC genuine.

            print(f"temp={temp:>.3f}C")
            print(f"{timestamp}")
            print(f"accel:\t{accel[0]: .3f}\t{accel[1]: .3f}\t{accel[2]: .3f}")
            print(f"gyro:\t{gyro[0]: .3f}\t{gyro[1]: .3f}\t{gyro[2]: .3f}")
            print(f"magn:\t{magn[0]: .3f}\t{magn[1]: .3f}\t{magn[2]: .3f}")

            # FIXME: maybe use select poll instead of sleep? At least do a smarter sleep.
            time.sleep(0.02)
        except KeyboardInterrupt as e:
            print("Exiting")
            break
#            t_total = time.time_ns() - t
#            if t_total > ns:
#                print("loop too slow")
#                continue
#            time.sleep(ns - t_total)

    print("cleaning up")
    disable_buffer(imu_path)
    enable_channels(channels, disable=True)
    

if __name__ == "__main__":
    imu_path, channels = find_mpu9250()

    with open(imu_path / "buffer" / "enable", 'w') as fh:
        fh.write('0')

    enable_channels(channels, disable=True)
 
    main()
