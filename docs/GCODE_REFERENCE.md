# G-Code & M-Code Reference

OpenPlotter implements a standard CNC G-code parser designed specifically for 2D plotters, drag knives, and cutters. 

## Supported G-Codes (Motion & State)

| Command | Description | Parameters |
| :--- | :--- | :--- |
| **G0** | Rapid Move (Pen Up Travel) | `X`, `Y` |
| **G1** | Linear Move (Cutting/Drawing) | `X`, `Y`, `F` (Feedrate) |
| **G2** | Clockwise Arc | `X`, `Y`, `I`, `J` (Center offset) or `R` (Radius) |
| **G3** | Counter-Clockwise Arc | `X`, `Y`, `I`, `J` (Center offset) or `R` (Radius) |
| **G4** | Dwell (Pause) | `P` (milliseconds) |
| **G10** | Set Coordinate Offset | `L2`, `P1`, `X`, `Y` |
| **G20** | Set Units to Inches | - |
| **G21** | Set Units to Millimeters | - |
| **G28** | Home Axes | - |
| **G90** | Absolute Positioning | - |
| **G91** | Relative Positioning | - |
| **G92** | Set Position (Override Coordinates) | `X`, `Y` |

## Supported M-Codes (Machine & Tool Control)

| Command | Description | Parameters |
| :--- | :--- | :--- |
| **M0** | Program Pause | - |
| **M1** | Optional Pause | - |
| **M2** / **M30** | Program End / Reset | - |
| **M3** | Tool ON / Pen DOWN | `S` (Pressure 0-255) |
| **M5** | Tool OFF / Pen UP | - |
| **M17** | Enable Stepper Motors | - |
| **M18** | Disable Stepper Motors | - |
| **M106** | Fan / Solenoid ON | `S` (PWM 0-255) |
| **M107** | Fan / Solenoid OFF | - |
| **M114** | Report Current Position | - |
| **M119** | Report Endstop Status | - |
| **M120** | Enable Sensorless Homing | - |
| **M121** | Disable Sensorless Homing | - |
| **M200** | Set Blade Pressure | `P` (0-255) |
| **M201** | Set Tangential Blade Offset | `P` (degrees) |
| **M500** | Save Settings to EEPROM | - |
| **M501** | Load Settings from EEPROM | - |
| **M502** | Reset Settings to Factory Defaults | - |

## System Commands ($)

OpenPlotter borrows the Grbl-style `$` commands for real-time querying and configuration.

| Command | Description |
| :--- | :--- |
| `$$` | Print all saved settings |
| `$#` | Print coordinate offsets |
| `$I` | Print firmware info |
| `$N` | Print startup blocks |
| `$H` | Run homing cycle |
| `$X` | Kill alarm lock |
| `$RST=*` | Factory reset |
| `$N=V` | Set setting N to value V |
| `$WIFI` | Print WiFi connection status |

## Real-Time Commands
These single characters can be sent at any time to interrupt or query the machine.

- `?` : Status Query
- `!` : Feed Hold (Pause motion immediately)
- `~` : Cycle Resume
- `Ctrl+X` (0x18) : Soft Reset
