# OpenPlotter Setup Guide

This guide walks you through setting up the software side of OpenPlotter, including the Companion Web App and flashing the firmware to your microcontroller.

## 1. Prerequisites

Before starting, ensure you have the following installed on your computer:
- **Node.js**: [Download here](https://nodejs.org/) (LTS version recommended).
- **Git**: [Download here](https://git-scm.com/).
- A Chromium-based browser (Chrome, Edge, Opera) for **Web Serial API** support.

## 2. Installing the Companion App

OpenPlotter comes with a premium desktop/web companion application.

1. Clone the repository:
   ```bash
   git clone https://github.com/OpenWonderWorks/OpenPlotter.git
   ```
2. Navigate to the app directory:
   ```bash
   cd OpenPlotter/app
   ```
3. Install dependencies:
   ```bash
   npm install
   ```

## 3. Running the App

You can run the app in two modes:

### Web Mode (Vite)
Runs the application as a standard website in your browser.
```bash
npm run dev
```
Open `http://localhost:5173` in your browser.

### Desktop Mode (Electron)
Runs the application as a native desktop program with deeper system integrations (like the built-in Flash Orchestrator).
```bash
npm run electron:start
```

## 4. Flashing the Firmware

The companion app comes with a **Flash Orchestrator** which can compile and upload the C++ firmware directly to your board without needing the Arduino IDE or PlatformIO.

1. Open the **OpenPlotter Companion App** (preferably in Desktop Mode).
2. Connect your board (Arduino Mega, Nano, or ESP32) via USB.
3. Open the **Plotter Config** tab.
4. Set your machine settings (driver type, kinematics, limits, etc.).
5. Scroll down to the **Flash Firmware** panel.
6. Select your COM port and board type.
7. Click **Compile & Flash**. 

> [!NOTE]
> The Flash Orchestrator automatically downloads required libraries, generates a customized `config.h` based on your UI selections, and flashes the board seamlessly.
