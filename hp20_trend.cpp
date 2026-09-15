#include "hp20_trend.h"

#include <cmath>
#include "model.h"

namespace hp20 { namespace trend {
namespace {
float history[POINTS];
uint8_t historyCount = 0;
uint8_t head = 0;
uint32_t sampledAt = 0;
}

void reset() {
  historyCount = 0;
  head = 0;
  sampledAt = 0;
  for (uint8_t i = 0; i < POINTS; ++i) history[i] = NAN;
}

void push(uint32_t now, float valueC) {
  if (!std::isfinite(valueC)) return;
  if (historyCount && !model::elapsed(now, sampledAt, SAMPLE_MS)) return;

  sampledAt = now;
  history[head] = valueC;
  head = (head + 1) % POINTS;
  if (historyCount < POINTS) historyCount++;
}

uint8_t count() {
  return historyCount;
}

float value(uint8_t chronologicalIndex) {
  if (chronologicalIndex >= historyCount) return NAN;
  const uint8_t oldest = (head + POINTS - historyCount) % POINTS;
  return history[(oldest + chronologicalIndex) % POINTS];
}

float delta() {
  if (historyCount < 4) return 0.0f;

  const uint8_t group = historyCount >= 8 ? 3 : 2;
  float oldAvg = 0.0f;
  float newAvg = 0.0f;

  for (uint8_t i = 0; i < group; ++i) {
    oldAvg += value(i);
    newAvg += value(historyCount - group + i);
  }

  return newAvg / group - oldAvg / group;
}

const char* label() {
  if (historyCount < 4) return "DANG THU THAP";
  const float d = delta();
  if (d >= DELTA_THRESHOLD_C) return "DANG TANG";
  if (d <= -DELTA_THRESHOLD_C) return "DANG GIAM";
  return "ON DINH";
}

} }  // namespace hp20::trend
