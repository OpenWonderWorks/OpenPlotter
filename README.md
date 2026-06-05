# OpenPlotter — Open-Source Cricut-Style Cutting Plotter System

OpenPlotter is a production-grade, open-source cutting plotter platform. It provides high-performance C++ firmware that runs across multiple microcontroller architectures (including Arduino Mega 2560, Arduino Nano, ESP32, and Mega/Nano+ESP32 hybrid boards) and a sleek, premium web companion application for design, manual jogging, calibration, and real-time cutting control.

---

## Repository Structure

```text
OpenPlotter/
├── firmware/                       # PlatformIO Embedded project
│   ├── include/
│   │   └── config.h                # Master configuration file
│   ├── src/
│   │   ├── main.cpp                # Core event loop & initialization
│   │   ├── hal/                    # Hardware Abstraction Layer & Pin maps
│   │   ├── gcode/                  # G-code command lexer & parser
│   │   ├── motion/                 # Look-ahead planner & stepper interrupts
│   │   ├── homing/                 # Homing (switch-based & StallGuard)
│   │   ├── tools/                  # Tool drivers (Pen servo, Solenoid blade)
│   │   └── comms/                  # Serial, WebSocket, & Bluetooth comms
│   └── docs/                       # Complete guide & documentation
│       ├── WIRING_GUIDE.md         # Full schematics (Mega+RAMPS+ESP32)
│       ├── SETUP_GUIDE.md          # Step-by-step assembly & calibration
│       ├── GCODE_REFERENCE.md      # Command reference index
│       └── TROUBLESHOOTING.md      # Common issues & fixes
└── app/                            # Web Companion App (Vite + Vanilla JS)
    ├── index.html                  # HTML5 Canvas design layout
    ├── src/
    │   ├── main.js                 # UI binds & coordinator
    │   ├── style.css               # Design system & styles
    │   └── lib/                    # Web Serial, WebSocket, & SVG parsers
```

---

## Features

### 1. Robust Multi-Platform Firmware
*   **Hardware Abstraction Layer (HAL)**: Uniform codebase compiling natively for AVR ATmega (Mega/Nano) and ESP32.
*   **Trapezoidal Look-Ahead Planner**: Look-ahead junction speed deviation calculations to allow smooth, high-speed cornering without full stops.
*   **Timer-Interrupt Stepper Engine**: High-frequency, jitter-free stepper pulse timing utilizing hardware timers.
*   **Dual Homing Modes**:
    *   **Sensored Homing**: Mechanical endstops (Normally Closed / Normally Open).
    *   **Sensorless Homing**: Integrated TMC2209 StallGuard torque sensor monitoring for crash-less, switch-free homing.
*   **Modular Tools**: Built-in support for servo-controlled pens (pen plotters), solenoid-actuated drag knives, and tangential rotating knives.

### 2. High-Performance Web Companion App
*   **Clean Glassmorphism Theme**: Premium neon styling featuring dynamic visual state feedback.
*   **SVG Path Importer & Optimizer**: Automatic curve linearization (Bézier curves to polylines), scaling, centering, and canvas previews.
*   **Unified Interface**: Same controls work transparently over USB Web Serial (Chrome/Edge/Opera) or WebSocket (WiFi-connected ESP32 setups).
*   **Interactive Design Bed Canvas**: Interactive pan, zoom, scale, and offset controls showing real-time machine crosshair tracking.
*   **Live Console & Settings Inspector**: Terminal for direct G-code entry and visual form editor to modify machine settings (`$$`) directly inside the UI.

---

## Quick Start

### 1. Compile and Flash the Firmware
1. Open the `/firmware` directory in **VS Code** with the **PlatformIO IDE** extension installed.
2. Open `/firmware/include/config.h` and edit settings to match your board.
3. Select your build profile in PlatformIO (e.g., `mega_ramps14`) and click **Build**, then **Upload**.
4. Refer to the [SETUP_GUIDE.md](file:///i:/OpenPlotter/firmware/docs/SETUP_GUIDE.md) and [WIRING_GUIDE.md](file:///i:/OpenPlotter/firmware/docs/WIRING_GUIDE.md) for detailed schematics.

### 2. Run the Companion Web App
1. Make sure [Node.js](https://nodejs.org/) is installed.
2. Open a terminal in `/app` and run:
   ```bash
   npm install
   npm run dev
   ```
3. Open `http://localhost:5173` in your browser (Google Chrome or Microsoft Edge required for Web Serial).
4. Click **Connect Machine** to begin.
