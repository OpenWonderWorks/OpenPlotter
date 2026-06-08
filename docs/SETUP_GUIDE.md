# OpenPlotter Setup Guide

This guide walks you through setting up the software side of OpenPlotter, including the Companion App and flashing the firmware to your microcontroller.

## 1. Prerequisites

Before starting, ensure you have the following installed on your computer:
- **Node.js**: [Download here](https://nodejs.org/) (LTS version recommended).
- **Git**: [Download here](https://git-scm.com/).
- A Chromium-based browser (Chrome, Edge, Opera) for **Web Serial API** support (if using Web Mode).

## 2. Installing the Companion App

OpenPlotter v4.0 features the **Elite Plotter Design Studio** interface, utilizing the *Industrial Minimalism* design system for high-precision workflows.

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

### Desktop Mode (Electron) - RECOMMENDED
Runs the application as a native desktop program with deeper system integrations (like the built-in Flash Orchestrator and native filesystem access).
```bash
npm run electron:start
```
> [!NOTE]
> For development and debugging, use `npm run electron:dev`. This will build the frontend assets using Vite and then launch Electron with `NODE_ENV=development`.

### Web Mode (Vite)
Runs the application as a standard website in your browser (requires Web Serial API to connect to the plotter).
```bash
npm run dev
```
Open `http://localhost:5173` in your browser.

## 4. Flashing the Firmware

The companion app comes with a **Flash Orchestrator** which can compile and upload the C++ firmware directly to your board without needing the Arduino IDE or PlatformIO. It natively supports advanced configurations like TMC UART/SPI drivers, StealthChop, and ESP32 Hybrid streaming.

1. Open the **OpenPlotter Companion App** (preferably in Desktop Mode).
2. Connect your board (Arduino Mega, Nano, BTT SKR, or ESP32) via USB.
3. Open the **Firmware** tab in the right sidebar.
4. Set your machine settings (driver type, kinematics, limits, current ratings, etc.).
5. Scroll down to the **Flash Firmware** panel.
6. Select your COM port and board type.
7. Click **Open Flash Orchestrator** and start the sequence.

> [!TIP]
> The Flash Orchestrator automatically downloads required libraries (like `TMCStepper` and `AccelStepper`), generates a customized `config.h` based on your UI selections, and flashes the board seamlessly.
