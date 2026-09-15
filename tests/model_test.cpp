#include "../model.h"
#include "../hp20_thermal.h"
#include "../hp20_trend.h"
#include "../hp20_indicator.h"
#include "../hp20_version.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  // Core Heat Index formula: NWS reference neighborhood.
  assert(std::abs(model::heatIndex(32.222222, 70) - 41.1) < 0.2);
  assert(std::isnan(model::heatIndex(25, 70)));
  assert(std::isnan(model::heatIndex(30, 101)));
  assert(std::isnan(model::heatIndex(NAN, 70)));
  assert(model::heatIndex(32, 80) > model::heatIndex(32, 60));

  // Raw sensor validity remains a low-level invariant.
  assert(!model::validSample(NAN, 50));
  assert(!model::validSample(25, -1));
  assert(model::validSample(32.5f, 72.0f));

  // Current HP20-SG-HI band source of truth.
  using TB = hp20::thermal::Band;
  assert(hp20::thermal::bandForFeel(NAN) == TB::NoData);
  assert(hp20::thermal::bandForFeel(24.9f) == TB::Cool);
  assert(hp20::thermal::bandForFeel(25.0f) == TB::MildCool);
  assert(hp20::thermal::bandForFeel(28.0f) == TB::Comfort);
  assert(hp20::thermal::bandForFeel(31.0f) == TB::WarmComfort);
  assert(hp20::thermal::bandForFeel(33.0f) == TB::SlightStuffy);
  assert(hp20::thermal::bandForFeel(35.0f) == TB::MildStuffy);
  assert(hp20::thermal::bandForFeel(37.0f) == TB::Stuffy);
  assert(hp20::thermal::bandForFeel(39.0f) == TB::StuffyHot);
  assert(hp20::thermal::bandForFeel(41.0f) == TB::Hot);
  assert(hp20::thermal::bandForFeel(44.0f) == TB::HighHot);
  assert(hp20::thermal::bandForFeel(47.0f) == TB::VeryHot);
  assert(hp20::thermal::bandForFeel(50.0f) == TB::HeatLoad);
  assert(hp20::thermal::bandForFeel(54.0f) == TB::HighLoad);
  assert(std::abs(hp20::thermal::optimalLowFeel() - 28.0f) < 0.001f);
  assert(std::abs(hp20::thermal::optimalHighFeel() - 33.0f) < 0.001f);


  // Firmware identity is centralized in hp20_version.h.
  static_assert(hp20::version::MAJOR == 0, "Unexpected major version");
  assert(std::string(hp20::version::STRING) == "0.9.6");

  // Green LED semantics: Comfort is solid green; hotter bands progressively
  // reduce green presence. This is a comfort signal, not an alarm strobe.
  using IB = hp20::indicator::Mode;
  assert(hp20::indicator::greenPatternForBand(TB::NoData).mode == IB::Off);
  assert(hp20::indicator::greenPatternForBand(TB::Comfort).mode == IB::SteadyOn);
  assert(hp20::indicator::greenPresencePercent(TB::Comfort) == 100);
  assert(hp20::indicator::greenPresencePercent(TB::WarmComfort) >
         hp20::indicator::greenPresencePercent(TB::SlightStuffy));
  assert(hp20::indicator::greenPresencePercent(TB::SlightStuffy) >
         hp20::indicator::greenPresencePercent(TB::MildStuffy));
  assert(hp20::indicator::greenPresencePercent(TB::MildStuffy) >
         hp20::indicator::greenPresencePercent(TB::Stuffy));
  assert(hp20::indicator::greenPresencePercent(TB::Stuffy) >
         hp20::indicator::greenPresencePercent(TB::StuffyHot));
  assert(hp20::indicator::greenPresencePercent(TB::StuffyHot) >
         hp20::indicator::greenPresencePercent(TB::Hot));
  assert(hp20::indicator::greenPresencePercent(TB::Hot) >
         hp20::indicator::greenPresencePercent(TB::HighHot));
  assert(hp20::indicator::greenPresencePercent(TB::HighHot) >
         hp20::indicator::greenPresencePercent(TB::VeryHot));
  assert(hp20::indicator::greenPresencePercent(TB::VeryHot) >
         hp20::indicator::greenPresencePercent(TB::HeatLoad));
  assert(hp20::indicator::greenPresencePercent(TB::HeatLoad) >
         hp20::indicator::greenPresencePercent(TB::HighLoad));

  // Thermal state uses calibrated T/RH input and keeps raw/UI FEEL separate.
  hp20::sensor::Reading env;
  env.valid = true;
  env.tempC = 32.5f;
  env.rh = 72.0f;
  hp20::thermal::reset();
  hp20::thermal::update(env);
  const auto& th = hp20::thermal::state();
  assert(std::isfinite(th.feelRaw));
  assert(std::isfinite(th.feelUi));
  assert(th.band == hp20::thermal::bandForFeel(th.feelUi));

  // Trend engine timing and direction.
  hp20::trend::reset();
  hp20::trend::push(0, 30.0f);
  hp20::trend::push(30000, 30.1f);
  hp20::trend::push(60000, 30.2f);
  hp20::trend::push(90000, 30.3f);
  assert(hp20::trend::count() == 4);
  assert(std::string(hp20::trend::label()) == "ON DINH");

  hp20::trend::reset();
  hp20::trend::push(0, 30.0f);
  hp20::trend::push(30000, 30.2f);
  hp20::trend::push(60000, 31.0f);
  hp20::trend::push(90000, 31.2f);
  assert(hp20::trend::delta() >= hp20::trend::DELTA_THRESHOLD_C);
  assert(std::string(hp20::trend::label()) == "DANG TANG");

  // millis() wrap-safe timing.
  assert(model::elapsed(50, UINT32_MAX - 49, 100));
  assert(!model::elapsed(49, UINT32_MAX - 49, 100));

  // Reminder behavior remains unchanged.
  model::Reminder r;
  assert(!r.update(0, 36, true, 35));
  assert(!r.update(119999, 36, true, 35));
  assert(r.update(120000, 36, true, 35));
  assert(r.update(120001, 34.5, true, 35));
  assert(!r.update(120002, 33.9, true, 35));
  assert(!r.update(130000, 36, false, 35));
  assert(!r.update(140000, 36, true, 35));
  assert(!r.update(150000, NAN, true, 35));
  assert(!r.update(270000, 36, true, 35));

  // Retry backoff.
  assert(model::retrySeconds(900, 1) == 1800);
  assert(model::retrySeconds(900, 10) == 21600);
  assert(model::retrySeconds(86400, 10) == 86400);

  // Button debounce/hold behavior.
  using E = model::ButtonEvent;
  model::Button button;
  assert(button.update(0, true) == E::None);
  assert(button.update(10, false) == E::None);
  assert(button.update(60, false) == E::None);
  assert(button.update(100, true) == E::None);
  assert(button.update(140, true) == E::None);
  assert(button.update(200, false) == E::None);
  assert(button.update(240, false) == E::Click);
  assert(button.update(280, false) == E::None);
  assert(button.update(300, true) == E::None);
  assert(button.update(340, true) == E::None);
  assert(button.update(3339, true) == E::None);
  assert(button.update(3340, true) == E::Hold);
  assert(button.update(4000, true) == E::None);
  assert(button.update(4010, false) == E::None);
  assert(button.update(4050, false) == E::None);

  model::Button wrapped;
  assert(wrapped.update(UINT32_MAX - 99, true) == E::None);
  assert(wrapped.update(UINT32_MAX - 59, true) == E::None);
  assert(wrapped.update(2940, true) == E::Hold);

  // Cloud cooldown/reboot protection.
  const uint64_t epoch = 1800000000;
  assert(model::remainingSeconds(0, 0, 900, epoch, 0) == 900);
  assert(model::remainingSeconds(899999, 0, 900, epoch, 0) == 1);
  assert(model::remainingSeconds(900000, 0, 900, epoch, 0) == 0);
  assert(model::remainingSeconds(900000, 0, 900, epoch, epoch + 86400) == 86400);
  assert(model::remainingSeconds(0, 0, 900, epoch, epoch + 30) == 900);
  assert(model::remainingSeconds(1000, 0, 900, 0, epoch) == 899);
  assert(model::remainingSeconds(500, UINT32_MAX - 499, 900, epoch, 0) == 899);

  uint32_t mark = 0, uptime = 0, sent = 0;
  uint64_t deadline = 0;
  for (uint32_t second = 0; second < 31UL * 86400; second++) {
    if (second && second % 3600 == 0) { uptime = 0; mark = 0; }
    if (model::remainingSeconds(uptime, mark, 900, epoch + second, deadline) == 0) {
      assert(epoch + second >= deadline);
      deadline = epoch + second + 900;
      mark = uptime;
      sent++;
    }
    uptime += 1000;
  }
  assert(sent > 0 && sent <= 2976);

  for (uint32_t second = 0; second < 86400; second++) {
    assert(model::remainingSeconds((second % 600) * 1000, 0, 900, epoch + second, 0) > 0);
  }
  assert(model::remainingSeconds(900000, 0, 900, epoch - 3600, epoch + 900) == 4500);

  std::cout << "HP20 v0.9.6 core/thermal/indicator/trend/button/cooldown tests passed\n";
}
