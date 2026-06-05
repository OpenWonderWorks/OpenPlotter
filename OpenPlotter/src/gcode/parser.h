/**
 * ============================================================================
 * OpenPlotter — G-code Parser Header
 * ============================================================================
 */

#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include "commands.h"
#include "config.h"
#include <stdint.h>

class GCodeParser {
public:
    GCodeParser();

    /**
     * Parse a single line of G-code text.
     * @param line  Null-terminated string (e.g., "G1 X10 Y20 F1000")
     * @param block Output: parsed command block
     * @return GCodeError::OK on success, error code on failure
     */
    GCodeError parseLine(const char* line, GCodeBlock& block);

    /**
     * Check if a character is a real-time command.
     * Real-time commands are single characters processed immediately
     * without waiting for a newline.
     */
    static bool isRealtimeCommand(char c);

    /**
     * Get the real-time command type for a character.
     */
    static RealtimeCmd getRealtimeCommand(char c);

    /**
     * Get the current modal state.
     */
    const ModalState& getModalState() const { return _modal; }

    /**
     * Set modal state (e.g., after loading settings).
     */
    void setModalState(const ModalState& state) { _modal = state; }

    /**
     * Reset modal state to defaults.
     */
    void resetModalState();

private:
    ModalState _modal;

    // Internal parsing helpers
    GCodeError parseSystemCommand(const char* line, GCodeBlock& block);
    GCodeError parseWords(const char* line, GCodeBlock& block);
    float readFloat(const char* line, uint8_t& pos);
    int32_t readInt(const char* line, uint8_t& pos);
    void skipWhitespace(const char* line, uint8_t& pos);

    // Validation
    GCodeError validateGCode(uint8_t code);
    GCodeError validateMCode(uint8_t code);
    GCodeError validateBlock(const GCodeBlock& block);

    // Apply modal updates
    void applyModalUpdates(const GCodeBlock& block);
};

#endif // GCODE_PARSER_H
