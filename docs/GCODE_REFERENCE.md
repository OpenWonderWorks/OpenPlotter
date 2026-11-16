# Firmware command reference

This page describes commands implemented by the checked-in firmware, not the full RS-274 or Grbl language. Use millimeters (`G21`) and absolute coordinates (`G90`) unless your job explicitly needs another mode. Test new commands with the tool raised.

## Motion and coordinate commands

| Command | Behavior | Parameters |
| --- | --- | --- |
| `G0` | Rapid linear move | `X`, `Y`, optional `Z`, `C` |
| `G1` | Feed move | `X`, `Y`, optional `Z`, `C`, `F` |
| `G2` / `G3` | Clockwise / counter-clockwise XY arc | End point `X`, `Y`; center offsets `I`, `J`; optional `F` |
| `G4` | Dwell | `P` in milliseconds |
| `G10` | Set work coordinate offset | `L`, `P`, and axis values; see current firmware implementation before use |
| `G20` / `G21` | Select inch / millimeter mode | — |
| `G28` | Home all configured axes | — |
| `G90` / `G91` | Absolute / relative positioning | — |
| `G92` | Set current work position | axis values such as `X0 Y0` |

Linear moves are checked against configured soft limits. Arc moves are currently planned in XY using I/J center offsets. Do not assume support for a complete industrial G-code dialect.

## Tool and machine commands

| Command | Behavior | Parameters |
| --- | --- | --- |
| `M0` / `M1` | Enter feed hold | — |
| `M2` / `M30` | Raise the active tool and stop the job | — |
| `M3` | Lower the active tool | Optional `S` pressure (0–255) |
| `M5` | Raise the active tool | — |
| `M17` / `M18` | Enable / disable motors | — |
| `M100` | Print firmware command help | — |
| `M106` / `M107` | Toggle optional solenoid output where configured | Optional `S` PWM (0–255) for `M106` |
| `M114` | Report position | — |
| `M119` | Report configured endstop state | — |
| `M120` / `M121` | Select sensorless / sensored homing | — |
| `M200` | Change active tool pressure | `P` pressure (0–255) |
| `M500` / `M501` / `M502` | Save / load / reset settings | — |

The firmware currently has one active tool controller. `T0` / `T1` tool changes and two-head workflows are not implemented end to end. Do not use generated dual-head jobs until firmware and app behavior explicitly support tool selection.

## System commands

| Command | Behavior |
| --- | --- |
| `$$` | Print settings |
| `$#` | Print work offsets |
| `$I` | Print firmware information |
| `$N` | Print startup blocks |
| `$H` | Run homing |
| `$X` | Clear alarm lock; after a stop, home again before moving |
| `$RST=*` | Restore and save factory settings, then reset |
| `$N=V` | Set setting number `N` to value `V` |
| `$WIFI` | Print Wi-Fi status where compiled with Wi-Fi support |

Setting numbers and defaults are defined in `OpenPlotter/src/system/settings.cpp` and `OpenPlotter/openplotter_config.h`. Changing a setting does not necessarily reconfigure every initialized subsystem until restart; consult the source and test before relying on it.

## Real-time serial characters

These characters are consumed outside the normal line parser on the serial connection:

| Character | Behavior |
| --- | --- |
| `?` | Query status |
| `!` | Feed hold |
| `~` | Resume after feed hold |
| Ctrl+X (`0x18`) | Reset motion state and raise the tool |

Real-time behavior depends on the connection transport. A parser accepting a command does not guarantee that every connection mode routes it correctly.

## Example: conservative air move

```gcode
G21
G90
M5
G0 X5 Y5 F300
G1 X10 Y5 F100
M5
```

Use a small, clear area and keep the tool disengaged while validating scale, direction, and limits.
