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
#include "src/system/safety_checks.h"
#include "src/utils/logger.h"
#include "src/motion/tmc_driver.h"
#include <stdio.h>
#include <stdarg.h>

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

#if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
    static WifiComms   wifiComms;
#endif
#ifdef HAS_BLUETOOTH
    static BluetoothComms btComms;
#endif

// ── Status report timing ────────────────────────────────────────────────────
static uint32_t lastStatusReport = 0;
#define DEFERRED_SERIAL_LINES 1
static char deferredSerialLines[DEFERRED_SERIAL_LINES][GCODE_LINE_MAX_LENGTH + 1];
static uint8_t deferredSerialHead = 0;
static uint8_t deferredSerialCount = 0;
static bool resetAfterAcknowledgement = false;

// ── Forward declarations ────────────────────────────────────────────────────
bool executeGCode(const GCodeBlock& block);
bool executeSystemCommand(const GCodeBlock& block);
void handleRealtimeCommand(char cmd);
void processLine(const char* line);

static void sendProtocolOk() {
    serialComms.sendOk();
    #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
    wifiComms.broadcast("ok");
    #endif
}

static void sendProtocolError(uint8_t code, const char* message) {
    serialComms.sendError(code, message);
    #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
    char response[96];
    if (message) snprintf(response, sizeof(response), "error:%u (%s)", code, message);
    else snprintf(response, sizeof(response), "error:%u", code);
    wifiComms.broadcast(response);
    #endif
}

static void sendProtocolLine(const char* line) {
    serialComms.sendLine(line);
    #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
    wifiComms.broadcast(line);
    #endif
}

static void sendProtocolMessage(const char* message) {
    serialComms.sendMessage(message);
    #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
    char response[144];
    snprintf(response, sizeof(response), "[MSG: %s]", message);
    wifiComms.broadcast(response);
    #endif
}

static void sendProtocolf(const char* format, ...) {
    char response[128];
    va_list args;
    va_start(args, format);
    vsnprintf(response, sizeof(response), format, args);
    va_end(args);
    sendProtocolLine(response);
}

static void applyPlannerSettings() {
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
}

static bool pollHomingSafety() {
    safety.update();
    return !status.isAlarmed();
}

static void pollRealtimeDuringWait() {
    // readLine() recognizes real-time characters while preserving a complete
    // ordinary command for the main loop after this wait finishes.
    if (deferredSerialCount < DEFERRED_SERIAL_LINES) {
        char* pending = serialComms.readLine();
        if (pending) {
            const uint8_t tail = (uint8_t)((deferredSerialHead + deferredSerialCount) %
                                           DEFERRED_SERIAL_LINES);
            strncpy(deferredSerialLines[tail], pending,
                    sizeof(deferredSerialLines[tail]) - 1);
            deferredSerialLines[tail][sizeof(deferredSerialLines[tail]) - 1] = '\0';
            ++deferredSerialCount;
        }
    }

    char rt = serialComms.checkRealtimeCommand();
    if (rt) handleRealtimeCommand(rt);

    #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
    wifiComms.update();
    rt = wifiComms.checkRealtimeCommand();
    if (rt) handleRealtimeCommand(rt);
    #endif

    safety.update();
    status.update();
}

static bool waitForMotionComplete() {
    while (planner.hasBlocks() || stepper.getState() != StepperState::IDLE) {
        pollRealtimeDuringWait();
        if (status.isAlarmed()) {
            sendProtocolError((uint8_t)GCodeError::ALARM_LOCK,
                              "Motion stopped by safety alarm");
            return false;
        }
        hal->delayMs(1);
    }
    return true;
}

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
    applyPlannerSettings();

    // 5. Initialize stepper engine
    stepper.setPlanner(&planner);
    stepper.init();

    // 6. Initialize homing
    homing.init(&stepper, &planner, &settings);
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
    safety.init(&stepper, &planner, &status, &settings, &homing, activeTool);

    // 9. Initialize status reporting
    status.init(&stepper, &planner, activeTool, &homing);
    homing.setSafetyPollCallback(pollHomingSafety);

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

    #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
        wifiComms.update();
        rtCmd = wifiComms.checkRealtimeCommand();
        if (rtCmd) handleRealtimeCommand(rtCmd);
    #endif

    // ── 2. Read and process G-code from Serial ─────────────────────────
    char* line = nullptr;
    if (deferredSerialCount > 0) {
        line = deferredSerialLines[deferredSerialHead];
        deferredSerialHead = (uint8_t)((deferredSerialHead + 1) % DEFERRED_SERIAL_LINES);
        --deferredSerialCount;
    } else {
        line = serialComms.readLine();
    }
    if (line) {
        processLine(line);
    }

    // ── 3. Read and process from WiFi WebSocket ────────────────────────
    #ifdef HAS_WIFI
    #ifndef BRIDGE_MODE
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
        sendProtocolError((uint8_t)err, gcodeErrorString(err));
        return;
    }

    // Handle system commands
    if (block.systemCmd != SystemCmd::NONE) {
        if (executeSystemCommand(block)) {
            sendProtocolOk();
            if (resetAfterAcknowledgement) {
                resetAfterAcknowledgement = false;
                hal->delayMs(250);
                hal->reset();
            }
        }
        return;
    }

    // Keep motion and actuator-on commands locked until the alarm is cleared.
    // Read-only queries and explicit stop/off commands remain available.
    const bool readOnlyQuery = block.gcode == GCode::NONE &&
        (block.mcode == MCode::M100 || block.mcode == MCode::M114 ||
         block.mcode == MCode::M119);
    const bool safeStopCommand = block.gcode == GCode::NONE &&
        isSafeMCodeWhenStopped((uint16_t)block.mcode);
    if (status.isAlarmed() && !readOnlyQuery && !safeStopCommand) {
        sendProtocolError((uint8_t)GCodeError::ALARM_LOCK,
                          "Alarm locked. Clear with $X, then home with $H.");
        return;
    }

    const bool hasCommand = block.gcode != GCode::NONE || block.mcode != MCode::NONE;
    const bool isHomingCommand = block.gcode == GCode::G28;
    if (blocksCommandUntilHomed(safety.requiresHoming(), hasCommand,
                                isHomingCommand, readOnlyQuery || safeStopCommand)) {
        sendProtocolError((uint8_t)GCodeError::ALARM_LOCK,
                          "Position unknown after stop. Home with $H before motion or tool-on commands.");
        return;
    }

    // Execute the G-code block
    if (executeGCode(block)) sendProtocolOk();
}

// ============================================================================
// Execute G-code Block
// ============================================================================

bool executeGCode(const GCodeBlock& block) {
    const ModalState& modal = gparser.getModalState();

    const bool needsMotionBarrier =
        block.mcode == MCode::M0 || block.mcode == MCode::M1 ||
        block.mcode == MCode::M2 || block.mcode == MCode::M3 ||
        block.mcode == MCode::M5 || block.mcode == MCode::M18 ||
        block.mcode == MCode::M30 || block.mcode == MCode::M500 ||
        block.mcode == MCode::M501 || block.mcode == MCode::M502 ||
        block.gcode == GCode::G4 ||
        block.gcode == GCode::G28;
    if (needsMotionBarrier && !waitForMotionComplete()) return false;

    // ── Handle M-codes first ────────────────────────────────────────────
    if (block.mcode != MCode::NONE) {
        switch (block.mcode) {
            case MCode::M0:
            case MCode::M1:
                // Program pause — feed hold
                stepper.feedHold();
                sendProtocolMessage("Program paused. Send ~ to resume.");
                break;

            case MCode::M2:
            case MCode::M30:
                // Program end
                if (activeTool) activeTool->toolUp();
                stepper.feedHold();
                sendProtocolMessage("Program complete.");
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
                sendProtocolMessage("OpenPlotter G-code Reference:");
                sendProtocolLine("G0 XY — Rapid move");
                sendProtocolLine("G1 XY F — Linear move");
                sendProtocolLine("G2/G3 XY IJ — Arc CW/CCW");
                sendProtocolLine("G28 — Home axes");
                sendProtocolLine("G90/G91 — Abs/Rel mode");
                sendProtocolLine("M3 S — Tool down (S=pressure)");
                sendProtocolLine("M5 — Tool up");
                sendProtocolLine("$$ — Settings");
                sendProtocolLine("$H — Home");
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
                sendProtocolf("X:%.3f Y:%.3f Z:%.3f C:%.3f",
                                  pos[0], pos[1], pos[2], pos[3]);
                break;
            }

            case MCode::M119: {
                // Report endstop status
                uint8_t endstops = homing.readEndstops();
                const char* xMax = "n/a";
                const char* yMax = "n/a";
                const char* zMin = "n/a";
                const char* zMax = "n/a";
                const char* xMin = "n/a";
                const char* yMin = "n/a";
                #if defined(X_MIN_PIN) && HARD_LIMIT_X_MIN_ENABLED
                xMin = (endstops & 0x01) ? "TRIGGERED" : "open";
                #endif
                #if defined(X_MAX_PIN) && HARD_LIMIT_X_MAX_ENABLED
                xMax = (endstops & 0x02) ? "TRIGGERED" : "open";
                #endif
                #if defined(Y_MIN_PIN) && HARD_LIMIT_Y_MIN_ENABLED
                yMin = (endstops & 0x04) ? "TRIGGERED" : "open";
                #endif
                #if defined(Y_MAX_PIN) && HARD_LIMIT_Y_MAX_ENABLED
                yMax = (endstops & 0x08) ? "TRIGGERED" : "open";
                #endif
                #if defined(Z_MIN_PIN) && HARD_LIMIT_Z_MIN_ENABLED
                zMin = (endstops & 0x10) ? "TRIGGERED" : "open";
                #endif
                #if defined(Z_MAX_PIN) && HARD_LIMIT_Z_MAX_ENABLED
                zMax = (endstops & 0x20) ? "TRIGGERED" : "open";
                #endif
                sendProtocolf("X_min:%s X_max:%s Y_min:%s Y_max:%s Z_min:%s Z_max:%s",
                              xMin, xMax, yMin, yMax, zMin, zMax);
                break;
            }

            case MCode::M120:
                homing.setMethod(HomingMethod::SENSORLESS);
                sendProtocolMessage("Sensorless homing enabled");
                break;

            case MCode::M121:
                homing.setMethod(HomingMethod::SENSORED);
                sendProtocolMessage("Sensored homing enabled");
                break;

            case MCode::M200:
                if (activeTool && block.hasP) {
                    activeTool->setPressure(block.p);
                    sendProtocolf("Blade pressure: %.0f", block.p);
                }
                break;

            case MCode::M500:
                settings.save();
                sendProtocolMessage("Settings saved");
                break;

            case MCode::M501:
                settings.load();
                applyPlannerSettings();
                safety.markPositionUnknown();
                sendProtocolMessage("Settings loaded");
                break;

            case MCode::M502:
                settings.resetDefaults();
                settings.save();
                applyPlannerSettings();
                safety.markPositionUnknown();
                sendProtocolMessage("Factory defaults restored");
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
                    sendProtocolError((uint8_t)GCodeError::SOFT_LIMIT_EXCEEDED,
                                      "Soft limit exceeded or invalid target");
                    return false;
                }

                float feedRate = modal.feedRate;
                bool isRapid = (block.gcode == GCode::G0);

                // Ensure stepper is running
                if (stepper.getState() == StepperState::IDLE) {
                    stepper.start();
                }

                // Wait for planner space (blocking)
                while (planner.isFull()) {
                    pollRealtimeDuringWait();
                    if (status.isAlarmed()) {
                        sendProtocolError((uint8_t)GCodeError::ALARM_LOCK,
                                          "Motion stopped by safety alarm");
                        return false;
                    }
                    hal->delayMs(1);
                }

                if (!planner.planLinearMove(target, feedRate, isRapid,
                                            modal.toolOn, modal.toolPressure)) {
                    sendProtocolError((uint8_t)GCodeError::UNSUPPORTED_COMMAND,
                                      "Planner buffer full");
                    return false;
                }
                break;
            }

            case GCode::G2:
            case GCode::G3: {
                // Arc move
                float currentPos[4];
                planner.getPosition(currentPos);

                float target[4];
                kinematics.resolveTarget(block, modal, currentPos, target);
                if (!safety.checkSoftLimits(target)) {
                    sendProtocolError((uint8_t)GCodeError::SOFT_LIMIT_EXCEEDED,
                                      "Soft limit exceeded or invalid target");
                    return false;
                }

                float offset[2] = {block.i, block.j};
                bool isClockwise = (block.gcode == GCode::G2);

                if (stepper.getState() == StepperState::IDLE) {
                    stepper.start();
                }

                float arcTarget[2] = {target[0], target[1]};
                if (!planner.planArcMove(arcTarget, offset, isClockwise,
                                         modal.feedRate, modal.toolOn,
                                         modal.toolPressure)) {
                    sendProtocolError((uint8_t)GCodeError::UNSUPPORTED_COMMAND,
                                      "Arc exceeds current planner capacity");
                    return false;
                }
                break;
            }

            case GCode::G4:
                // Dwell
                if (block.hasP) {
                    const uint32_t dwellMs = (uint32_t)block.p;
                    const uint32_t start = hal->millis();
                    while ((uint32_t)(hal->millis() - start) < dwellMs) {
                        pollRealtimeDuringWait();
                        if (status.isAlarmed()) {
                            sendProtocolError((uint8_t)GCodeError::ALARM_LOCK,
                                              "Dwell cancelled by safety alarm");
                            return false;
                        }
                        hal->delayMs(1);
                    }
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
                    safety.markPositionKnown();
                    sendProtocolMessage("Homing complete");
                } else {
                    safety.markPositionUnknown();
                    sendProtocolError(14, "Homing failed");
                    status.setAlarm(3);
                    return false;
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
    return true;
}

// ============================================================================
// Execute System Command
// ============================================================================

bool executeSystemCommand(const GCodeBlock& block) {
    bool succeeded = true;
    if ((block.systemCmd == SystemCmd::RUN_HOMING ||
         block.systemCmd == SystemCmd::SET_SETTING ||
         block.systemCmd == SystemCmd::FACTORY_RESET) &&
        !waitForMotionComplete()) {
        return false;
    }
    switch (block.systemCmd) {
        case SystemCmd::REPORT_SETTINGS:
            settings.printAll(sendProtocolLine);
            break;

        case SystemCmd::REPORT_OFFSETS: {
            float offset[4];
            kinematics.getOffset(offset);
            sendProtocolf("[G92: X%.3f Y%.3f Z%.3f C%.3f]",
                              offset[0], offset[1], offset[2], offset[3]);
            break;
        }

        case SystemCmd::REPORT_INFO:
            sendProtocolf("[VER: OpenPlotter v%s]", OPENPLOTTER_VERSION);
            sendProtocolf("[OPT: %s]",
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
            sendProtocolf("[FREE: %lu bytes]", hal->getFreeMemory());
            break;

        case SystemCmd::RUN_HOMING: {
            safety.update();
            if (!canStartHoming(status.isAlarmed(), safety.estopActive())) {
                sendProtocolError((uint8_t)GCodeError::ALARM_LOCK,
                                  "Clear the alarm and release E-stop before homing");
                return false;
            }
            status.setState(MachineState::HOMING);
            HomingResult result = homing.homeAll();
            if (result == HomingResult::OK) {
                safety.markPositionKnown();
            } else {
                safety.markPositionUnknown();
                status.setAlarm(3);
                sendProtocolError((uint8_t)GCodeError::HOMING_REQUIRED,
                                  "Homing failed");
                succeeded = false;
            }
            break;
        }

        case SystemCmd::KILL_ALARM:
            safety.update();
            if (safety.estopActive()) {
                sendProtocolError((uint8_t)GCodeError::ALARM_LOCK,
                                  "Release E-stop before clearing alarm");
                return false;
            }
            status.clearAlarm();
            sendProtocolMessage("Alarm cleared. Caution: position may be lost.");
            break;

        case SystemCmd::FACTORY_RESET:
            settings.resetDefaults();
            settings.save();
            sendProtocolMessage("Factory reset complete. Restarting after acknowledgement.");
            resetAfterAcknowledgement = true;
            break;

        case SystemCmd::SET_SETTING: {
            // Parse "$N=V" from block.systemValue
            char* eq = strchr(block.systemValue, '=');
            if (eq) {
                uint16_t number = atoi(block.systemValue);
                float value = atof(eq + 1);
                if (settings.setByNumber(number, value)) {
                    applyPlannerSettings();
                    if (number >= 100 && number <= 103) {
                        safety.markPositionUnknown();
                    }
                    sendProtocolf("$%d=%.3f", number, value);
                } else {
                    sendProtocolError((uint8_t)GCodeError::SYSTEM_CMD_ERROR,
                                      "Unknown setting or invalid value");
                    succeeded = false;
                }
            } else {
                sendProtocolError((uint8_t)GCodeError::SYSTEM_CMD_ERROR,
                                  "Expected $number=value");
                succeeded = false;
            }
            break;
        }

        #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
        case SystemCmd::WIFI_STATUS:
            sendProtocolf("[WIFI: %s]", wifiComms.isConnected() ? "Connected" : "Disconnected");
            sendProtocolf("[IP: %s]", wifiComms.getIP());
            sendProtocolf("[SSID: %s]", wifiComms.getSSID());
            break;

        case SystemCmd::SET_WIFI_SSID:
            wifiComms.setCredentials(block.systemValue, "");
            sendProtocolf("[SSID set to: %s]", block.systemValue);
            break;

        case SystemCmd::SET_WIFI_PASS:
            sendProtocolMessage("WiFi password updated. Restart to apply.");
            break;
        #else
        case SystemCmd::WIFI_STATUS:
        case SystemCmd::SET_WIFI_SSID:
        case SystemCmd::SET_WIFI_PASS:
            sendProtocolMessage("WiFi not available on this board");
            break;
        #endif

        default:
            sendProtocolError((uint8_t)GCodeError::SYSTEM_CMD_ERROR,
                              "Unsupported system command");
            succeeded = false;
            break;
    }
    return succeeded;
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
            #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
            float pos[4];
            planner.getPosition(pos);
            wifiComms.sendStatus(status.getStateString(), pos[0], pos[1], pos[2],
                                 0.0f, activeTool ? activeTool->isDown() : false);
            #endif
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
            safety.emergencyStop();
            gparser.resetModalState();
            sendProtocolMessage("Reset. Clear alarm with $X, then home with $H.");
            #if defined(HAS_WIFI) && !defined(BRIDGE_MODE)
            wifiComms.broadcast("Reset. Clear alarm with $X, then home with $H.");
            #endif
            break;
    }
}
