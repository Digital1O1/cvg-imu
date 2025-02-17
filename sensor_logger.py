from mpu9250_jmdev.registers import *
from mpu9250_jmdev.mpu_9250 import MPU9250
import time
import smbus2

# Function to initialize the MPU-9250 sensor
def initialize_mpu9250():
    try:
        # Initialize the MPU-9250 sensor
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
        mpu.configure()  # Configure the sensor
        return mpu
    except OSError as e:
        print(f"Error initializing MPU-9250: {e}")
        return None

# Main script
if __name__ == "__main__":
    # Retry initialization if the I2C bus is busy
    mpu = None
    while mpu is None:
        mpu = initialize_mpu9250()
        if mpu is None:
            print("Retrying initialization in 5 seconds...")
            time.sleep(5)

    # Open a file called data.txt in append mode
    with open("data.txt", "a") as file:
        try:
            # Continuously read and write sensor data
            while True:
                try:
                    # Read sensor data
                    accelerometer = mpu.readAccelerometerMaster()
                    gyroscope = mpu.readGyroscopeMaster()
                    magnetometer = mpu.readMagnetometerMaster()
                    temperature = mpu.readTemperatureMaster()

                    # Format the data as a string
                    data_string = (
                        f"Accelerometer: {accelerometer}, "
                        f"Gyroscope: {gyroscope}, "
                        f"Magnetometer: {magnetometer}, "
                        f"Temperature: {temperature}\n"
                    )

                    # Write the data to the file
                    file.write(data_string)

                    # Print the data to the console for debugging
                    print(data_string.strip())

                    # Wait for 1 second before the next reading
                    time.sleep(1)

                except OSError as e:
                    print(f"I2C bus error: {e}. Retrying in 5 seconds...")
                    time.sleep(5)

        except KeyboardInterrupt:
            # Handle Ctrl+C to stop the script gracefully
            print("Script stopped by user.")
