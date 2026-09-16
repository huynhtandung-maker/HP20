#include "hp20_ui.h"

#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <cmath>

#include "hp20_thermal.h"
#include "hp20_trend.h"
#include "model.h"
#include "portal.h"
#include "settings.h"
#include "hp20_version.h"

#ifndef OLED_SH1106
#define OLED_SH1106 1  // 1=SH1106, 0=SSD1306; both 128x64 I2C.
#endif

namespace hp20 { namespace ui {
namespace {
#if OLED_SH1106
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#endif

// =============================================================================
// HP20 OLED UX v0.9.13
// =============================================================================
// 128x64 OLED = VERY SMALL information surface.
// Maintenance rules:
//   1) ONE page answers ONE user question.
//   2) ONE moving marquee maximum per page. Never run two scrolling lines together.
//   3) Reserve y=57..63 only for page indicator; content must stay above it.
//   4) Thermal thresholds NEVER belong in this file. UI reads hp20_thermal only.
//   5) Hierarchy: large state/action first -> short context -> optional scrolling detail.
//
// Page map:
//   1/7 CAM NHAN   : What does the room feel like now?
//   2/7 THANG      : Where am I versus the work target zone?
//   3/7 XU HUONG   : Is FEEL rising, stable, or falling?
//   4/7 NHAN DINH  : What does this state mean and what drives it?
//   5/7 HANH DONG  : What should I do now?
//   6/7 GOI Y      : One extra contextual suggestion.
//   7/7 KET NOI    : Wi-Fi / portal / cloud technical state.
// =============================================================================

bool displayOk = false;
bool portalWasActive = false;
uint8_t currentPage = 0;
uint32_t pageSince = 0;
uint32_t drawnAt = 0;

// Full-screen OTA overlay. Busy OTA states are sticky; terminal states remain
// visible briefly so the user gets a clear confirmation instead of a silent
// return to the normal 7-page carousel.
bool otaOverlayVisible = false;
bool otaOverlaySticky = false;
ota::State otaOverlayState = ota::State::Waiting;
uint8_t otaOverlayProgress = 0;
String otaOverlayTarget;
String otaOverlayError;
uint32_t otaOverlaySince = 0;
uint32_t otaOverlayHoldMs = 0;

constexpr uint16_t MARQUEE_STEP_MS = 430;
constexpr uint8_t MARQUEE_HOLD_STEPS = 6;

void drawText(int y, const String& s) {
  oled.drawStr(2, y, s.c_str());
}

void drawCentered(int y, const String& text) {
  const int w = oled.getStrWidth(text.c_str());
  oled.drawStr(max(0, (128 - w) / 2), y, text.c_str());
}

String clipped(const String& text, uint8_t chars) {
  if (text.length() <= chars) return text;
  if (chars <= 2) return text.substring(0, chars);
  return text.substring(0, chars - 2) + "..";
}

// Single-line horizontal marquee.
// IMPORTANT: page layouts below intentionally call this at most ONCE per page.
void scrollLine(int y, String text, uint8_t columns, uint32_t now) {
  for (unsigned i = 0; i < text.length(); ++i) {
    if ((uint8_t)text[i] < 32 || (uint8_t)text[i] > 126) text.setCharAt(i, '?');
  }

  unsigned start = 0;
  if (text.length() > columns) {
    const unsigned distance = text.length() - columns;
    const unsigned cycleSteps = distance * 2 + MARQUEE_HOLD_STEPS * 2;
    const unsigned phase = (uint32_t(now - pageSince) / MARQUEE_STEP_MS) % cycleSteps;

    if (phase < MARQUEE_HOLD_STEPS) {
      start = 0;  // pause at beginning
    } else if (phase < distance + MARQUEE_HOLD_STEPS) {
      start = phase - MARQUEE_HOLD_STEPS;  // move forward
    } else if (phase < distance + MARQUEE_HOLD_STEPS * 2) {
      start = distance;  // pause at end
    } else {
      start = distance * 2 + MARQUEE_HOLD_STEPS * 2 - phase;  // return
    }
  }

  drawText(y, text.substring(start, start + columns));
}

uint32_t marqueeCycleMs(const String& text, uint8_t columns) {
  if (text.length() <= columns) return 0;
  const unsigned distance = text.length() - columns;
  return uint32_t(distance * 2 + MARQUEE_HOLD_STEPS * 2) * MARQUEE_STEP_MS;
}

void title(const char* label) {
  oled.setFont(u8g2_font_5x7_tf);
  drawText(8, label);

  // Wi-Fi icon owns the top-right corner. Page indicator is therefore kept below.
  if (WiFi.status() == WL_CONNECTED) {
    const int rssi = WiFi.RSSI();
    for (int i = 0; i < 3; ++i) {
      if (i == 0 || (i == 1 && rssi > -80) || (i == 2 && rssi > -65)) {
        oled.drawBox(112 + i * 5, 7 - i * 2, 3, 2 + i * 2);
      }
    }
  } else {
    oled.drawLine(117, 2, 123, 8);
    oled.drawLine(117, 8, 123, 2);
  }

  oled.drawHLine(0, 11, 128);
}

// Dedicated footer zone. No content is allowed below baseline y=54.
void pageMark(uint8_t pageNumber) {
  // Footer is intentionally split into two zones:
  //   left  = firmware identity (always visible for field diagnostics)
  //   right = page position dots
  // No other page content may use y=57..63.
  oled.setFont(u8g2_font_4x6_tf);
  String versionText = String("v") + hp20::version::STRING;
  oled.drawStr(2, 63, versionText.c_str());

  const uint8_t startX = 101;
  const uint8_t y = 61;
  for (uint8_t i = 0; i < PAGE_COUNT; ++i) {
    const uint8_t x = startX + i * 4;
    if (i == pageNumber) oled.drawBox(x, y, 3, 2);
    else oled.drawPixel(x + 1, y);
  }
}

int scaleXForFeel(float hi) {
  // Drawing range only. This is NOT a thermal threshold.
  const float lo = 24.0f;
  const float hiMax = 54.0f;
  if (!std::isfinite(hi)) hi = lo;
  if (hi < lo) hi = lo;
  if (hi > hiMax) hi = hiMax;
  const float norm = (hi - lo) / (hiMax - lo);
  return int(8.0f + norm * 112.0f + 0.5f);
}

void drawScaleChart(float shownFeel) {
  // Visual semantics:
  //   thin frame  = total displayed FEEL span
  //   double frame = target work zone from hp20_thermal
  //   vertical pin = current FEEL
  const int y = 33;
  const int h = 7;
  const int x0 = 8;
  const int x1 = 120;
  const int idealL = scaleXForFeel(thermal::optimalLowFeel());
  const int idealR = scaleXForFeel(thermal::optimalHighFeel());
  const int cur = scaleXForFeel(shownFeel);

  oled.drawFrame(x0, y, x1 - x0 + 1, h);
  oled.drawFrame(idealL, y - 2, max(4, idealR - idealL), h + 4);
  oled.drawVLine(cur, y - 4, h + 8);
  oled.drawBox(cur - 1, y - 5, 3, 3);

  oled.setFont(u8g2_font_4x6_tf);
  oled.setCursor(1, 48); oled.print(24);
  oled.setCursor(scaleXForFeel(thermal::optimalLowFeel()) - 5, 48); oled.print(thermal::optimalLowFeel(), 0);
  oled.setCursor(scaleXForFeel(thermal::optimalHighFeel()) - 5, 48); oled.print(thermal::optimalHighFeel(), 0);
  oled.setCursor(scaleXForFeel(44.0f) - 4, 48); oled.print(44);
  oled.setCursor(scaleXForFeel(54.0f) - 4, 48); oled.print(54);
}

void drawTrendChart(float currentFeel) {
  const int x0 = 8, x1 = 120;
  const int yTop = 18, yBottom = 43;

  oled.drawFrame(x0, yTop, x1 - x0 + 1, yBottom - yTop + 1);
  if (trend::count() < 2) {
    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(18, 34, "DANG THU THAP DU LIEU");
    return;
  }

  float mn = trend::value(0), mx = trend::value(0);
  for (uint8_t i = 1; i < trend::count(); ++i) {
    const float v = trend::value(i);
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  if (std::isfinite(currentFeel)) {
    if (currentFeel < mn) mn = currentFeel;
    if (currentFeel > mx) mx = currentFeel;
  }

  float span = mx - mn;
  if (span < 2.0f) {
    const float mid = (mx + mn) * 0.5f;
    mn = mid - 1.0f;
    mx = mid + 1.0f;
    span = 2.0f;
  } else {
    mn -= 0.4f;
    mx += 0.4f;
    span = mx - mn;
  }

  int prevX = x0 + 2;
  int prevY = yBottom - 2 - int((trend::value(0) - mn) / span * float(yBottom - yTop - 4));
  for (uint8_t i = 1; i < trend::count(); ++i) {
    const int x = x0 + 2 + int(float(i) / float(max(1, int(trend::count() - 1))) * float(x1 - x0 - 4));
    const int y = yBottom - 2 - int((trend::value(i) - mn) / span * float(yBottom - yTop - 4));
    oled.drawLine(prevX, prevY, x, y);
    prevX = x;
    prevY = y;
  }
  oled.drawDisc(prevX, prevY, 1);

  oled.setFont(u8g2_font_4x6_tf);
  oled.setCursor(1, 17); oled.print(mx, 1);
  oled.setCursor(1, 50); oled.print(mn, 1);
}

void drawOtaProgressBar(uint8_t pct) {
  if (pct > 100) pct = 100;
  const int x = 8;
  const int y = 39;
  const int w = 112;
  const int h = 8;
  oled.drawFrame(x, y, w, h);
  const int fill = int((uint16_t(w - 2) * pct) / 100U);
  if (fill > 0) oled.drawBox(x + 1, y + 1, fill, h - 2);
}

String otaVersionLine() {
  if (!otaOverlayTarget.isEmpty()) {
    return String("v") + hp20::version::STRING + "  >  v" + otaOverlayTarget;
  }
  return String("HIEN TAI v") + hp20::version::STRING;
}

void otaFooter(const String& text) {
  oled.setFont(u8g2_font_4x6_tf);
  drawCentered(63, clipped(text, 30));
}

void renderOtaOverlay(uint32_t now) {
  if (!displayOk) return;

  oled.clearBuffer();
  title("HP20 / CAP NHAT");

  oled.setFont(u8g2_font_5x7_tf);
  drawCentered(20, clipped(otaVersionLine(), 24));

  switch (otaOverlayState) {
    case ota::State::Checking:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(35, "DANG KIEM TRA");
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(51, "VUI LONG CHO...");
      otaFooter("DANG TIM BAN FIRMWARE");
      break;

    case ota::State::UpdateAvailable:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(35, "CO BAN MOI");
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(51, "CHUAN BI TAI...");
      otaFooter("KHONG TAT NGUON");
      break;

    case ota::State::Downloading: {
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(33, String("DANG TAI  ") + String(otaOverlayProgress) + "%");
      drawOtaProgressBar(otaOverlayProgress);
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(55, "KHONG TAT NGUON");
      otaFooter("DANG NHAN FIRMWARE");
      break;
    }

    case ota::State::Verifying:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(34, "DANG XAC MINH");
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(48, "SHA-256");
      drawCentered(56, "KHONG TAT NGUON");
      otaFooter("DU LIEU DA TAI 100%");
      break;

    case ota::State::Applying:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(34, "DANG CAI DAT");
      drawOtaProgressBar(100);
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(55, "KHONG TAT NGUON");
      otaFooter("DANG GHI FIRMWARE");
      break;

    case ota::State::Restarting:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(34, "HOAN TAT");
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(48, otaOverlayTarget.isEmpty()
                       ? String("FIRMWARE DA CAP NHAT")
                       : String("SAN SANG v") + otaOverlayTarget);
      drawCentered(56, "KHOI DONG LAI...");
      otaFooter("CAP NHAT THANH CONG");
      break;

    case ota::State::UpToDate:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(35, "DA LA BAN MOI NHAT");
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(51, "KHONG CAN CAP NHAT");
      otaFooter(String("FW v") + hp20::version::STRING);
      break;

    case ota::State::Failed:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(34, "CAP NHAT THAT BAI");
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(48, clipped(otaOverlayError.isEmpty()
                               ? String("LOI KHONG XAC DINH")
                               : otaOverlayError, 24));
      drawCentered(56, "KIEM TRA HE THONG");
      otaFooter("CO THE THU LAI");
      break;

    case ota::State::Disabled:
    case ota::State::Waiting:
    default:
      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(35, "OTA SAN SANG");
      oled.setFont(u8g2_font_5x7_tf);
      drawCentered(51, "CHO LENH CAP NHAT");
      otaFooter(String("FW v") + hp20::version::STRING);
      break;
  }

  oled.sendBuffer();
  drawnAt = now;
}

bool otaOverlayExpired(uint32_t now) {
  if (!otaOverlayVisible || otaOverlaySticky) return false;
  return model::elapsed(now, otaOverlaySince, otaOverlayHoldMs);
}

// Dynamic duration ensures the ONE marquee on a page can finish one full cycle.
uint32_t pageDuration(uint8_t pageNumber, bool setupOpen,
                      const sensor::Reading& env, const char* cloudState) {
  uint32_t minimum = 8500;
  uint32_t need = 0;
  const thermal::Band band = thermal::state().band;

  switch (pageNumber) {
    case 0:  // hero
      minimum = 8500;
      break;
    case 1:  // scale
      minimum = 10000;
      break;
    case 2:  // trend
      minimum = 10000;
      break;
    case 3:  // assessment: only meaning scrolls
      minimum = 14000;
      need = marqueeCycleMs(String("Y NGHIA: ") + thermal::meaning(band), 24);
      break;
    case 4:  // action: only actionDetail scrolls
      minimum = 15000;
      need = marqueeCycleMs(String("GOI Y: ") + thermal::actionDetail(band, env.tempC, env.rh), 24);
      break;
    case 5:  // extra tip: only secondTip scrolls
      minimum = 15000;
      need = marqueeCycleMs(String("THEM: ") + thermal::secondTip(band, env.tempC, env.rh), 24);
      break;
    case 6:  // connection/setup: only first line may scroll
      minimum = 10500;
      if (setupOpen) {
        need = marqueeCycleMs(String("AP: ") + portalName(), 24);
      } else {
        need = marqueeCycleMs(
          WiFi.status() == WL_CONNECTED ? String("WIFI: ") + WiFi.SSID()
                                        : String("WIFI: DANG KET NOI"), 24);
      }
      break;
  }

  // A short calm pause after the marquee returns to the start.
  return max(minimum, need + 2200UL);
}

}  // namespace

void begin() {
  Wire.begin(settings::SDA_PIN, settings::SCL_PIN);
  Wire.setTimeOut(50);

  for (uint8_t addr : {uint8_t(0x3c), uint8_t(0x3d)}) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      oled.setI2CAddress(addr * 2);
      displayOk = true;
      break;
    }
  }

  if (displayOk) {
    oled.begin();
    oled.setContrast(100);
    Serial.println("OLED I2C: detected (verify panel driver visually)");
  } else {
    Serial.println("OLED absent: verify I2C wiring/driver");
  }
}

void nextPage(uint32_t now) {
  currentPage = (currentPage + 1) % PAGE_COUNT;
  pageSince = now;
}

void showSetupPage(uint32_t now) {
  currentPage = SETUP_PAGE;
  pageSince = now;
}

uint8_t page() {
  return currentPage;
}

void showOtaState(uint32_t now,
                  ota::State state,
                  uint8_t progress,
                  const char* targetVersion,
                  const char* errorText) {
  otaOverlayState = state;
  otaOverlayProgress = progress > 100 ? 100 : progress;
  otaOverlayTarget = targetVersion ? String(targetVersion) : String();
  otaOverlayError = errorText ? String(errorText) : String();
  otaOverlaySince = now;

  switch (state) {
    case ota::State::Checking:
    case ota::State::UpdateAvailable:
    case ota::State::Downloading:
    case ota::State::Verifying:
    case ota::State::Applying:
    case ota::State::Restarting:
      otaOverlayVisible = true;
      otaOverlaySticky = true;
      otaOverlayHoldMs = 0;
      break;

    case ota::State::UpToDate:
      otaOverlayVisible = true;
      otaOverlaySticky = false;
      otaOverlayHoldMs = 6000UL;
      break;

    case ota::State::Failed:
      otaOverlayVisible = true;
      otaOverlaySticky = false;
      otaOverlayHoldMs = 12000UL;
      break;

    case ota::State::Disabled:
    case ota::State::Waiting:
    default:
      otaOverlayVisible = false;
      otaOverlaySticky = false;
      otaOverlayHoldMs = 0;
      return;
  }

  // Immediate draw is intentional: downloadAndApply() blocks the normal loop.
  renderOtaOverlay(now);
}

void tick(uint32_t now, const sensor::Reading& env, const char* cloudState, const char* otaState) {
  const bool setupOpen = portalActive();
  if (setupOpen && !portalWasActive) showSetupPage(now);
  portalWasActive = setupOpen;

  if (otaOverlayExpired(now)) {
    otaOverlayVisible = false;
    otaOverlaySticky = false;
  }

  if (otaOverlayVisible) {
    if (!displayOk || !model::elapsed(now, drawnAt, 200)) return;
    renderOtaOverlay(now);
    return;
  }

  if (!displayOk || !model::elapsed(now, drawnAt, 200)) return;
  drawnAt = now;

  if (!(setupOpen && currentPage == SETUP_PAGE) &&
      model::elapsed(now, pageSince, pageDuration(currentPage, setupOpen, env, cloudState))) {
    nextPage(now);
  }

  const thermal::State& thermalState = thermal::state();
  const thermal::Band band = thermalState.band;
  const float shownFeel = thermalState.feelUi;

  oled.clearBuffer();

  // ---------------------------------------------------------------------------
  // 1/7 - HERO: one number + one state. Fast glance page.
  // ---------------------------------------------------------------------------
  if (currentPage == 0) {
    title("HP20 / CAM NHAN");

    oled.setFont(u8g2_font_logisoso24_tn);
    const String value = sensor::fresh(now) && std::isfinite(shownFeel)
                       ? String(shownFeel, 1) : "--";
    drawCentered(36, value);

    oled.setFont(u8g2_font_6x10_tf);
    String stateText = thermal::label(band);
    if (oled.getStrWidth(stateText.c_str()) <= 124) drawCentered(48, stateText);
    else drawCentered(48, clipped(stateText, 19));

    oled.setFont(u8g2_font_5x7_tf);
    if (sensor::fresh(now)) {
      drawText(55, String("T ") + String(env.tempC,1) + "C   RH " + String(env.rh,0) + "%");
    } else {
      drawText(55, "DHT22: CHO / LOI");
    }
    pageMark(currentPage);
  }

  // ---------------------------------------------------------------------------
  // 2/7 - SCALE: visual comparison with the locally tuned work target zone.
  // ---------------------------------------------------------------------------
  else if (currentPage == 1) {
    title("HP20 / THANG");
    oled.setFont(u8g2_font_5x7_tf);
    drawText(20, String("MUC TIEU ") + String(thermal::optimalLowFeel(),0)
                 + "-" + String(thermal::optimalHighFeel(),0) + "C");
    drawScaleChart(shownFeel);

    oled.setFont(u8g2_font_5x7_tf);
    drawText(55, clipped(String("VI TRI: ") + thermal::chartRelation(band), 24));
    pageMark(currentPage);
  }

  // ---------------------------------------------------------------------------
  // 3/7 - TREND: graph first, one concise status second.
  // ---------------------------------------------------------------------------
  else if (currentPage == 2) {
    title("HP20 / XU HUONG 10P");
    drawTrendChart(shownFeel);

    oled.setFont(u8g2_font_5x7_tf);
    String trendText = String(trend::label());
    if (trend::count() >= 4) {
      const float d = trend::delta();
      trendText += " ";
      if (d > 0) trendText += "+";
      trendText += String(d,1) + "C";
    }
    drawText(54, clipped(trendText, 22));
    pageMark(currentPage);
  }

  // ---------------------------------------------------------------------------
  // 4/7 - ASSESSMENT: big state + static cause + ONE scrolling explanation.
  // ---------------------------------------------------------------------------
  else if (currentPage == 3) {
    title("HP20 / NHAN DINH");

    oled.setFont(u8g2_font_6x10_tf);
    String stateText = thermal::label(band);
    if (oled.getStrWidth(stateText.c_str()) <= 124) drawCentered(25, stateText);
    else drawCentered(25, clipped(stateText, 19));

    oled.setFont(u8g2_font_5x7_tf);
    drawText(38, clipped(String("TAC NHAN: ") + thermal::driverText(env.tempC, env.rh), 24));
    scrollLine(52, String("Y NGHIA: ") + thermal::meaning(band), 24, now);
    pageMark(currentPage);
  }

  // ---------------------------------------------------------------------------
  // 5/7 - ACTION: one dominant command + ONE scrolling detailed action.
  // ---------------------------------------------------------------------------
  else if (currentPage == 4) {
    title("HP20 / HANH DONG");

    oled.setFont(u8g2_font_6x10_tf);
    String primary = thermal::actionPrimary(band);
    if (oled.getStrWidth(primary.c_str()) <= 124) drawCentered(26, primary);
    else drawCentered(26, clipped(primary, 19));

    oled.setFont(u8g2_font_5x7_tf);
    drawText(39, clipped(String("MUC: ") + thermal::label(band), 24));
    scrollLine(52, String("GOI Y: ") + thermal::actionDetail(band, env.tempC, env.rh), 24, now);
    pageMark(currentPage);
  }

  // ---------------------------------------------------------------------------
  // 6/7 - EXTRA TIP: contextual advice gets its OWN page instead of competing
  //       with the action sentence. This removes the old two-marquee clutter.
  // ---------------------------------------------------------------------------
  else if (currentPage == 5) {
    title("HP20 / GOI Y THEM");

    oled.setFont(u8g2_font_6x10_tf);
    drawCentered(25, clipped(thermal::label(band), 19));

    oled.setFont(u8g2_font_5x7_tf);
    if (sensor::fresh(now)) {
      drawText(38, String("T ") + String(env.tempC,1) + "C   RH " + String(env.rh,0) + "%");
    } else {
      drawText(38, "CHO DU LIEU CAM BIEN");
    }
    scrollLine(52, String("THEM: ") + thermal::secondTip(band, env.tempC, env.rh), 24, now);
    pageMark(currentPage);
  }

  // ---------------------------------------------------------------------------
  // 7/7 - TECHNICAL: networking isolated from human thermal guidance.
  //        Only ONE line scrolls here too.
  // ---------------------------------------------------------------------------
  else {
    title(setupOpen ? "HP20 / CAI DAT" : "HP20 / KET NOI");
    oled.setFont(u8g2_font_5x7_tf);

    if (setupOpen) {
      scrollLine(25, String("AP: ") + portalName(), 24, now);
      drawText(39, clipped(String("PW: ") + portalPassword(), 24));
      drawText(52, "WEB: 192.168.4.1");
    } else {
      scrollLine(25,
                 WiFi.status() == WL_CONNECTED ? String("WIFI: ") + WiFi.SSID()
                                                : String("WIFI: DANG KET NOI"),
                 24, now);
      drawText(39, clipped(String(cloudState), 24));
      const String otaLine = otaState ? String(otaState) : String("OTA: ?");
      if (otaLine == "OTA: SAN SANG" || otaLine == "OTA: TAT" || otaLine == "OTA: MOI NHAT")
        drawText(52, "BOOT 3s = CAI DAT");
      else
        drawText(52, clipped(otaLine, 24));
    }
    pageMark(currentPage);
  }

  oled.sendBuffer();
}

} }  // namespace hp20::ui
