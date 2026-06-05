/**
 * ============================================================================
 * OpenPlotter — Math Utilities
 * ============================================================================
 *
 * Fast math helpers for motion planning. Avoids floating-point where
 * possible on AVR (no FPU), uses fixed-point or integer approximations.
 *
 * ============================================================================
 */

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>
#include <math.h>

namespace MathUtils {

    // ── Constants ───────────────────────────────────────────────────────────
    constexpr float PI_F = 3.14159265358979323846f;
    constexpr float TWO_PI_F = 6.28318530717958647692f;
    constexpr float HALF_PI_F = 1.57079632679489661923f;
    constexpr float DEG_TO_RAD_F = 0.01745329251994329576f;
    constexpr float RAD_TO_DEG_F = 57.29577951308232087679f;
    constexpr float MM_PER_INCH = 25.4f;

    // ── Fast Square Root ────────────────────────────────────────────────────
    // Quake III inverse square root approximation, adapted for direct sqrt.
    // ~2% error, but 3-4x faster than sqrtf() on AVR.
    inline float fastSqrt(float x) {
        if (x <= 0.0f) return 0.0f;
        float xhalf = 0.5f * x;
        int32_t i;
        memcpy(&i, &x, sizeof(i));
        i = 0x5f375a86 - (i >> 1);
        float y;
        memcpy(&y, &i, sizeof(y));
        y = y * (1.5f - xhalf * y * y);  // Newton iteration 1
        y = y * (1.5f - xhalf * y * y);  // Newton iteration 2 (better accuracy)
        return x * y;
    }

    // ── Hypotenuse (2D distance) ────────────────────────────────────────────
    inline float hypot2(float x, float y) {
        return fastSqrt(x * x + y * y);
    }

    // ── Hypotenuse (3D distance) ────────────────────────────────────────────
    inline float hypot3(float x, float y, float z) {
        return fastSqrt(x * x + y * y + z * z);
    }

    // ── Clamp ───────────────────────────────────────────────────────────────
    template <typename T>
    inline T clamp(T value, T minVal, T maxVal) {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }

    // ── Min / Max ───────────────────────────────────────────────────────────
    template <typename T>
    inline T min2(T a, T b) { return (a < b) ? a : b; }

    template <typename T>
    inline T max2(T a, T b) { return (a > b) ? a : b; }

    // ── Map (float version) ─────────────────────────────────────────────────
    inline float mapFloat(float x, float inMin, float inMax,
                           float outMin, float outMax) {
        return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
    }

    // ── Degrees ↔ Radians ──────────────────────────────────────────────────
    inline float degToRad(float deg) { return deg * DEG_TO_RAD_F; }
    inline float radToDeg(float rad) { return rad * RAD_TO_DEG_F; }

    // ── Angle between two vectors (for junction velocity calculation) ──────
    inline float angleBetween(float ax, float ay, float bx, float by) {
        float dot = ax * bx + ay * by;
        float magA = hypot2(ax, ay);
        float magB = hypot2(bx, by);
        if (magA < 1e-6f || magB < 1e-6f) return 0.0f;
        float cosAngle = clamp(dot / (magA * magB), -1.0f, 1.0f);
        return acosf(cosAngle);
    }

    // ── Integer absolute value ──────────────────────────────────────────────
    inline int32_t iabs(int32_t x) { return (x < 0) ? -x : x; }

    // ── Sign ────────────────────────────────────────────────────────────────
    inline int8_t sign(float x) {
        if (x > 0.0f) return 1;
        if (x < 0.0f) return -1;
        return 0;
    }

    // ── Float comparison with epsilon ───────────────────────────────────────
    inline bool floatEqual(float a, float b, float epsilon = 1e-6f) {
        return fabsf(a - b) < epsilon;
    }

    // ── Truncate to N decimal places ────────────────────────────────────────
    inline float truncate(float value, uint8_t decimals) {
        float mult = 1.0f;
        for (uint8_t i = 0; i < decimals; i++) mult *= 10.0f;
        return floorf(value * mult) / mult;
    }

} // namespace MathUtils

#endif // MATH_UTILS_H
