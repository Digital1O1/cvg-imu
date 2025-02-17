from mpu9250_jmdev.registers import *
from mpu9250_jmdev.mpu_9250 import MPU9250
import time
import smbus2
import numpy as np
from math import atan2, sqrt, pi

class SensorFusion:
    def __init__(self, alpha=0.96):
        self.alpha = alpha
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0
        self.dt = 1.0  # 1 second sample rate to match your current timing
        
    def update(self, accel, gyro, mag):
        ax, ay, az = accel
        gx, gy, gz = gyro
        mx, my, mz = mag
        
        # Calculate acceleration-based angles
        accel_roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / pi
        accel_pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / pi
        
        # Integrate gyroscope data
        gyro_roll = self.roll + gx * self.dt
        gyro_pitch = self.pitch + gy * self.dt
        
        # Complementary filter
        self.roll = self.alpha * gyro_roll + (1 - self.alpha) * accel_roll
        self.pitch = self.alpha * gyro_pitch + (1 - self.alpha) * accel_pitch
        
        # Calculate yaw using magnetometer
        cos_roll = np.cos(self.roll * pi / 180.0)
        sin_roll = np.sin(self.roll * pi / 180.0)
        cos_pitch = np.cos(self.pitch * pi / 180.0)
        sin_pitch = np.sin(self.pitch * pi / 180.0)
        
        # Tilt compensated magnetic sensor measurements
        mag_x = mx * cos_pitch + my * sin_roll * sin_pitch + mz * cos_roll * sin_pitch
        mag_y = my * cos_roll - mz * sin_roll
        
        # Calculate yaw
        self.yaw = atan2(-mag_y, mag_x) * 180.0 / pi
        
        return self.roll, self.pitch, self.yaw

def initialize_mpu9250():
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

    # Open data file
    with open("data.txt", "a") as file:
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

                    # Format the orientation data
                    data_string = (
                        f"Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S')}, "
                        f"Roll: {roll:.2f}°, "
                        f"Pitch: {pitch:.2f}°, "
                        f"Yaw: {yaw:.2f}°\n"
                    )

                    # Write and display data
                    file.write(data_string)
                    print(data_string.strip())
                    
                    # Ensure data is written to disk
                    file.flush()
                    
                    time.sleep(1)

                except OSError as e:
                    print(f"I2C bus error: {e}. Retrying in 5 seconds...")
                    time.sleep(5)

        except KeyboardInterrupt:
            print("Script stopped by user.")