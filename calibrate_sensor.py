# Import necessary libraries
from mpu9250_jmdev.registers import *  # Register addresses for MPU-9250
from mpu9250_jmdev.mpu_9250 import MPU9250  # MPU-9250 library
import time  # For adding delays

# Initialize the MPU-9250 sensor
mpu = MPU9250(
    address_ak=AK8963_ADDRESS,  # Magnetometer address
    address_mpu_master=MPU9050_ADDRESS_68,  # MPU-9250 address (0x68 or 0x69)
    address_mpu_slave=None,  # No secondary MPU
    bus=1,  # I2C bus (1 for Raspberry Pi)
    gfs=GFS_1000,  # Gyroscope full-scale range
    afs=AFS_8G,  # Accelerometer full-scale range
    mfs=AK8963_BIT_16,  # Magnetometer resolution
    mode=AK8963_MODE_C100HZ  # Magnetometer operation mode
)

# Configure the sensor with default settings
mpu.configure()

# Step 1: Accelerometer and Gyroscope Calibration
print("Starting accelerometer and gyroscope calibration...")
print("Place the sensor on a flat, stable surface and keep it still.")

# Collect data for 100 samples
num_samples = 100
accel_offsets = [0, 0, 0]  # Initialize accelerometer offsets
gyro_offsets = [0, 0, 0]  # Initialize gyroscope offsets

for _ in range(num_samples):
    # Read raw accelerometer and gyroscope data
    accel_data = mpu.readAccelerometerMaster()
    gyro_data = mpu.readGyroscopeMaster()

    # Accumulate offsets
    for i in range(3):
        accel_offsets[i] += accel_data[i]
        gyro_offsets[i] += gyro_data[i]

    # Wait for a short time before the next reading
    time.sleep(0.1)

# Calculate average offsets
for i in range(3):
    accel_offsets[i] /= -num_samples  # Negative sign to correct the offset
    gyro_offsets[i] /= -num_samples

print("Accelerometer offsets:", accel_offsets)
print("Gyroscope offsets:", gyro_offsets)

# Apply the calculated offsets
mpu.setAccelerometerOffsets(accel_offsets[0], accel_offsets[1], accel_offsets[2])
mpu.setGyroscopeOffsets(gyro_offsets[0], gyro_offsets[1], gyro_offsets[2])

# Step 2: Magnetometer Calibration
print("Starting magnetometer calibration...")
print("Rotate the sensor in a figure-8 pattern for 30 seconds.")

# Collect magnetometer data for 30 seconds
magnetometer_data = []
start_time = time.time()
while time.time() - start_time < 30:
    magnetometer_data.append(mpu.readMagnetometerMaster())
    time.sleep(0.1)

# Calculate hard-iron offsets (average of min and max values)
magnetometer_min = [min(axis) for axis in zip(*magnetometer_data)]
magnetometer_max = [max(axis) for axis in zip(*magnetometer_data)]
hard_iron_offsets = [(min_val + max_val) / 2 for min_val, max_val in zip(magnetometer_min, magnetometer_max)]

print("Magnetometer hard-iron offsets:", hard_iron_offsets)

# Apply the hard-iron offsets
mpu.setMagnetometerOffsets(hard_iron_offsets[0], hard_iron_offsets[1], hard_iron_offsets[2])

print("Calibration complete! Offsets have been applied.")
