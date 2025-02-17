import smbus

# Initialize I2C bus
bus = smbus.SMBus(1)

# MPU-9250 address
address = 0x68

# Read WHO_AM_I register (0x75) to verify communication
try:
    who_am_i = bus.read_byte_data(address, 0x75)
    print(f"WHO_AM_I register value: {hex(who_am_i)}")
except Exception as e:
    print(f"Error reading from I2C device: {e}")
