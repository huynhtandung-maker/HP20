#pragma once

#include <cstdint>
#include "hp20_thermal.h"

// =============================================================================
// HP20 - HUMAN VISIBLE THERMAL INDICATOR
// =============================================================================
// External GREEN LED semantics:
//   More green presence = more thermal comfort / better condition to maintain.
//   Less green presence = comfort is falling / intervention is more desirable.
//
// IMPORTANT:
//   This module controls ONLY the external user-facing green LED (settings::LED_PIN).
//   The onboard BLUE LED remains a technical/system-status indicator in HP20.ino.
//
// Design rule:
//   Comfort        -> green ON continuously.
//   Less comfort   -> progressively shorter ON time and longer cycle.
//   Worst heat     -> very short green pulse with a long OFF interval.
//
// This makes the signal intuitive without adding a second color:
//   GREEN PRESENCE ~= COMFORT QUALITY.
// =============================================================================
namespace hp20 { namespace indicator {

enum class Mode : uint8_t {
  Off = 0,
  SteadyOn,
  Blink
};

struct GreenPattern {
  Mode mode;
  uint32_t periodMs;   // Full ON+OFF cycle. Used only in Blink mode.
  uint32_t onMs;       // Green ON time inside each cycle.
  uint8_t presencePct; // Human-readable approximate green presence (0..100%).

  constexpr GreenPattern(Mode m = Mode::Off,
                         uint32_t period = 0,
                         uint32_t on = 0,
                         uint8_t presence = 0)
      : mode(m), periodMs(period), onMs(on), presencePct(presence) {}
};

// Single source of truth for green LED behavior per thermal band.
GreenPattern greenPatternForBand(thermal::Band band);

// Returns true when the green LED should be ON at this moment.
bool greenLedOn(thermal::Band band, uint32_t nowMs);

// Diagnostic helper for Serial/testing.
uint8_t greenPresencePercent(thermal::Band band);

} } // namespace hp20::indicator
