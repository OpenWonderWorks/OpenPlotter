/**
 * ============================================================================
 * OpenPlotter — G-code Parser Implementation
 * ============================================================================
 *
 * Lexer and parser for standard G-code, M-code, and $ system commands.
 * Handles:
 *   - G-code lines: "G1 X10.5 Y20.3 F1000"
 *   - M-code lines: "M3 S128"
 *   - System commands: "$$", "$H", "$100=80.0"
 *   - Comments: "(comment)" and "; comment"
 *   - Line numbers: "N10 G1 X10 Y20"
 *   - Real-time commands: '?', '!', '~', Ctrl+X
 *
 * The parser extracts all word-value pairs from a line and populates
 * a GCodeBlock struct. Modal state is maintained between lines.
 *
 * ============================================================================
 */

#include "parser.h"
#include "../utils/logger.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// ============================================================================
// Constructor
// ============================================================================

GCodeParser::GCodeParser() {
    resetModalState();
}

void GCodeParser::resetModalState() {
    _modal.motionMode = GCode::G0;
    _modal.absoluteMode = true;
    _modal.inchMode = false;
    _modal.toolOn = false;
    _modal.feedRate = 1000.0f;
    _modal.toolPressure = 128.0f;
}

// ============================================================================
// Real-time Command Detection
// ============================================================================

bool GCodeParser::isRealtimeCommand(char c) {
    return c == '?' || c == '!' || c == '~' || c == 0x18;
}

RealtimeCmd GCodeParser::getRealtimeCommand(char c) {
    switch (c) {
        case '?':  return RealtimeCmd::STATUS_QUERY;
        case '!':  return RealtimeCmd::FEED_HOLD;
        case '~':  return RealtimeCmd::CYCLE_RESUME;
        case 0x18: return RealtimeCmd::SOFT_RESET;
        default:   return RealtimeCmd::NONE;
    }
}

// ============================================================================
// Main Parse Entry Point
// ============================================================================

GCodeError GCodeParser::parseLine(const char* line, GCodeBlock& block) {
    block.reset();

    if (!line || line[0] == '\0') {
        return GCodeError::OK; // Empty line is valid (just respond "ok")
    }

    // Check line length
    if (strlen(line) > GCODE_LINE_MAX_LENGTH) {
        return GCodeError::LINE_TOO_LONG;
    }

    // System commands start with '$'
    if (line[0] == '$') {
        return parseSystemCommand(line, block);
    }

    // Parse G-code words
    GCodeError err = parseWords(line, block);
    if (err != GCodeError::OK) return err;

    // Validate the parsed block
    err = validateBlock(block);
    if (err != GCodeError::OK) return err;

    // Apply modal state updates
    applyModalUpdates(block);

    return GCodeError::OK;
}

// ============================================================================
// System Command Parser ($)
// ============================================================================

GCodeError GCodeParser::parseSystemCommand(const char* line, GCodeBlock& block) {
    if (line[0] != '$') return GCodeError::SYSTEM_CMD_ERROR;

    // $$ — Print all settings
    if (line[1] == '$' && line[2] == '\0') {
        block.systemCmd = SystemCmd::REPORT_SETTINGS;
        return GCodeError::OK;
    }

    // $# — Print offsets
    if (line[1] == '#' && line[2] == '\0') {
        block.systemCmd = SystemCmd::REPORT_OFFSETS;
        return GCodeError::OK;
    }

    // $I — Print firmware info
    if (line[1] == 'I' && line[2] == '\0') {
        block.systemCmd = SystemCmd::REPORT_INFO;
        return GCodeError::OK;
    }

    // $N — Print startup blocks
    if (line[1] == 'N' && line[2] == '\0') {
        block.systemCmd = SystemCmd::REPORT_STARTUP;
        return GCodeError::OK;
    }

    // $H — Run homing cycle
    if (line[1] == 'H' && line[2] == '\0') {
        block.systemCmd = SystemCmd::RUN_HOMING;
        return GCodeError::OK;
    }

    // $X — Kill alarm lock
    if (line[1] == 'X' && line[2] == '\0') {
        block.systemCmd = SystemCmd::KILL_ALARM;
        return GCodeError::OK;
    }

    // $RST=* — Factory reset
    if (strncmp(line + 1, "RST=*", 5) == 0) {
        block.systemCmd = SystemCmd::FACTORY_RESET;
        return GCodeError::OK;
    }

    // $WIFI — WiFi status
    if (strncmp(line + 1, "WIFI", 4) == 0 && line[5] == '\0') {
        block.systemCmd = SystemCmd::WIFI_STATUS;
        return GCodeError::OK;
    }

    // $SSID=xxx — Set WiFi SSID
    if (strncmp(line + 1, "SSID=", 5) == 0) {
        block.systemCmd = SystemCmd::SET_WIFI_SSID;
        strncpy(block.systemValue, line + 6, sizeof(block.systemValue) - 1);
        return GCodeError::OK;
    }

    // $PASS=xxx — Set WiFi password
    if (strncmp(line + 1, "PASS=", 5) == 0) {
        block.systemCmd = SystemCmd::SET_WIFI_PASS;
        strncpy(block.systemValue, line + 6, sizeof(block.systemValue) - 1);
        return GCodeError::OK;
    }

    // $N=V — Set setting N to value V (e.g., "$100=80.0")
    if (isdigit(line[1])) {
        // Find the '=' separator
        const char* eq = strchr(line + 1, '=');
        if (eq) {
            block.systemCmd = SystemCmd::SET_SETTING;
            // Store the full "N=V" part
            strncpy(block.systemValue, line + 1, sizeof(block.systemValue) - 1);
            return GCodeError::OK;
        }
    }

    return GCodeError::SYSTEM_CMD_ERROR;
}

// ============================================================================
// G-code Word Parser
// ============================================================================

GCodeError GCodeParser::parseWords(const char* line, GCodeBlock& block) {
    uint8_t pos = 0;
    uint8_t len = strlen(line);
    bool hasGCode = false;
    bool hasMCode = false;

    while (pos < len) {
        skipWhitespace(line, pos);
        if (pos >= len) break;

        char letter = toupper(line[pos]);

        // Skip comments
        if (letter == '(' || letter == ';' || letter == '%') {
            break; // Rest of line is comment
        }

        pos++; // Move past the letter

        // Read the numeric value
        float value = readFloat(line, pos);

        switch (letter) {
            case 'G': {
                uint8_t code = (uint8_t)value;
                GCodeError err = validateGCode(code);
                if (err != GCodeError::OK) return err;
                block.gcode = (GCode)code;
                hasGCode = true;
                break;
            }

            case 'M': {
                uint8_t code = (uint8_t)value;
                GCodeError err = validateMCode(code);
                if (err != GCodeError::OK) return err;
                block.mcode = (MCode)code;
                hasMCode = true;
                break;
            }

            case 'N':
                block.lineNumber = (int32_t)value;
                block.hasLineNumber = true;
                break;

            case 'X':
                block.x = value;
                block.hasX = true;
                break;

            case 'Y':
                block.y = value;
                block.hasY = true;
                break;

            case 'Z':
                block.z = value;
                block.hasZ = true;
                break;

            case 'C':
                block.c = value;
                block.hasC = true;
                break;

            case 'I':
                block.i = value;
                block.hasI = true;
                break;

            case 'J':
                block.j = value;
                block.hasJ = true;
                break;

            case 'R':
                block.r = value;
                block.hasR = true;
                break;

            case 'F':
                block.f = value;
                block.hasF = true;
                break;

            case 'S':
                block.s = value;
                block.hasS = true;
                break;

            case 'P':
                block.p = value;
                block.hasP = true;
                break;

            case 'L':
                block.l = value;
                block.hasL = true;
                break;

            default:
                // Unknown letter — skip
                LOG_DEBUG_GCODE("Unknown word: %c%.2f", letter, value);
                break;
        }
    }

    // If no explicit G-code in this line, use the current motion mode
    // (modal behavior: "X10 Y20" is the same as "G1 X10 Y20" if G1 was last used)
    if (!hasGCode && !hasMCode && (block.hasX || block.hasY || block.hasZ)) {
        block.gcode = _modal.motionMode;
    }

    return GCodeError::OK;
}

// ============================================================================
// Number Reading
// ============================================================================

float GCodeParser::readFloat(const char* line, uint8_t& pos) {
    char buf[16];
    uint8_t i = 0;
    uint8_t len = strlen(line);

    // Handle negative sign
    if (pos < len && (line[pos] == '-' || line[pos] == '+')) {
        buf[i++] = line[pos++];
    }

    // Read digits and decimal point
    while (pos < len && i < sizeof(buf) - 1) {
        char c = line[pos];
        if (isdigit(c) || c == '.') {
            buf[i++] = c;
            pos++;
        } else {
            break;
        }
    }

    buf[i] = '\0';

    if (i == 0) return 0.0f;
    return strtof(buf, nullptr);
}

int32_t GCodeParser::readInt(const char* line, uint8_t& pos) {
    return (int32_t)readFloat(line, pos);
}

void GCodeParser::skipWhitespace(const char* line, uint8_t& pos) {
    while (line[pos] == ' ' || line[pos] == '\t') {
        pos++;
    }
}

// ============================================================================
// Validation
// ============================================================================

GCodeError GCodeParser::validateGCode(uint8_t code) {
    switch (code) {
        case 0: case 1: case 2: case 3: case 4:
        case 10: case 20: case 21: case 28:
        case 90: case 91: case 92:
            return GCodeError::OK;
        default:
            return GCodeError::INVALID_GCODE;
    }
}

GCodeError GCodeParser::validateMCode(uint8_t code) {
    switch (code) {
        case 0: case 1: case 2: case 3: case 5:
        case 17: case 18: case 30:
        case 100: case 106: case 107:
        case 114: case 119: case 120: case 121:
        case 200: case 201:
        case 500: case 501: case 502:
            return GCodeError::OK;
        default:
            // Accept unknown M-codes with a warning rather than error
            LOG_WARN("Unknown M-code: M%d", code);
            return GCodeError::OK;  // Be lenient — some senders use custom M-codes
    }
}

GCodeError GCodeParser::validateBlock(const GCodeBlock& block) {
    // Check that G1 (linear move) has a feed rate set (either in this line or previously)
    if (block.gcode == GCode::G1 || block.gcode == GCode::G2 || block.gcode == GCode::G3) {
        float effectiveFeedRate = block.hasF ? block.f : _modal.feedRate;
        if (effectiveFeedRate <= 0.0f) {
            return GCodeError::FEED_RATE_NOT_SET;
        }
    }

    // Arc commands need I/J or R
    if (block.gcode == GCode::G2 || block.gcode == GCode::G3) {
        if (!block.hasI && !block.hasJ && !block.hasR) {
            return GCodeError::ARC_RADIUS_ERROR;
        }
    }

    return GCodeError::OK;
}

// ============================================================================
// Modal State Updates
// ============================================================================

void GCodeParser::applyModalUpdates(const GCodeBlock& block) {
    // Update motion mode
    if (block.gcode == GCode::G0 || block.gcode == GCode::G1 ||
        block.gcode == GCode::G2 || block.gcode == GCode::G3) {
        _modal.motionMode = block.gcode;
    }

    // Update positioning mode
    if (block.gcode == GCode::G90) _modal.absoluteMode = true;
    if (block.gcode == GCode::G91) _modal.absoluteMode = false;

    // Update units
    if (block.gcode == GCode::G20) _modal.inchMode = true;
    if (block.gcode == GCode::G21) _modal.inchMode = false;

    // Update feed rate
    if (block.hasF && block.f > 0.0f) {
        _modal.feedRate = block.f;
    }

    // Update tool state
    if (block.mcode == MCode::M3) {
        _modal.toolOn = true;
        if (block.hasS) {
            _modal.toolPressure = block.s;
        }
    }
    if (block.mcode == MCode::M5) {
        _modal.toolOn = false;
    }
}
