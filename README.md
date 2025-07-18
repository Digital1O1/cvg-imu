# Cancer Vision Goggles Head Tracking Safety System

This program uses libiio to access various sensors within the Epson Moverio BT-40 HMD over a USB connection and implements a safety feature for the Cancer Vision Goggles (CVG). The feature turns the lasers off when the user looks away from a defined field of view. The `head_tracking` program reads the 'gravity' device provided by the glasses to define two vectors: the direction the user is facing and the direction of the earth relative to the glasses (straight down). When the angle between these vectors is greater than 50° (i.e., the user looks too far from the ground), a command to turn the lasers off is written to a named pipe, which is read by the main CVG software.

This program can be adjusted to work with other HMDs/IMUs that support the IIO interface by modifying the libiio implementation. If no gravity vector is provided by the glasses, one can be calculated using the accelerometer, magnetometer, and gyroscope sensors. The program can also support any field of vision that can be defined mathematically using those two vectors; the 50-degree cone was chosen for simplicity.

## Configuration

1. **Kernel Configuration:**
   - The kernel must be configured to include the HID drivers. Recompile the kernel and, in the `.config` file, enable or modularize any component that starts with `HID_SENSOR_`.
   - The most important for this application are:
     - `HID_SENSOR_HUB`
     - `HID_SENSOR_ACCEL`
     - `HID_SENSOR_GYRO`
     - `HID_SENSOR_MAGN`
     - ...and their dependencies. Enabling the rest is also recommended.

2. **Dependencies:**
   - This program relies on the [libiio](https://github.com/analogdevicesinc/libiio) package.
   - Install on Ubuntu/Debian:
     ```sh
     sudo apt-get install libiio-dev libiio-utils
     ```

3. **Named Pipe:**
   - The program writes commands to `/tmp/hmdop_laser_pipe`. Ensure this named pipe exists and is being read by the main CVG software. The `head_tracking` executable will attempt to create it if it does not already exist.
     ```sh
     mkfifo /tmp/hmdop_laser_pipe
     ```

## Building

To build the program, run:

```sh
make head_tracking
```

This will produce the `head_tracking` executable.

## Usage

Run the program with:

```sh
./head_tracking
```

- When using in tandem with the CVG software, run both processes independently: `head_tracking` first then `hmdopapp`
- The program will continuously monitor the head orientation and write `LASER_OFF` to the named pipe if the user looks outside the defined field of view (greater than 50° from straight down).
- To change the field of view, modify the angle threshold in the source code.

## Calibration

To calibrate the gravity offset:
1. Uncomment the `calibrate_gravity_offset(dev);` line in `head_tracking.c` (in the main function, after the device is set up).
2. Build and run the program. Follow the on-screen instructions to point the glasses straight down and press Enter.
3. The program will print a new `GRAVITY_OFFSET` array. Copy this value into the source code, re-comment the calibration line, and rebuild.

## Troubleshooting

- **Buffer creation errors:** Ensure you are not creating multiple buffers for the same device at the same time. Run calibration before entering the main loop.
- **Permission errors:** You may need to run as root or with `sudo` if you do not have access to the IIO device files.
- **No gravity device found:** Make sure the HMD is connected and the kernel drivers are loaded.
- **No output to pipe:** Ensure the named pipe exists and is being read by the main CVG software.

## Adapting to Other Devices

- If your HMD does not provide a gravity vector, you can modify the code to compute it from the accelerometer, gyroscope, and magnetometer channels.
- Adjust the field of view logic as needed for your application.

