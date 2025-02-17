from mpu9250_jmdev.registers import *
from mpu9250_jmdev.mpu_9250 import MPU9250
import time
import smbus2
import numpy as np
from math import atan2, sqrt, pi
import posix_ipc
import struct

class SensorFusion:
    # [Previous SensorFusion class implementation remains the same]
    def __init__(self, alpha=0.96):
        self.alpha = alpha
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0
        self.dt = 1.0
        
    def update(self, accel, gyro, mag):
        # [Previous update method implementation remains the same]
        ax, ay, az = accel
        gx, gy, gz = gyro
        mx, my, mz = mag
        
        accel_roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / pi
        accel_pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / pi
        
        gyro_roll = self.roll + gx * self.dt
        gyro_pitch = self.pitch + gy * self.dt
        
        self.roll = self.alpha * gyro_roll + (1 - self.alpha) * accel_roll
        self.pitch = self.alpha * gyro_pitch + (1 - self.alpha) * accel_pitch
        
        cos_roll = np.cos(self.roll * pi / 180.0)
        sin_roll = np.sin(self.roll * pi / 180.0)
        cos_pitch = np.cos(self.pitch * pi / 180.0)
        sin_pitch = np.sin(self.pitch * pi / 180.0)
        
        mag_x = mx * cos_pitch + my * sin_roll * sin_pitch + mz * cos_roll * sin_pitch
        mag_y = my * cos_roll - mz * sin_roll
        
        self.yaw = atan2(-mag_y, mag_x) * 180.0 / pi
        
        return self.roll, self.pitch, self.yaw

def initialize_mpu9250():
    # [Previous initialize_mpu9250 implementation remains the same]
    try:
        mpu = MPU9250(
            address_ak=AK8963_ADDRESS,
            address_mpu_master=MPU9050_ADDRESS_68,
            address_mpu_slave=None,
            bus=1,
            gfs=GFS_1000,
            afs=AFS_8G,
            mfs=AK8963_BIT_16,
            mode=AK8963_MODE_C100HZ
        )
        mpu.configure()
        return mpu
    except OSError as e:
        print(f"Error initializing MPU-9250: {e}")
        return None

if __name__ == "__main__":
    # Initialize sensor and fusion
    mpu = None
    while mpu is None:
        mpu = initialize_mpu9250()
        if mpu is None:
            print("Retrying initialization in 5 seconds...")
            time.sleep(5)
    
    # Initialize sensor fusion
    fusion = SensorFusion()

    # Create or open message queue
    try:
        mq = posix_ipc.MessageQueue("/sensor_data", posix_ipc.O_CREAT, mode=0o644, max_message_size=64)
    except posix_ipc.ExistentialError:
        mq = posix_ipc.MessageQueue("/sensor_data")

    try:
        while True:
            try:
                # Read sensor data
                accelerometer = mpu.readAccelerometerMaster()
                gyroscope = mpu.readGyroscopeMaster()
                magnetometer = mpu.readMagnetometerMaster()
                
                # Calculate orientation using sensor fusion
                roll, pitch, yaw = fusion.update(
                    accelerometer,
                    gyroscope,
                    magnetometer
                )

                # Pack data into bytes
                message = struct.pack('fff', roll, pitch, yaw)
                
                # Send data through message queue
                mq.send(message)
                
                # Print for debugging
                print(f"Sent - Roll: {roll:.2f}°, Pitch: {pitch:.2f}°, Yaw: {yaw:.2f}°")
                
                time.sleep(0.1)  # 10Hz update rate

            except OSError as e:
                print(f"I2C bus error: {e}. Retrying in 5 seconds...")
                time.sleep(5)

    except KeyboardInterrupt:
        print("\nCleaning up...")
        mq.close()
        try:
            mq.unlink()
        except posix_ipc.ExistentialError:
            pass
        print("Script stopped by user.")