# OpenPlotter — G-code & System Command Reference

This document describes all G-code and system commands supported by the OpenPlotter firmware.

---

## 1. Supported G-codes

| Code | Command | Description | Syntax |
|:---|:---|:---|:---|
| **G0** | Rapid Travel | Move the tool at maximum speed (with tool lifted) to absolute/relative target coordinates. | `G0 X[val] Y[val]` |
| **G1** | Linear Cut / Draw | Move the tool at a controlled feed rate `F` (with tool lowered) to cut or draw a line. | `G1 X[val] Y[val] F[rate]` |
| **G2** | Circular Arc CW | Moves tool clockwise in a circle from current point to (X,Y) with radius R, or offsets I,J. | `G2 X[val] Y[val] R[radius]` |
| **G3** | Circular Arc CCW | Moves tool counter-clockwise from current point to (X,Y) with radius R, or offsets I,J. | `G3 X[val] Y[val] R[radius]` |
| **G4** | Dwell / Pause | Wait for a specified number of milliseconds before executing the next G-code line. | `G4 P[ms_value]` |
| **G20** | Inches Mode | Interpret all coordinates as inches. | `G20` |
| **G21** | Millimeters Mode | Interpret all coordinates as millimeters (default). | `G21` |
| **G28** | Home Machine | Run the configured homing cycle on all axes or specified axes (e.g. `G28 X`). | `G28` |
| **G90** | Absolute Positioning | Interpret all target coordinates relative to the origin (0,0). | `G90` |
| **G91** | Relative Positioning | Interpret all target coordinates as relative offsets from the current position. | `G91` |
| **G92** | Set Position Offset | Force the machine's current coordinates to the specified values (e.g. zero current position). | `G92 X[val] Y[val]` |

---

## 2. Supported M-codes

| Code | Command | Description | Syntax |
|:---|:---|:---|:---|
| **M0** | Program Pause | Halts execution until the user issues a cycle start (resume) command. | `M0` |
| **M2** | End of Program | Terminates the active cut queue and resets defaults. | `M2` |
| **M3** | Tool Engage (Down) | Lowers the pen or blade. S specifies cutting pressure (PWM duty cycle 0-255). | `M3 S[0-255]` |
| **M5** | Tool Disengage (Up) | Lifts the pen or blade up off the material. | `M5` |
| **M17** | Enable Steppers | Powers on all stepper motor coils to hold position. | `M17` |
| **M18** / **M84** | Disable Steppers | Powers off stepper coils, allowing manual positioning. | `M18` |
| **M100** | Print Help | Prints firmware version and lists all supported commands. | `M100` |
| **M106** | Auxiliary Output On | Activates the onboard solenoid or fan port with PWM duty cycle S (0-255). | `M106 S[value]` |
| **M107** | Auxiliary Output Off | Disables the onboard solenoid or fan port. | `M107` |
| **M114** | Report Coordinates | Prints current real-time coordinates of all axes. | `M114` |
| **M119** | Limit Switch Status | Prints the triggered/open status of all limit switches. | `M119` |
| **M500** | Save Settings | Saves all current setting adjustments to EEPROM / NVS. | `M500` |
| **M501** | Load Settings | Reloads setting values from EEPROM / NVS. | `M501` |
| **M502** | Factory Reset Settings | Resets all settings to their default values (does not commit to storage until `M500`). | `M502` |

---

## 3. System Commands ($)

System commands are configuration parameters parsed directly by the serial loop.

### Actions
*   **`?`** (Real-time Status Query): Returns the machine state and current coordinates.
    - Example response: `<Idle|MPos:10.000,20.000,0.000|B:0>`
*   **`!`** (Feed Hold): Instantly pauses active movements.
*   **`~`** (Resume): Resumes movements halted by feed hold.
*   **`Ctrl+X`** (Soft Reset): Resets the board and state engine.
*   **`$$`** (Print Settings): Lists all active settings on the console.
*   **`$H`** (Run Homing): Starts the homing sequence.
*   **`$X`** (Kill Lock): Unlocks the machine from an Alarm state.
*   **`$RST=*`** (Full Wipe): Clears all EEPROM/NVS configurations and reboots.

### Configurations
Write configuration using `$number=value` (e.g. `$100=80`).

| Parameter | Unit | Description |
|:---|:---|:---|
| **$100** | steps/mm | X-axis steps per millimeter |
| **$101** | steps/mm | Y-axis steps per millimeter |
| **$102** | steps/mm | Z-axis steps per millimeter |
| **$103** | steps/deg | C-axis steps per degree (for tangential knife) |
| **$110** | mm/min | X-axis maximum feed rate |
| **$111** | mm/min | Y-axis maximum feed rate |
| **$112** | mm/min | Z-axis maximum feed rate |
| **$120** | mm/s² | Stepper travel acceleration |
| **$130** | mm | Cornering junction deviation tolerance |
| **$140** | mm | X-axis maximum travel limits (soft limit boundary) |
| **$141** | mm | Y-axis maximum travel limits (soft limit boundary) |
| **$150** | mm/min | Homing seek rate (fast search speed) |
| **$151** | mm/min | Homing feed rate (slow precision touch speed) |
| **$152** | mm | Homing pull-off (backing off distance from switches) |
| **$160** | integer | Homing method (0 = limit switches, 1 = StallGuard sensorless) |
| **$161** | 0-255 | StallGuard motor stall threshold (TMC2209/TMC2130/TMC5160) |
| **$162** | mA | TMC Driver running current (RMS) |
| **$163** | mA | TMC Driver idle holding current |
| **$164** | integer | TMC Driver microstepping factor (e.g. 16, 32, 64) |
| **$165** | boolean | TMC StealthChop active (0 = SpreadCycle, 1 = StealthChop) |
| **$170** | degrees | Servo up angle |
| **$171** | degrees | Servo down angle |
| **$172** | ms | Wait delay after activating servo pen state |
| **$173** | 0-255 | Default blade cutting pressure (PWM value) |
| **$174** | integer | Tool type (0 = servo pen, 1 = solenoid blade, 2 = tangential knife) |
| **$180** | boolean | Invert X-axis step direction |
| **$181** | boolean | Invert Y-axis step direction |
| **$190** | boolean | WiFi configuration mode (0 = Access Point, 1 = Station Client) |
