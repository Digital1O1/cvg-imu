# Head Tracking App Testing Plan

This document outlines the testing plan for the Cancer Vision Goggles Head Tracking Safety System, as described in the main README. The goal is to ensure the system is robust, accurate, and reliable for clinical and research use.

## Test 1: Rocking Test (Drift Assessment)

**Purpose:**
- To verify that the calculated head orientation (specifically the angle between the gravity vector and the headset's forward vector) does not drift over long periods of use.

**Method:**
- The HMD is placed on a platform and rocked uniformly back and forth for an extended period (e.g., 3 hours).
- The `rocking_test` program is run to log all relevant sensor data, including the computed angle, at regular intervals (e.g., every 500 ms).
- After the test, the data is plotted (angle vs. time and other relevant plots) to visually inspect for drift or bias in the angle measurement.

**Expected Outcome:**
- The angle should remain consistent and periodic, with no significant drift or bias over the 3-hour period.
- Any drift or bias would indicate a problem with sensor calibration, integration, or filtering.

---

## Test 2: Laser-Off Latency Test

**Purpose:**
- To measure the responsiveness of the system in turning the laser off when the user looks away from the defined field of view.

**Method:**
- The HMD is repeatedly moved in and out of the "laser-off" condition (i.e., the angle threshold is crossed) over the course of several hours.
- The time at which the condition is induced is recorded, and the time at which the system writes the `LASER_OFF` command to the named pipe is logged.
- This can be done by synchronizing the movement with a known time or by using the logged data to infer the timing.

**Expected Outcome:**
- The system should consistently and promptly send the `LASER_OFF` signal each time the threshold is crossed, with minimal and consistent latency.
- No missed or delayed signals should occur, even after many hours of operation.

---

## Test 3: Angle Threshold Repeatability Test

**Purpose:**
- To ensure that the system triggers the laser-off signal at the same angle every time, confirming the reliability of the angle threshold logic.

**Method:**
- The HMD is mounted on a platform with a known, repeatable motion (e.g., a rotary stage or protractor setup).
- The platform is slowly moved through the angle threshold multiple times.
- The frequency of LASER_OFF messages should match the frequency of the platform's motion
- By correlating the time and angle, the repeatability of the threshold can be assessed.

**Expected Outcome:**
- The `LASER_OFF` signal should be sent at the same time (within a small margin of error) on every period.
- Any significant variation would indicate unreliability with the angle calculation or threshold logic.

