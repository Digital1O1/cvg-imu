# OVERVIEW : Cancer Vision Goggles Head Tracking Safety System

This program uses libiio to access various sensors within the Epson Moverio BT-40 HMD over a USB connection and implements a safety feature for the Cancer Vision Goggles (CVG). The feature turns the lasers off when the user looks away from a defined field of view. The `head_tracking` program reads the 'gravity' device provided by the glasses to define two vectors: the direction the user is facing and the direction of the earth relative to the glasses (straight down). When the angle between these vectors is greater than 50° (i.e., the user looks too far from the ground), a command to turn the lasers off is written to a named pipe, which is read by the main CVG software.

This program can be adjusted to work with other HMDs/IMUs that support the IIO interface by modifying the libiio implementation. If no gravity vector is provided by the glasses, one can be calculated using the accelerometer, magnetometer, and gyroscope sensors. The program can also support any field of vision that can be defined mathematically using those two vectors; the 50-degree cone was chosen for simplicity.

# THE EXECUITABLE MUST BE RAN WITH SUDO
## Example
```bash
cd ~
sudo ./cvg-imu/build/head_tracking
```

## Configuration

1. **Kernel Configuration:**
   - The kernel must be configured to include the HID drivers. Recompile the kernel and, in the `.config` file, enable or modularize any component that starts with `HID_SENSOR_`.
   - The most important for this application are:
     - `HID_SENSOR_HUB`
     - `HID_SENSOR_ACCEL`
     - `HID_SENSOR_GYRO`
     - `HID_SENSOR_MAGN`
     - ...and their dependencies. Enabling the rest is also recommended.

1.1 **Updating `source.list` due to Buster OS** 
    - Edit `sources.list` 
        - Command : `sudo nano /etc/apt/sources.list`

1.2 **Rebuilding the kernel for Industiral IIO**
    - Steps taken so far 
        - Download source onto machine that's going to do the cross compiling : `git clone https://github.com/raspberrypi/linux`
            - Testing different branches : `git  branch -a`
                - rpi-5.4.y 
                    - Apparently closest to 'true buster-era' kernel
                - rpi-5.10.y(LTS)  
                    - Best balance?
                    - Going to test this out first 
                        - : Command : `git checkout rpi-5.10.y`
                - rpi6.1.y 
                    - If newer IIO drivers are needed
        
    - Info about busterOS
        - Kernel version : 5.10.103-v7l 
        - System architecture -m : arm7l

1.3 **Commands used so far**
- ~~On compiling machine~~ 
- This was done natively since I'm still figuring out how to cross compile

```bash

# Dependencies
sudo apt install bc bison flex libssl-dev make libc6-dev libncurses5-dev
sudo apt install crossbuild-essential-arm64   # 64-bit


git clone https://github.com/raspberrypi/linux
git checkout rpi-5.10.y


cd linux
KERNEL=kernel7l
make bcm2711_defconfig

# Install graphical config GUI and add industrial IIO support
make menuconfig

# While in menuconfig, press '/' and search for the following items and enable them 
# Then press number associated with item to enable it
# HID_SENSOR_HUB
# HID_SENSOR_ACCEL
# HID_SENSOR_GYRO
# HID_SENSOR_MAGN

# Build 32bit kernel 
make -j6 zImage modules dtbs

# Install the kernel 
sudo make -j6 modules_install

# Create backup of current kernel 
sudo cp /boot/kernel7l.img /boot/kernel7l-backup.img

# Install new kernel image
sudo cp arch/arm/boot/zImage /boot/kernel7l.img
