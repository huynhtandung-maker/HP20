#pragma once

#include <cmath>
#include <cstdint>
#include "hp20_sensor.h"

// =============================================================================
// COMPONENT - DOMAIN / THERMAL
// =============================================================================
// Responsibility:
//   Calibrated T/RH -> Heat Index/FEEL -> local HP20 band -> meaning/action.
//
// SINGLE SOURCE OF TRUTH:
//   All current FEEL thresholds and user guidance live here. UI must DISPLAY
//   these results, not recreate or reinterpret thresholds by itself.
//
// IMPORTANT:
//   The numeric Heat Index formula itself is intentionally delegated to the
//   existing model::heatIndex() so firmware preserves the established
//   numerical behavior. Thermal bands are defined only in this module.
// =============================================================================
namespace hp20 { namespace thermal {

constexpr const char* MODEL_VERSION = "HP20-SG-HI-v0.9.0";

// These names are UX/comfort interpretation labels, NOT clinical diagnoses.
enum class Band : uint8_t {
  NoData = 0,
  Cool,
  MildCool,
  Comfort,
  WarmComfort,
  SlightStuffy,
  MildStuffy,
  Stuffy,
  StuffyHot,
  Hot,
  HighHot,
  VeryHot,
  HeatLoad,
  HighLoad
};

struct State {
  double feelRaw = NAN;  // Unsmoothened HI/FEEL: used by cloud/reminder.
  float feelUi = NAN;    // Smoothed presentation value: used by OLED/trend.
  Band band = Band::NoData;
};

void reset();
void update(const sensor::Reading& env);
const State& state();

Band bandForFeel(float feelC);
const char* label(Band band);
const char* meaning(Band band);
const char* driverText(float tempC, float rh);
const char* actionPrimary(Band band);
const char* actionDetail(Band band, float tempC, float rh);
const char* secondTip(Band band, float tempC, float rh);
const char* chartRelation(Band band);

// Preferred HP20 work/study target used by the scale UI.
// It is an operational UX target, not a legal/medical standard.
float optimalLowFeel();
float optimalHighFeel();

} }  // namespace hp20::thermal
