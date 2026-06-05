/**
 * ============================================================================
 *
 *   ██████╗ ██████╗ ███████╗███╗   ██╗██████╗ ██╗      ██████╗ ████████╗████████╗███████╗██████╗
 *  ██╔═══██╗██╔══██╗██╔════╝████╗  ██║██╔══██╗██║     ██╔═══██╗╚══██╔══╝╚══██╔══╝██╔════╝██╔══██╗
 *  ██║   ██║██████╔╝█████╗  ██╔██╗ ██║██████╔╝██║     ██║   ██║   ██║      ██║   █████╗  ██████╔╝
 *  ██║   ██║██╔═══╝ ██╔══╝  ██║╚██╗██║██╔═══╝ ██║     ██║   ██║   ██║      ██║   ██╔══╝  ██╔══██╗
 *  ╚██████╔╝██║     ███████╗██║ ╚████║██║     ███████╗╚██████╔╝   ██║      ██║   ███████╗██║  ██║
 *   ╚═════╝ ╚═╝     ╚══════╝╚═╝  ╚═══╝╚═╝     ╚══════╝ ╚═════╝    ╚═╝      ╚═╝   ╚══════╝╚═╝  ╚═╝
 *
 *  Open-Source Cutting Plotter Firmware
 *  https://github.com/OpenWonderWorks/OpenPlotter
 *
 *  Version: 1.0.0
 *  License: MIT
 *
 * ============================================================================
 *
 *  MAIN ENTRY POINT
 *
 *  This file:
 *    1. Creates the HAL instance for the target board
 *    2. Initializes all subsystems
 *    3. Runs the main loop:
 *       a. Read serial/WiFi/BT for G-code lines
 *       b. Parse G-code
 *       c. Execute commands (motion, tool, homing, settings)
 *       d. Handle real-time commands (status, feed hold, resume)
 *       e. Update safety checks
 *
 * ============================================================================
 */

#include "openplotter_config.h"

// ── HAL ─────────────────────────────────────────────────────────────────────
#include "src/hal/hal.h"

#if defined(BOARD_ESP32)
    #include "src/hal/hal_esp32.h"
#elif defined(BOARD_MEGA) || defined(BOARD_NANO)
    #include "src/hal/hal_avr.h"
#endif

// ── Subsystems ──────────────────────────────────────────────────────────────
#include "src/gcode/parser.h"
#include "src/motion/planner.h"
#include "src/motion/stepper.h"
#include "src/motion/kinematics.h"
#include "src/homing/homing.h"
#include "src/tools/tool_controller.h"
#include "src/tools/pen_servo.h"
#include "src/tools/blade_solenoid.h"
#include "src/tools/tangential_knife.h"
#include "src/comms/serial_comms.h"
#include "src/system/settings.h"
#include "src/system/status.h"
#include "src/system/safety.h"
#include "src/utils/logger.h"
#include "src/motion/tmc_driver.h"

#ifdef HAS_WIFI
    #include "src/comms/wifi_comms.h"
#endif
#ifdef HAS_BLUETOOTH
    #include "src/comms/bluetooth_comms.h"
#endif

// ── Global HAL Instance ─────────────────────────────────────────────────────
HAL* hal = nullptr;

#if defined(BOARD_ESP32)
    static HAL_ESP32 _hal_instance;
#elif defined(BOARD_MEGA) || defined(BOARD_NANO)
    static HAL_AVR _hal_instance;
#endif

// ── Global Subsystem Instances ──────────────────────────────────────────────
static GCodeParser     gparser;
static Planner         planner;
static Stepper         stepper;
static Kinematics      kinematics;
static HomingManager   homing;
static SerialComms     serialComms;
Settings        settings;
static Status          status;
static Safety          safety;

// Tool controller (selected based on settings)
static PenServo        penServo;
static BladeSolenoid   bladeSolenoid;
static TangentialKnife tangentialKnife;
static ToolController* activeTool = nullptr;

#ifdef HAS_WIFI
    static WifiComms   wifiComms;
#endif
#ifdef HAS_BLUETOOTH
    static BluetoothComms btComms;
#endif

// ── Status report timing ────────────────────────────────────────────────────
static uint32_t lastStatusReport = 0;

// ── Forward declarations ────────────────────────────────────────────────────
void executeGCode(const GCodeBlock& block);
void executeSystemCommand(const GCodeBlock& block);
void handleRealtimeCommand(char cmd);
void processLine(const char* line);

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // 1. Initialize HAL
    hal = &_hal_instance;
    hal->init();

    // 2. Initialize serial communication
    serialComms.init();

    // Print banner
    hal->serialPrintln("");
    hal->serialPrintln("=================================");
    hal->serialPrintln(" OpenPlotter v" OPENPLOTTER_VERSION);
    hal->serialPrintln(" Open-Source Cutting Plotter");
    hal->serialPrintln("=================================");

    #if defined(BOARD_MEGA) && defined(SHIELD_RAMPS14)
        hal->serialPrintln("[Board: Mega 2560 + RAMPS 1.4]");
    #elif defined(BOARD_MEGA)
        hal->serialPrintln("[Board: Mega 2560]");
    #elif defined(BOARD_NANO)
        hal->serialPrintln("[Board: Nano]");
    #elif defined(BOARD_ESP32)
        hal->serialPrintln("[Board: ESP32]");
    #endif

    #ifdef HYBRID_MODE
        hal->serialPrintln("[Mode: Hybrid (motion controller)]");
    #elif defined(BRIDGE_MODE)
        hal->serialPrintln("[Mode: Bridge (WiFi/BT only)]");
    #else
        hal->serialPrintln("[Mode: Standalone]");
    #endif

    // 3. Load settings from EEPROM/NVS
    settings.init();

    // Initialize TMC Stepper Drivers (UART/SPI)
    initTmcDrivers();

    // 4. Initialize motion planner
    float stepsPerMm[4] = {
        settings.stepsPerMm(0), settings.stepsPerMm(1),
        settings.stepsPerMm(2), settings.stepsPerMm(3)
    };
    float maxRate[4] = {
        settings.maxRate(0), settings.maxRate(1),
        settings.maxRate(2), settings.maxRate(3)
    };
    planner.init(stepsPerMm, maxRate, settings.acceleration(),
                 settings.junctionDeviation());

    // 5. Initialize stepper engine
    stepper.setPlanner(&planner);
    stepper.init();

    // 6. Initialize homing
    homing.init(&stepper, &planner);
    homing.setMethod((HomingMethod)settings.homingMethod());
    homing.setSgThreshold(settings.sgThreshold());

    // 7. Initialize tool controller
    switch (settings.toolType()) {
        case 0:  // Servo pen/blade
            penServo.setUpAngle(settings.servoUpAngle());
            penServo.setDownAngle(settings.servoDownAngle());
            penServo.setDelay(settings.servoDelay());
            penServo.init();
            activeTool = &penServo;
            break;
        case 1:  // Solenoid
            bladeSolenoid.init();
            activeTool = &bladeSolenoid;
            break;
        case 2:  // Tangential knife
            tangentialKnife.init();
            activeTool = &tangentialKnife;
            break;
        default:
            penServo.init();
            activeTool = &penServo;
            break;
    }

    // 8. Initialize safety system
    safety.init(&stepper, &planner, &status);

    // 9. Initialize status reporting
    status.init(&stepper, &planner, activeTool, &homing);

    // 10. Initialize WiFi (ESP32 only)
    #ifdef HAS_WIFI
    #ifndef BRIDGE_MODE
        wifiComms.setMode(settings.wifiMode());
        wifiComms.init();
    #endif
    #endif

    // 11. Initialize Bluetooth (ESP32 only)
    #ifdef HAS_BLUETOOTH
    #ifndef BRIDGE_MODE
        btComms.init();
    #endif
    #endif

    // 12. Initialize hybrid bridge serial
    #ifdef HYBRID_MODE
        hal->bridgeSerialInit(BRIDGE_BAUD);
        hal->serialPrintln("[Bridge: Serial connected]");
    #endif

    // Status LED: solid ON = ready
    hal->pinMode(STATUS_LED_PIN, PinMode::OUTPUT_MODE);
    hal->digitalWrite(STATUS_LED_PIN, true);

    hal->serialPrintln("");
    hal->serialPrintln("Ready. Type $I for info, $$ for settings, $H to home.");
    serialComms.sendOk();

    LOG_INFO("OpenPlotter initialized. Free RAM: %lu bytes", hal->getFreeMemory());
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // ── 1. Check for real-time commands ─────────────────────────────────
    char rtCmd = serialComms.checkRealtimeCommand();
    if (rtCmd) {
        handleRealtimeCommand(rtCmd);
    }

    // ── 2. Read and process G-code from Serial ─────────────────────────
    char* line = serialComms.readLine();
    if (line) {
        processLine(line);
    }

    // ── 3. Read and process from WiFi WebSocket ────────────────────────
    #ifdef HAS_WIFI
    #ifndef BRIDGE_MODE
        wifiComms.update();
        char* wsLine = wifiComms.readLine();
        if (wsLine) {
            processLine(wsLine);
            // Echo response to WebSocket clients
        }
    #endif
    #endif

    // ── 4. Read and process from Bluetooth ─────────────────────────────
    #ifdef HAS_BLUETOOTH
    #ifndef BRIDGE_MODE
        char* btLine = btComms.readLine();
        if (btLine) {
            processLine(btLine);
        }
    #endif
    #endif

    // ── 5. Read from hybrid bridge serial ──────────────────────────────
    #ifdef HYBRID_MODE
        // In hybrid mode, the ESP32 forwards commands to us via bridge serial
        if (hal->hasBridgeSerial() && hal->bridgeSerialAvailable() > 0) {
            // Read bridge serial into a buffer
            static char bridgeBuf[GCODE_LINE_MAX_LENGTH + 1];
            static uint8_t bridgeIdx = 0;

            while (hal->bridgeSerialAvailable() > 0) {
                char c = (char)hal->bridgeSerialRead();
                if (c == '\n' || c == '\r') {
                    if (bridgeIdx > 0) {
                        bridgeBuf[bridgeIdx] = '\0';
                        processLine(bridgeBuf);
                        bridgeIdx = 0;
                    }
                } else if (bridgeIdx < GCODE_LINE_MAX_LENGTH) {
                    bridgeBuf[bridgeIdx++] = c;
                }
            }
        }
    #endif

    // ── 6. Update safety checks ────────────────────────────────────────
    safety.update();

    // ── 7. Update status ───────────────────────────────────────────────
    status.update();

    // ── 8. Periodic status report (for WiFi clients) ───────────────────
    #ifdef HAS_WIFI
    #ifndef BRIDGE_MODE
    uint32_t now = hal->millis();
    if (now - lastStatusReport >= STATUS_REPORT_INTERVAL_MS) {
        lastStatusReport = now;
        float pos[4];
        planner.getPosition(pos);
        wifiComms.sendStatus(status.getStateString(),
                             pos[0], pos[1], pos[2],
                             0.0f, activeTool ? activeTool->isDown() : false);
    }
    #endif
    #endif

    // ── 9. Update stepper idle timer ───────────────────────────────────
    stepper.updateIdleTimer();
}

// ============================================================================
// Process a Complete G-code Line
// ============================================================================

void processLine(const char* line) {
    GCodeBlock block;
    GCodeError err = gparser.parseLine(line, block);

    if (err != GCodeError::OK) {
        serialComms.sendError((uint8_t)err, gcodeErrorString(err));
        return;
    }

    // Handle system commands
    if (block.systemCmd != SystemCmd::NONE) {
        executeSystemCommand(block);
        serialComms.sendOk();
        return;
    }

    // Check alarm lock
    if (status.isAlarmed() && block.gcode != GCode::NONE) {
        serialComms.sendError((uint8_t)GCodeError::ALARM_LOCK, "Alarm locked. $X to clear.");
        return;
    }

    // Execute the G-code block
    executeGCode(block);
    serialComms.sendOk();
}

// ============================================================================
// Execute G-code Block
// ============================================================================

void executeGCode(const GCodeBlock& block) {
    const ModalState& modal = gparser.getModalState();

    // ── Handle M-codes first ────────────────────────────────────────────
    if (block.mcode != MCode::NONE) {
        switch (block.mcode) {
            case MCode::M0:
            case MCode::M1:
                // Program pause — feed hold
                stepper.feedHold();
                serialComms.sendMessage("Program paused. Send ~ to resume.");
                break;

            case MCode::M2:
            case MCode::M30:
                // Program end
                if (activeTool) activeTool->toolUp();
                stepper.feedHold();
                serialComms.sendMessage("Program complete.");
                break;

            case MCode::M3:
                // Tool down (pen/blade engage)
                if (activeTool) {
                    float pressure = block.hasS ? block.s : -1.0f;
                    activeTool->toolDown(pressure);
                }
                break;

            case MCode::M5:
                // Tool up (pen/blade disengage)
                if (activeTool) activeTool->toolUp();
                break;

            case MCode::M17:
                stepper.enableMotors(true);
                break;

            case MCode::M18:
                stepper.enableMotors(false);
                break;

            case MCode::M100:
                serialComms.sendMessage("OpenPlotter G-code Reference:");
                serialComms.sendLine("G0 XY — Rapid move");
                serialComms.sendLine("G1 XY F — Linear move");
                serialComms.sendLine("G2/G3 XY IJ — Arc CW/CCW");
                serialComms.sendLine("G28 — Home axes");
                serialComms.sendLine("G90/G91 — Abs/Rel mode");
                serialComms.sendLine("M3 S — Tool down (S=pressure)");
                serialComms.sendLine("M5 — Tool up");
                serialComms.sendLine("$$ — Settings");
                serialComms.sendLine("$H — Home");
                break;

            case MCode::M106:
                // Fan/solenoid on
                #ifdef SOLENOID_PIN
                hal->analogWrite(SOLENOID_PIN, block.hasS ? (uint8_t)block.s : 255);
                #endif
                break;

            case MCode::M107:
                #ifdef SOLENOID_PIN
                hal->analogWrite(SOLENOID_PIN, 0);
                #endif
                break;

            case MCode::M114: {
                // Report position
                float pos[4];
                planner.getPosition(pos);
                serialComms.sendf("X:%.3f Y:%.3f Z:%.3f C:%.3f",
                                  pos[0], pos[1], pos[2], pos[3]);
                break;
            }

            case MCode::M119: {
                // Report endstop status
                uint8_t endstops = homing.readEndstops();
                serialComms.sendf("X_min:%s Y_min:%s Z_min:%s",
                                  (endstops & 0x01) ? "TRIGGERED" : "open",
                                  (endstops & 0x04) ? "TRIGGERED" : "open",
                                  (endstops & 0x10) ? "TRIGGERED" : "open");
                break;
            }

            case MCode::M120:
                homing.setMethod(HomingMethod::SENSORLESS);
                serialComms.sendMessage("Sensorless homing enabled");
                break;

            case MCode::M121:
                homing.setMethod(HomingMethod::SENSORED);
                serialComms.sendMessage("Sensored homing enabled");
                break;

            case MCode::M200:
                if (activeTool && block.hasP) {
                    activeTool->setPressure(block.p);
                    serialComms.sendf("Blade pressure: %.0f", block.p);
                }
                break;

            case MCode::M500:
                settings.save();
                serialComms.sendMessage("Settings saved");
                break;

            case MCode::M501:
                settings.load();
                serialComms.sendMessage("Settings loaded");
                break;

            case MCode::M502:
                settings.resetDefaults();
                settings.save();
                serialComms.sendMessage("Factory defaults restored");
                break;

            default:
                break;
        }
    }

    // ── Handle G-codes ──────────────────────────────────────────────────
    if (block.gcode != GCode::NONE) {
        switch (block.gcode) {
            case GCode::G0:
            case GCode::G1: {
                // Linear move
                float currentPos[4];
                planner.getPosition(currentPos);

                float target[4];
                kinematics.resolveTarget(block, modal, currentPos, target);

                // Check soft limits
                if (!safety.checkSoftLimits(target)) {
                    serialComms.sendError((uint8_t)GCodeError::SOFT_LIMIT_EXCEEDED,
                                          "Soft limit exceeded");
                    return;
                }

                float feedRate = modal.feedRate;
                bool isRapid = (block.gcode == GCode::G0);

                // Ensure stepper is running
                if (stepper.getState() == StepperState::IDLE) {
                    stepper.start();
                }

                // Wait for planner space (blocking)
                while (planner.isFull()) {
                    // Process real-time commands while waiting
                    char rt = serialComms.checkRealtimeCommand();
                    if (rt) handleRealtimeCommand(rt);
                }

                planner.planLinearMove(target, feedRate, isRapid,
                                       modal.toolOn, modal.toolPressure);
                break;
            }

            case GCode::G2:
            case GCode::G3: {
                // Arc move
                float currentPos[4];
                planner.getPosition(currentPos);

                float target[4];
                kinematics.resolveTarget(block, modal, currentPos, target);

                float offset[2] = {block.i, block.j};
                bool isClockwise = (block.gcode == GCode::G2);

                if (stepper.getState() == StepperState::IDLE) {
                    stepper.start();
                }

                float arcTarget[2] = {target[0], target[1]};
                planner.planArcMove(arcTarget, offset, isClockwise,
                                    modal.feedRate, modal.toolOn, modal.toolPressure);
                break;
            }

            case GCode::G4:
                // Dwell
                if (block.hasP) {
                    hal->delayMs((uint32_t)block.p);
                }
                break;

            case GCode::G10:
                // Set coordinate offset
                kinematics.setWorkOffset(block);
                break;

            case GCode::G20:
            case GCode::G21:
                // Units change handled by parser modal state
                break;

            case GCode::G28: {
                // Home
                status.setState(MachineState::HOMING);
                HomingResult result = homing.homeAll();
                if (result == HomingResult::OK) {
                    serialComms.sendMessage("Homing complete");
                } else {
                    serialComms.sendError(14, "Homing failed");
                    status.setAlarm(3);
                }
                break;
            }

            case GCode::G90:
            case GCode::G91:
                // Positioning mode change handled by parser
                break;

            case GCode::G92:
                // Set position override
                {
                    float currentPos[4];
                    planner.getPosition(currentPos);
                    kinematics.setG92Offset(block, currentPos);
                }
                break;

            default:
                break;
        }
    }
}

// ============================================================================
// Execute System Command
// ============================================================================

void executeSystemCommand(const GCodeBlock& block) {
    switch (block.systemCmd) {
        case SystemCmd::REPORT_SETTINGS:
            settings.printAll();
            break;

        case SystemCmd::REPORT_OFFSETS: {
            float offset[4];
            kinematics.getOffset(offset);
            serialComms.sendf("[G92: X%.3f Y%.3f Z%.3f C%.3f]",
                              offset[0], offset[1], offset[2], offset[3]);
            break;
        }

        case SystemCmd::REPORT_INFO:
            serialComms.sendf("[VER: OpenPlotter v%s]", OPENPLOTTER_VERSION);
            serialComms.sendf("[OPT: %s]",
                #if defined(BOARD_MEGA) && defined(SHIELD_RAMPS14)
                    "Mega+RAMPS1.4"
                #elif defined(BOARD_MEGA)
                    "Mega"
                #elif defined(BOARD_NANO)
                    "Nano"
                #elif defined(BOARD_ESP32)
                    "ESP32"
                #else
                    "Unknown"
                #endif
            );
            serialComms.sendf("[FREE: %lu bytes]", hal->getFreeMemory());
            break;

        case SystemCmd::RUN_HOMING: {
            status.setState(MachineState::HOMING);
            HomingResult result = homing.homeAll();
            if (result != HomingResult::OK) {
                status.setAlarm(3);
            }
            break;
        }

        case SystemCmd::KILL_ALARM:
            status.clearAlarm();
            serialComms.sendMessage("Alarm cleared. Caution: position may be lost.");
            break;

        case SystemCmd::FACTORY_RESET:
            settings.resetDefaults();
            settings.save();
            serialComms.sendMessage("Factory reset complete. Restarting...");
            hal->delayMs(500);
            hal->reset();
            break;

        case SystemCmd::SET_SETTING: {
            // Parse "$N=V" from block.systemValue
            char* eq = strchr(block.systemValue, '=');
            if (eq) {
                uint16_t number = atoi(block.systemValue);
                float value = atof(eq + 1);
                if (settings.setByNumber(number, value)) {
                    serialComms.sendf("$%d=%.3f", number, value);
                } else {
                    serialComms.sendError(12, "Unknown setting");
                }
            }
            break;
        }

        #ifdef HAS_WIFI
        case SystemCmd::WIFI_STATUS:
            serialComms.sendf("[WIFI: %s]", wifiComms.isConnected() ? "Connected" : "Disconnected");
            serialComms.sendf("[IP: %s]", wifiComms.getIP());
            serialComms.sendf("[SSID: %s]", wifiComms.getSSID());
            break;

        case SystemCmd::SET_WIFI_SSID:
            wifiComms.setCredentials(block.systemValue, "");
            serialComms.sendf("[SSID set to: %s]", block.systemValue);
            break;

        case SystemCmd::SET_WIFI_PASS:
            serialComms.sendMessage("WiFi password updated. Restart to apply.");
            break;
        #else
        case SystemCmd::WIFI_STATUS:
        case SystemCmd::SET_WIFI_SSID:
        case SystemCmd::SET_WIFI_PASS:
            serialComms.sendMessage("WiFi not available on this board");
            break;
        #endif

        default:
            break;
    }
}

// ============================================================================
// Handle Real-time Commands
// ============================================================================

void handleRealtimeCommand(char cmd) {
    switch (cmd) {
        case '?': {
            // Status query
            char statusBuf[128];
            status.formatStatusReport(statusBuf, sizeof(statusBuf));
            serialComms.sendStatus(statusBuf);
            break;
        }

        case '!':
            // Feed hold
            stepper.feedHold();
            break;

        case '~':
            // Cycle resume
            stepper.resume();
            break;

        case 0x18:
            // Soft reset (Ctrl+X)
            stepper.emergencyStop();
            planner.reset();
            if (activeTool) activeTool->toolUp();
            gparser.resetModalState();
            status.setState(MachineState::IDLE);
            serialComms.sendMessage("Reset");
            break;
    }
}
