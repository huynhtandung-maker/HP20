#include "hp20_indicator.h"

namespace hp20 { namespace indicator {

GreenPattern greenPatternForBand(thermal::Band band) {
  // ---------------------------------------------------------------------------
  // GREEN LED COMFORT SCALE - USER-TUNABLE UX SIGNAL
  // ---------------------------------------------------------------------------
  // Read each row as:
  //   { Mode, full cycle, green ON time, approximate green presence }
  //
  // Philosophy:
  //   - Comfort is the reference: solid green = condition worth maintaining.
  //   - As comfort decreases, green appears less often AND for less time.
  //   - The hottest bands blink SLOWLY, not rapidly, because rapid blinking
  //     visually resembles an alarm. The buzzer/reminder owns alarm behavior.
  //   - Cool-side bands are also below optimum, so they do not receive 100% green.
  // ---------------------------------------------------------------------------
  switch (band) {
    case thermal::Band::NoData:
      // No valid thermal interpretation -> no green comfort claim.
      return {Mode::Off, 0, 0, 0};

    case thermal::Band::Cool:
      // Clearly cooler than target: some comfort, but not "maintain optimum".
      // 1.2 s ON every 4.0 s -> ~30% green presence.
      return {Mode::Blink, 4000, 1200, 30};

    case thermal::Band::MildCool:
      // Near-comfort cool side: mostly green, short dark pause.
      // 2.0 s ON every 2.8 s -> ~71% green presence.
      return {Mode::Blink, 2800, 2000, 71};

    case thermal::Band::Comfort:
      // OPTIMAL semantic: continuous green means "good, maintain this condition".
      return {Mode::SteadyOn, 0, 0, 100};

    case thermal::Band::WarmComfort:
      // Still acceptable but moving away from optimum.
      // 2.2 s ON every 3.0 s -> ~73% green presence.
      return {Mode::Blink, 3000, 2200, 73};

    case thermal::Band::SlightStuffy:
      // First noticeable loss of comfort.
      // 1.8 s ON every 3.5 s -> ~51% green presence.
      return {Mode::Blink, 3500, 1800, 51};

    case thermal::Band::MildStuffy:
      // Mild stuffiness: green is now clearly less present than absent.
      // 1.4 s ON every 4.0 s -> 35% green presence.
      return {Mode::Blink, 4000, 1400, 35};

    case thermal::Band::Stuffy:
      // Oi / stuffy: short green confirmation separated by long OFF time.
      // 1.1 s ON every 4.5 s -> ~24% green presence.
      return {Mode::Blink, 4500, 1100, 24};

    case thermal::Band::StuffyHot:
      // Oi nong: green becomes a brief pulse.
      // 0.9 s ON every 5.0 s -> 18% green presence.
      return {Mode::Blink, 5000, 900, 18};

    case thermal::Band::Hot:
      // Hot: low comfort. Slow cycle avoids looking like an emergency strobe.
      // 0.75 s ON every 6.0 s -> ~12% green presence.
      return {Mode::Blink, 6000, 750, 12};

    case thermal::Band::HighHot:
      // High hot: green is rare.
      // 0.6 s ON every 7.0 s -> ~9% green presence.
      return {Mode::Blink, 7000, 600, 9};

    case thermal::Band::VeryHot:
      // Very hot: only a short green heartbeat remains.
      // 0.45 s ON every 8.0 s -> ~6% green presence.
      return {Mode::Blink, 8000, 450, 6};

    case thermal::Band::HeatLoad:
      // High heat load: almost no green. Long OFF communicates poor comfort.
      // 0.35 s ON every 9.0 s -> ~4% green presence.
      return {Mode::Blink, 9000, 350, 4};

    case thermal::Band::HighLoad:
      // Worst current band: VERY SLOW, VERY SHORT green pulse.
      // 0.25 s ON every 10.0 s -> ~2.5% green presence.
      // This matches the intended semantics: comfort is nearly absent.
      return {Mode::Blink, 10000, 250, 3};
  }

  return {Mode::Off, 0, 0, 0};
}

bool greenLedOn(thermal::Band band, uint32_t nowMs) {
  const GreenPattern p = greenPatternForBand(band);

  if (p.mode == Mode::SteadyOn) return true;
  if (p.mode == Mode::Off) return false;

  // Defensive fallback: invalid blink parameters must never become solid green.
  if (p.periodMs == 0 || p.onMs == 0 || p.onMs >= p.periodMs) return false;

  return (nowMs % p.periodMs) < p.onMs;
}

uint8_t greenPresencePercent(thermal::Band band) {
  return greenPatternForBand(band).presencePct;
}

} } // namespace hp20::indicator
