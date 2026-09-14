#pragma once
#include <cmath>
#include <stdint.h>

namespace model {
inline bool elapsed(uint32_t now, uint32_t since, uint32_t duration) {
  return uint32_t(now - since) >= duration;
}
enum class ButtonEvent { None, Click, Hold };
class Button {
  bool raw_ = false, stable_ = false, handled_ = false;
  uint32_t changed_ = 0, pressed_ = 0;
public:
  ButtonEvent update(uint32_t now, bool down) {
    if (down != raw_) { raw_ = down; changed_ = now; }
    if (raw_ != stable_ && elapsed(now,changed_,40)) {
      stable_ = raw_;
      if (stable_) { pressed_ = now; handled_ = false; }
      else if (!handled_) return ButtonEvent::Click;
    }
    if (stable_ && raw_ && !handled_ && elapsed(now,pressed_,3000)) {
      handled_ = true; return ButtonEvent::Hold;
    }
    return ButtonEvent::None;
  }
};
// Both monotonic boot delay and persisted wall-clock reservation must expire.
inline uint32_t remainingSeconds(uint32_t now, uint32_t mark, uint32_t wait,
                                 uint64_t epoch, uint64_t deadline) {
  uint32_t used = uint32_t(now-mark)/1000;
  uint64_t left = used >= wait ? 0 : wait-used;
  if (epoch >= 1704067200 && deadline > epoch && deadline-epoch > left) left=deadline-epoch;
  return left > UINT32_MAX ? UINT32_MAX : uint32_t(left);
}
// Conservative product domain. This is an estimate, NOT a clinical measurement.
// NWS Rothfusz implementation; category thresholds are intentionally not imported.
inline double heatIndex(double c, double rh) {
  if (!std::isfinite(c) || !std::isfinite(rh) || c < 26.7 || c > 50 || rh < 40 || rh > 100)
    return NAN;
  double t = c * 1.8 + 32;
  double simple = .5 * (t + 61 + (t - 68) * 1.2 + rh * .094);
  if ((simple + t) * .5 < 80) return NAN;
  double hi = -42.379 + 2.04901523*t + 10.14333127*rh - .22475541*t*rh
      - .00683783*t*t - .05481717*rh*rh + .00122874*t*t*rh
      + .00085282*t*rh*rh - .00000199*t*t*rh*rh;
  if (rh > 85 && t >= 80 && t <= 87) hi += ((rh-85)/10)*((87-t)/5);
  return (hi-32)/1.8;
}
inline bool validSample(float t, float rh) {
  return std::isfinite(t) && std::isfinite(rh) && t >= -40 && t <= 80 && rh >= 0 && rh <= 100;
}
// HP20 working-room action bands. Operational guidance, not a medical diagnosis.
enum class RoomBand { Invalid, Cool, Comfortable, Humid, Warm, Hot, SevereHeat };
inline RoomBand roomBand(float t, float rh, double hi) {
  if (!validSample(t,rh)) return RoomBand::Invalid;
  if (std::isfinite(hi)) {
    if (hi >= 45.0) return RoomBand::SevereHeat;
    if (hi >= 39.0) return RoomBand::Hot;
    if (hi >= 32.0) return RoomBand::Warm;
  }
  if (t < 24.0) return RoomBand::Cool;
  if (rh > 75.0) return RoomBand::Humid;
  if (t > 29.0) return RoomBand::Warm;
  return RoomBand::Comfortable;
}
// User-selected reminder, not a Vietnamese medical scale. Disabled by default.
class Reminder {
  bool active_ = false, pending_ = false;
  uint32_t since_ = 0;
public:
  bool update(uint32_t now, double hi, bool enabled, double threshold) {
    if (!enabled || !std::isfinite(hi)) { active_ = pending_ = false; return false; }
    if (active_) { if (hi < threshold - 1.0) active_ = false; return active_; }
    if (hi < threshold) { pending_ = false; return false; }
    if (!pending_) { pending_ = true; since_ = now; }
    if (elapsed(now, since_, 120000)) { active_ = true; pending_ = false; }
    return active_;
  }
};
inline uint32_t retrySeconds(uint32_t base, unsigned failures) {
  uint32_t value = base;
  while (failures-- && value < 21600) value = value > 10800 ? 21600 : value * 2;
  return value > base ? value : base;
}
}
