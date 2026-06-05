# OpenPlotter — Setup & Calibration Guide

This guide walks you through compiling, flashing, and calibrating your OpenPlotter cutting system.

---

## Step 1: Install Build Tools
1. Download and install [VS Code](https://code.visualstudio.com/).
2. Open VS Code, go to the Extensions tab (Ctrl+Shift+X), and search for **PlatformIO IDE**. Install the extension.
3. If using the companion app locally, make sure you have [Node.js](https://nodejs.org/) installed (v18 or newer recommended).

---

## Step 2: Configure the Firmware
Open the workspace folder in VS Code. Navigate to [config.h](file:///i:/OpenPlotter/firmware/include/config.h) and verify the parameters.

### Critical Configurations:
*   **Hardware Setup**: Ensure `-DBOARD_MEGA` and `-DSHIELD_RAMPS14` (or other board flags) are set in [platformio.ini](file:///i:/OpenPlotter/firmware/platformio.ini) or configure pin definitions.
*   **Homing Method**:
    *   Set `#define HOMING_METHOD 0` for physical limit switches.
    *   Set `#define HOMING_METHOD 1` for TMC2209 sensorless homing.
*   **Tool Type**:
    *   Set `#define TOOL_TYPE 0` for servo-activated pens/blades (e.g. Cricut style/pen plotters).
    *   Set `#define TOOL_TYPE 1` for solenoid control.
*   **WiFi Settings (ESP32 only)**:
    *   Set your network SSID and password under `DEFAULT_WIFI_SSID` and `DEFAULT_WIFI_PASS` if you want it to automatically join your home network.

---

## Step 3: Build & Flash
1. Click the PlatformIO icon on the sidebar in VS Code.
2. Under "Project Tasks", expand the environment matching your hardware (e.g., `mega_ramps14` or `esp32`).
3. Click **Build** to compile the firmware.
4. Connect your board via USB.
5. Click **Upload** to compile and flash the firmware.

---

## Step 4: Calibrate the Machine

Once the firmware is flashed, open the Web Companion App or connect via a serial terminal (baud 115200) to verify and tune the settings.

### A. Verify Communication
1. Send the status command: `?`
   - You should receive a status report containing coordinates like `<Idle|MPos:0.00,0.00,0.00|B:0>`
2. Send the info command: `$I`
   - You should receive version info: `[OpenPlotter Version:1.0.0]`

### B. Calibrate Steps/mm
To ensure that a requested movement of 10mm actually moves the pen exactly 10mm:
1. Mark the current position of the pen.
2. Send the command to jog 100mm: `G91 G0 X100 F1000 G90`
3. Measure the physical distance moved.
4. Calculate the new steps/mm value:
   $$\text{New steps/mm} = \frac{\text{Requested Distance (100)}}{\text{Measured Distance}} \times \text{Current steps/mm}$$
5. Send the new value to the machine:
   - For X: `$100=calculated_value`
   - For Y: `$101=calculated_value`
6. Save the settings: `M500`

### C. Calibrate Servo Pen Height
1. Adjust the Pen Up and Pen Down angles so that:
   - Up: The pen is completely lifted off the material (default: `$170=30`).
   - Down: The pen is pressed firmly enough against the material to draw/cut without flexing the mount (default: `$171=70`).
2. Test the tool states:
   - Drop pen: `M3 S120` (pressure value 120)
   - Lift pen: `M5`

### D. Tune StallGuard (Sensorless Homing)
If using TMC2209 sensorless homing, you must adjust the sensitivity threshold (`$161` StallGuard threshold):
*   **Value too low (e.g. < 20)**: The motor will stall immediately upon starting homing.
*   **Value too high (e.g. > 150)**: The motor won't detect the physical end stops, and will crash into the wall, grinding the belts.
1. Start with the default value of 50: `$161=50`
2. Send `$H` to home. Keep your hand near the power switch in case it fails to stop.
3. If it stalls before reaching the end, increase the threshold value (e.g., to 60).
4. If it crashes and grinds without stopping, decrease the threshold value (e.g., to 40).
5. Once tuned, save settings: `M500`
