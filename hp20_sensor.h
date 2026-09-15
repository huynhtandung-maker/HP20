#pragma once

#include <cmath>
#include <cstdint>

// =============================================================================
// COMPONENT - DEVICE / SENSOR
// =============================================================================
// Responsibility:
//   DHT22 RAW -> validate -> user calibration -> calibrated T/RH.
//
// IMPORTANT MAINTENANCE RULE:
//   No Heat Index, thermal band, OLED, Wi-Fi or ThingsBoard logic belongs here.
//   Other modules should normally consume the calibrated values (tempC/rh),
//   while rawTempC/rawRh exist only for diagnostics and calibration checks.
// =============================================================================
namespace hp20 { namespace sensor {

struct Reading {
  float rawTempC = NAN;  // Direct DHT22 temperature, BEFORE user correction.
  float rawRh    = NAN;  // Direct DHT22 RH, BEFORE user correction.
  float tempC    = NAN;  // Calibrated temperature used by the rest of HP20.
  float rh       = NAN;  // Calibrated RH used by the rest of HP20.
  bool valid     = false;
  uint32_t sampledAt = 0;
};

// -----------------------------------------------------------------------------
// USER DHT22 CALIBRATION - THIS IS THE MAIN USER-ADJUSTABLE SENSOR AREA
// -----------------------------------------------------------------------------
// CURRENT BASELINE values below reproduce the latest working setup shown by the
// user. Change them only after comparing DHT22 with a trusted reference sensor
// placed at the SAME position for enough time to stabilize.
//
// RULE TO REMEMBER:
//   DHT22 reads HIGHER than reference -> use NEGATIVE percent.
//   DHT22 reads LOWER  than reference -> use POSITIVE percent.
//
// Example - temperature:
//   DHT22 = 30.0 C, reference = 29.4 C
//   29.4 / 30.0 = 0.98 -> about -2%
//   Set TEMP_CORRECTION_PERCENT = -2.0f;
//
// Example - humidity:
//   DHT22 = 70% RH, reference = 73.5% RH
//   73.5 / 70 = 1.05 -> about +5%
//   Set HUMIDITY_CORRECTION_PERCENT = +5.0f;
//
// If no correction is wanted, set BOTH values to 0.0f.
//
// CAUTION:
//   This is PERCENT correction, NOT direct +/- degrees C or RH points.
//   Begin with small adjustments (+/-1%, 2%, 3%...) and re-check.
//   The software clamps accidental entries to +/-20%.
// -----------------------------------------------------------------------------
constexpr float TEMP_CORRECTION_PERCENT = -7.0f;
constexpr float HUMIDITY_CORRECTION_PERCENT = +6.8f;
constexpr float CORRECTION_LIMIT_PERCENT = 20.0f;

void begin();

// Returns true only when a new DHT22 sampling attempt has occurred.
// Inspect state().valid to know whether that new sample was accepted.
bool tick(uint32_t now);

const Reading& state();
bool fresh(uint32_t now);

// Exposed for Serial diagnostics so the user can see the effective safe value.
float safeCorrectionPercent(float requestedPercent);

} }  // namespace hp20::sensor
