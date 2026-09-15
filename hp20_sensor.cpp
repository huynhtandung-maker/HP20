#include "hp20_sensor.h"

#include <DHT.h>
#include <cmath>

#include "model.h"
#include "settings.h"

namespace hp20 { namespace sensor {
namespace {
DHT dht(settings::DHT_PIN, DHT22);
Reading reading;

float applyPercentCorrection(float rawValue, float requestedPercent) {
  if (!std::isfinite(rawValue)) return NAN;
  const float p = safeCorrectionPercent(requestedPercent);
  return rawValue * (1.0f + p / 100.0f);
}

float clampHumidityPercent(float rh) {
  if (!std::isfinite(rh)) return NAN;
  if (rh < 0.0f) return 0.0f;
  if (rh > 100.0f) return 100.0f;
  return rh;
}
}  // namespace

float safeCorrectionPercent(float requestedPercent) {
  if (requestedPercent > CORRECTION_LIMIT_PERCENT) return CORRECTION_LIMIT_PERCENT;
  if (requestedPercent < -CORRECTION_LIMIT_PERCENT) return -CORRECTION_LIMIT_PERCENT;
  return requestedPercent;
}

void begin() {
  dht.begin();
}

bool tick(uint32_t now) {
  if (!model::elapsed(now, reading.sampledAt, settings::SAMPLE_MS)) return false;

  reading.sampledAt = now;
  const float rawRh = dht.readHumidity();
  const float rawT = dht.readTemperature();

  reading.rawTempC = rawT;
  reading.rawRh = rawRh;
  reading.valid = model::validSample(rawT, rawRh);

  if (reading.valid) {
    // Keep baseline behavior exactly: validate the RAW sample first, then apply
    // user calibration. Calibrated values feed HI/FEEL, OLED and telemetry.
    reading.tempC = applyPercentCorrection(rawT, TEMP_CORRECTION_PERCENT);
    reading.rh = clampHumidityPercent(
      applyPercentCorrection(rawRh, HUMIDITY_CORRECTION_PERCENT)
    );
  } else {
    reading.tempC = NAN;
    reading.rh = NAN;
  }

  return true;
}

const Reading& state() {
  return reading;
}

bool fresh(uint32_t now) {
  return reading.valid && !model::elapsed(now, reading.sampledAt, settings::STALE_MS);
}

} }  // namespace hp20::sensor
