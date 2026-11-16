#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../../src/gcode/parser.h"
#include "../../src/system/safety_checks.h"
#include "../../src/system/settings.h"

static void test_extended_m_codes_keep_their_full_value() {
    GCodeParser parser;
    const uint16_t codes[] = {500, 501, 502};

    for (uint8_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
        char line[8];
        snprintf(line, sizeof(line), "M%u", (unsigned)codes[i]);

        GCodeBlock block;
        assert(parser.parseLine(line, block) == GCodeError::OK);
        assert((uint16_t)block.mcode == codes[i]);
    }

    GCodeBlock invalidBlock;
    assert(parser.parseLine("G256", invalidBlock) == GCodeError::INVALID_GCODE);
    assert(parser.parseLine("M65536", invalidBlock) == GCodeError::INVALID_MCODE);
    assert(parser.parseLine("M999", invalidBlock) == GCodeError::INVALID_MCODE);
}

static void test_nc_estop_and_limit_switch_polarity() {
    // The documented NC-to-GND wiring reads LOW while healthy and HIGH when
    // the circuit opens. The E-stop is therefore active HIGH.
    assert(!isActiveLevel(false, ESTOP_PIN_ACTIVE_LOW));
    assert(isActiveLevel(true, ESTOP_PIN_ACTIVE_LOW));

    assert(!isLimitTriggered(false, LIMIT_SWITCH_NC));
    assert(isLimitTriggered(true, LIMIT_SWITCH_NC));
}

static void test_homing_switch_mapping_and_safe_stop_commands() {
    assert(homeSwitchMaskForAxis(0) == 0x01);
    assert(homeSwitchMaskForAxis(1) == 0x04);
    assert(homeSwitchMaskForAxis(2) == 0x10);
    assert(homeSwitchMaskForAxis(3) == 0);
    assert(isHomeSwitchActive(0x03, 0));
    assert(!isHomeSwitchActive(0x03, 1));
    assert(isHomeSwitchActive(0x04, 1));

    assert(isSafeMCodeWhenStopped(5));
    assert(isSafeMCodeWhenStopped(18));
    assert(isSafeMCodeWhenStopped(107));
    assert(!isSafeMCodeWhenStopped(3));
}

static void test_soft_limits_use_configured_travel_and_reject_nonfinite_values() {
    const float travel[4] = {100.0f, 80.0f, 10.0f, 360.0f};
    float target[4] = {99.0f, 79.0f, 9.0f, 359.0f};
    assert(withinTravelLimits(target, travel, 4));

    target[0] = 101.0f;
    assert(!withinTravelLimits(target, travel, 4));

    target[0] = NAN;
    assert(!withinTravelLimits(target, travel, 4));
}

static void test_unknown_position_requires_homing_but_allows_home_and_queries() {
    assert(blocksCommandUntilHomed(true, true, false, false));
    assert(!blocksCommandUntilHomed(true, true, true, false));
    assert(!blocksCommandUntilHomed(true, true, false, true));
    assert(!blocksCommandUntilHomed(false, true, false, false));
    assert(canStartHoming(false, false));
    assert(!canStartHoming(true, false));
    assert(!canStartHoming(false, true));
}

static void test_unsafe_settings_values_are_rejected() {
    assert(Settings::isValidSettingValue(100, 80.0f));
    assert(!Settings::isValidSettingValue(100, 0.0f));
    assert(!Settings::isValidSettingValue(100, -1.0f));
    assert(!Settings::isValidSettingValue(100, NAN));
    assert(!Settings::isValidSettingValue(140, 0.0f));
    assert(!Settings::isValidSettingValue(160, 2.0f));
}

int main() {
    test_extended_m_codes_keep_their_full_value();
    test_nc_estop_and_limit_switch_polarity();
    test_homing_switch_mapping_and_safe_stop_commands();
    test_soft_limits_use_configured_travel_and_reject_nonfinite_values();
    test_unknown_position_requires_homing_but_allows_home_and_queries();
    test_unsafe_settings_values_are_rejected();
    puts("Firmware unit tests passed");
    return 0;
}
