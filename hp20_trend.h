#pragma once

#include <cstdint>

// =============================================================================
// COMPONENT - DOMAIN / TREND
// =============================================================================
// Responsibility:
//   Keep a compact rolling FEEL history and describe direction over ~10 min.
//
// This module never changes sensor data, FEEL, cloud telemetry or alarms.
// It is an analytical/read-only layer for the OLED UX.
// =============================================================================
namespace hp20 { namespace trend {

// Baseline behavior: 20 points x 30 seconds ~= 10 minutes.
constexpr uint8_t POINTS = 20;
constexpr uint32_t SAMPLE_MS = 30000UL;

// Trend decision threshold preserved from v0.8.x.
// >= +0.6 C => rising; <= -0.6 C => falling; otherwise stable.
constexpr float DELTA_THRESHOLD_C = 0.6f;

void reset();
void push(uint32_t now, float displayFeelC);
uint8_t count();
float value(uint8_t chronologicalIndex);
float delta();
const char* label();

} }  // namespace hp20::trend
