#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#include "settings.h"
#include "hp20_version.h"
#include "model.h"
#include "config.h"
#include "cloud.h"
#include "portal.h"

#include "hp20_sensor.h"
#include "hp20_thermal.h"
#include "hp20_trend.h"
#include "hp20_ui.h"
#include "hp20_indicator.h"
#include "hp20_provisioning.h"
#include "hp20_ota.h"
#include "thingsboard_ca.h"

// =============================================================================
// COMPONENT - APPLICATION / ORCHESTRATOR
// =============================================================================
// Responsibility:
//   Start modules, coordinate network/cloud/control flow, and call each tick().
//
// MAINTENANCE RULE:
//   Do NOT place Heat Index thresholds, DHT calibration rules, trend algorithms,
//   or OLED drawing code here. Those belong to their dedicated components.
//
// Existing infrastructure remains unchanged:
//   config.* = persisted config
//   portal.* = AP/captive portal
//   cloud.*  = HTTPS/ThingsBoard transport
//   model.h  = shared utilities: Button, Reminder, HI formula, retry helpers
// =============================================================================

Config config;
Preferences runtime;
model::Reminder reminder;
model::Button bootButton;

bool cloudReady = false;
bool inFlight = false;
bool authBlocked = false;
bool remind = false;
bool wasConnected = false;
bool ignoreOldResult = false;
bool buzzing = false;
bool bootBeeping = false;

// Wi-Fi reconnect state. HP20 remembers multiple networks in Config/NVS and
// tries them in preferred/last-good order without asking the user again.
bool wifiConnecting = false;
bool wifiRetryPause = false;
bool portalConnectionSession = false;
uint8_t wifiProfileCursor = 0;
uint32_t wifiAttemptSince = 0;
uint32_t wifiRetryPauseSince = 0;
uint32_t offlineSince = 0;
uint32_t connectedSince = 0;
uint32_t portalSuccessSince = 0;
String wifiAttemptSsid;
String wifiUiState = "CHUA CO MANG";
uint32_t beepSince = 0;
uint32_t lastBeep = 0;
uint32_t bootBeepSince = 0;
uint32_t sendMark = 0;
uint32_t waitSeconds = 900;
uint32_t statusSince = 0;
uint32_t ntpRetrySince = 0;
uint64_t notBefore = 0;
unsigned failures = 0;
const char* cloudState = "CHUA CAU HINH";

uint32_t nextSendSeconds(uint32_t now) {
  return model::remainingSeconds(
    now,
    sendMark,
    waitSeconds,
    (uint64_t)time(nullptr),
    notBefore
  );
}

void buzzerOff() {
  if (settings::PASSIVE_BUZZER) noTone(settings::BUZZER_PIN);
  digitalWrite(settings::BUZZER_PIN, LOW);
}

void startBootBuzzer(uint32_t now) {
  if (!settings::BUZZER_BOOT_TEST || bootBeeping) return;
  bootBeeping = true;
  bootBeepSince = now;
  lastBeep = now;
  if (settings::PASSIVE_BUZZER) tone(settings::BUZZER_PIN, 2200);
  else digitalWrite(settings::BUZZER_PIN, HIGH);
  Serial.println("BUZZER boot test: ON");
}

void otaBuzzerPulse(uint16_t onMs, uint16_t gapMs = 0) {
  if (settings::PASSIVE_BUZZER) tone(settings::BUZZER_PIN, 2300);
  else digitalWrite(settings::BUZZER_PIN, HIGH);

  delay(onMs);
  buzzerOff();
  if (gapMs) delay(gapMs);
}

void otaUxCallback(hp20::ota::State state,
                   uint8_t progress,
                   const char* targetVersion,
                   const char* errorText) {
  // OLED first: even if the buzzer is sounding, the user immediately sees why.
  hp20::ui::showOtaState(millis(), state, progress, targetVersion, errorText);

  // During maintenance the thermal-comfort green signal is not meaningful.
  // Turn it off until normal loop control resumes or the MCU reboots.
  if (state == hp20::ota::State::Checking ||
      state == hp20::ota::State::UpdateAvailable ||
      state == hp20::ota::State::Downloading ||
      state == hp20::ota::State::Verifying ||
      state == hp20::ota::State::Applying ||
      state == hp20::ota::State::Restarting) {
    digitalWrite(settings::LED_PIN, LOW);
  }

  static hp20::ota::State lastState = hp20::ota::State::Disabled;
  if (state == lastState) return;

  // Prevent a thermal reminder or boot-test tone from being left active when an
  // OTA maintenance session takes ownership of the buzzer.
  buzzing = false;
  bootBeeping = false;
  buzzerOff();

  switch (state) {
    case hp20::ota::State::Checking:
      // One short acknowledgement: the update request was received.
      otaBuzzerPulse(45);
      break;

    case hp20::ota::State::UpdateAvailable:
      // If this update was discovered in the background there was no CHECKING
      // acknowledgement, so emit one concise attention pulse.
      if (lastState != hp20::ota::State::Checking) otaBuzzerPulse(55);
      break;

    case hp20::ota::State::Applying:
      // Two short pulses mark the critical flash/write phase.
      otaBuzzerPulse(40, 45);
      otaBuzzerPulse(85);
      break;

    case hp20::ota::State::Restarting:
      // Positive completion signature before reboot.
      otaBuzzerPulse(55, 45);
      otaBuzzerPulse(120);
      break;

    case hp20::ota::State::UpToDate:
      // The user's explicit check completed and no upgrade is required.
      otaBuzzerPulse(75);
      break;

    case hp20::ota::State::Failed:
      // Distinct triple pulse: update stopped and needs attention.
      otaBuzzerPulse(85, 55);
      otaBuzzerPulse(85, 55);
      otaBuzzerPulse(120);
      break;

    case hp20::ota::State::Disabled:
    case hp20::ota::State::Waiting:
    case hp20::ota::State::Downloading:
    case hp20::ota::State::Verifying:
    default:
      break;
  }

  lastState = state;
}

void startWifiProfileAttempt(uint8_t index, uint32_t now) {
  if (index >= config.wifiProfileCount || index >= MAX_WIFI_PROFILES) {
    wifiConnecting = false;
    return;
  }

  portalCancelScan();

  const WifiProfile& profile = config.wifiProfiles[index];
  if (profile.ssid.isEmpty()) {
    wifiConnecting = false;
    return;
  }

  wifiProfileCursor = index;
  wifiAttemptSsid = profile.ssid;
  wifiAttemptSince = now;
  wifiConnecting = true;
  wifiRetryPause = false;
  wifiUiState = "DANG THU MANG DA LUU";

  // Keep AP alive during a setup session; WiFi.begin() works in AP+STA mode.
  if (!portalActive()) WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(25);
  WiFi.begin(profile.ssid.c_str(), profile.password.c_str());

  if (portalActive() && portalConnectionSession) {
    portalSetStatus(
      PortalPhase::ConnectingWifi, 30,
      "Đang kết nối Wi-Fi",
      String("Đang xác thực với ") + profile.ssid,
      profile.ssid
    );
  }

  Serial.printf("WIFI try %u/%u: %s\n",
                unsigned(index + 1),
                unsigned(config.wifiProfileCount),
                profile.ssid.c_str());
}

void restartWifiCycle(uint32_t now) {
  wifiProfileCursor = 0;
  wifiRetryPause = false;
  wifiConnecting = false;

  if (config.wifiProfileCount == 0) {
    wifiUiState = "CHUA CO MANG";
    return;
  }
  startWifiProfileAttempt(0, now);
}

void persistCooldown(uint32_t seconds) {
  notBefore = (uint64_t)time(nullptr) + seconds;
  if (runtime.putULong64("notBefore", notBefore) != sizeof(uint64_t)) {
    cloudReady = false;
    cloudState = "LOI LUU HAN GUI";
  }
}

void resetCloudAfterProvisionSave(uint32_t now) {
  ignoreOldResult = inFlight;
  authBlocked = false;
  if (runtime.putBool("auth", false) != sizeof(bool)) {
    cloudReady = false;
    cloudState = "LOI LUU TRANG THAI";
  }

  failures = 0;
  cloudState = "TB: CHO WI-FI";
  sendMark = now;
  waitSeconds = 5;
  notBefore = 0;
  runtime.putULong64("notBefore", 0);

  remind = false;
  reminder = model::Reminder();
}

void networkTick(uint32_t now) {
  portalTick();

  if (portalSaved()) {
    resetCloudAfterProvisionSave(now);
    portalConnectionSession = config.wifiProfileCount > 0;
    portalSuccessSince = 0;

    WiFi.disconnect(false, false);
    wasConnected = false;
    offlineSince = now;
    restartWifiCycle(now);

    if (config.otaEnabled) hp20::ota::requestCheck();
  }

  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected) {
    if (!wasConnected) {
      connectedSince = now;
      offlineSince = 0;
      wifiConnecting = false;
      wifiRetryPause = false;
      wifiUiState = "WIFI: DA KET NOI";

      // A successfully used remembered network becomes preferred/last-good.
      const String actualSsid = WiFi.SSID();
      const String actualPass = wifiPasswordFor(config, actualSsid);
      if (!actualSsid.isEmpty() &&
          rememberWifiProfile(config, actualSsid, actualPass, true)) {
        if (!saveConfig(config))
          Serial.println("WIFI warning: could not persist last-good profile");
      }

      configTime(0, 0, "time.cloudflare.com", "time.google.com", "pool.ntp.org");
      ntpRetrySince = now;
      Serial.printf("WIFI connected: %s RSSI=%d IP=%s\n",
                    actualSsid.c_str(), WiFi.RSSI(),
                    WiFi.localIP().toString().c_str());
      Serial.println("TIME sync requested (NTP)");

      if (portalActive() && portalConnectionSession) {
        portalSetStatus(
          PortalPhase::WifiConnected, 55,
          "Wi-Fi đã kết nối",
          "Đã nhận địa chỉ mạng. Đang chuẩn bị kết nối cloud.",
          actualSsid,
          WiFi.localIP().toString()
        );
      }
    }
    wasConnected = true;
    return;
  }

  if (wasConnected) {
    wasConnected = false;
    offlineSince = now;
    wifiConnecting = false;
    wifiRetryPause = false;
    wifiProfileCursor = 0;
    wifiUiState = "MAT KET NOI - DANG THU LAI";
    Serial.println("WIFI lost: automatic reconnect cycle started");
  }

  if (config.wifiProfileCount == 0) {
    wifiUiState = "CHUA CO MANG WIFI";
    // First-use device may expose setup automatically. Once at least one network
    // has been provisioned, ordinary Wi-Fi loss never exposes an open AP.
    if (!portalActive()) portalBegin(&config);
    return;
  }

  if (wifiConnecting) {
    if (!model::elapsed(now, wifiAttemptSince, 12000UL)) return;

    Serial.printf("WIFI timeout: %s\n", wifiAttemptSsid.c_str());
    wifiConnecting = false;

    // During explicit provisioning, validate exactly the network the user
    // selected. Do not silently fall back to an older remembered network and
    // falsely report that the new credentials worked.
    if (portalActive() && portalConnectionSession) {
      wifiRetryPause = true;
      wifiRetryPauseSince = now;
      wifiUiState = "SAI MAT KHAU / KHONG TIM THAY";
      portalSetStatus(
        PortalPhase::Failed, 30,
        "Chưa kết nối được Wi-Fi",
        "Kiểm tra mật khẩu hoặc chọn mạng khác. AP cài đặt vẫn đang mở.",
        config.ssid
      );
      return;
    }

    const uint8_t next = wifiProfileCursor + 1;
    if (next < config.wifiProfileCount) {
      startWifiProfileAttempt(next, now);
      return;
    }

    wifiProfileCursor = 0;
    wifiRetryPause = true;
    wifiRetryPauseSince = now;
    wifiUiState = "CHUA KET NOI DUOC";
  }

  if (portalActive() && portalConnectionSession &&
      portalPhase() == PortalPhase::Failed) {
    return;
  }

  if (wifiRetryPause) {
    if (!model::elapsed(now, wifiRetryPauseSince, 10000UL)) return;
    restartWifiCycle(now);
    return;
  }

  if (!wifiConnecting) restartWifiCycle(now);
}

void syncPortalConnectionStatus(uint32_t now) {
  if (!portalActive() || !portalConnectionSession) return;

  if (WiFi.status() != WL_CONNECTED) return;

  const time_t epochNow = time(nullptr);
  if (epochNow < 1704067200) {
    if (portalPhase() != PortalPhase::SyncingTime) {
      portalSetStatus(
        PortalPhase::SyncingTime, 70,
        "Đang đồng bộ thời gian",
        "Bước này cần cho kết nối HTTPS an toàn.",
        WiFi.SSID(),
        WiFi.localIP().toString()
      );
    }
    return;
  }

  if (config.host.isEmpty() || config.token.isEmpty()) {
    if (portalPhase() != PortalPhase::Success) {
      portalSetStatus(
        PortalPhase::Success, 100,
        "Wi-Fi đã sẵn sàng",
        "Thiết bị đã kết nối mạng; ThingsBoard chưa được cấu hình.",
        WiFi.SSID(),
        WiFi.localIP().toString()
      );
      portalSuccessSince = now;
    }
  }
  else if (authBlocked || strcmp(cloudState, "TB: KIEM TRA TOKEN") == 0) {
    portalSetStatus(
      PortalPhase::Failed, 85,
      "Wi-Fi OK, cloud chưa xác thực",
      "Kiểm tra Device access token trong phần Cloud.",
      WiFi.SSID(),
      WiFi.localIP().toString()
    );
    return;
  }
  else if (!cloudReady ||
           strcmp(cloudState, "TB: THIEU CA TLS") == 0 ||
           strcmp(cloudState, "TB: LOI / CHO LAI") == 0 ||
           strcmp(cloudState, "TB: TAM DUNG 24H") == 0 ||
           strcmp(cloudState, "TB: LOI HANG DOI") == 0) {
    portalSetStatus(
      PortalPhase::Failed, 85,
      "Wi-Fi OK, cloud chưa sẵn sàng",
      String("Trạng thái: ") + cloudState,
      WiFi.SSID(),
      WiFi.localIP().toString()
    );
    return;
  }
  else if (strcmp(cloudState, "TB: DA NHAN") == 0) {
    if (portalPhase() != PortalPhase::Success) {
      portalSetStatus(
        PortalPhase::Success, 100,
        "Hoàn tất kết nối",
        "Wi-Fi và ThingsBoard đều đã sẵn sàng.",
        WiFi.SSID(),
        WiFi.localIP().toString()
      );
      portalSuccessSince = now;
    }
  }
  else {
    if (portalPhase() != PortalPhase::ConnectingCloud) {
      portalSetStatus(
        PortalPhase::ConnectingCloud, 86,
        "Đang kết nối ThingsBoard",
        "Wi-Fi đã ổn định. Đang xác nhận cloud.",
        WiFi.SSID(),
        WiFi.localIP().toString()
      );
    }
    return;
  }

  // Leave enough time for the phone to render SUCCESS before the AP disappears.
  if (portalPhase() == PortalPhase::Success &&
      portalSuccessSince != 0 &&
      model::elapsed(now, portalSuccessSince, 12000UL)) {
    portalClose();
    portalConnectionSession = false;
    portalSuccessSince = 0;
  }
}

void portalUxBuzzerTick() {
  static PortalPhase lastPhase = PortalPhase::Idle;
  const PortalPhase phase = portalActive() ? portalPhase() : PortalPhase::Idle;
  if (phase == lastPhase) return;

  // Provisioning owns the buzzer while its state changes.
  buzzing = false;
  bootBeeping = false;
  buzzerOff();

  switch (phase) {
    case PortalPhase::Ready:
      otaBuzzerPulse(45, 40);
      otaBuzzerPulse(70);
      break;
    case PortalPhase::ScanningNetworks:
      if (lastPhase == PortalPhase::Idle) {
        otaBuzzerPulse(45, 40);
        otaBuzzerPulse(70);
      }
      break;
    case PortalPhase::ConnectingWifi:
      otaBuzzerPulse(55);
      break;
    case PortalPhase::Success:
      otaBuzzerPulse(45, 45);
      otaBuzzerPulse(45, 45);
      otaBuzzerPulse(120);
      break;
    case PortalPhase::Failed:
      otaBuzzerPulse(80, 55);
      otaBuzzerPulse(80, 55);
      otaBuzzerPulse(110);
      break;
    default:
      break;
  }

  lastPhase = phase;
}

void cloudTick(uint32_t now) {
  int result;

  if (hp20::ota::busy()) return;

  if (cloudResult(result)) {
    inFlight = false;

    if (ignoreOldResult && result != 429) {
      ignoreOldResult = false;
      return;
    }
    ignoreOldResult = false;

    if (result >= 200 && result < 300) {
      failures = 0;
      cloudState = "TB: DA NHAN";
    }
    else if (result == 401 || result == 403) {
      authBlocked = true;
      cloudState = "TB: KIEM TRA TOKEN";
      if (runtime.putBool("auth", true) != sizeof(bool)) {
        cloudReady = false;
        cloudState = "LOI LUU TRANG THAI";
      }
    }
    else if (result == 429) {
      waitSeconds = 86400;
      sendMark = now;
      persistCooldown(waitSeconds);
      cloudState = "TB: TAM DUNG 24H";
    }
    else {
      failures++;
      waitSeconds = model::retrySeconds(config.intervalSeconds, failures);
      sendMark = now;
      persistCooldown(waitSeconds);
      cloudState = "TB: LOI / CHO LAI";
    }
  }

  if (!cloudReady || inFlight) return;
  if (config.host.isEmpty() || config.token.isEmpty()) {
    cloudState = "TB: CHUA CAU HINH";
    return;
  }
  if (!hp20::tbtrust::hasEffectiveCa(config)) {
    cloudState = "TB: THIEU CA TLS";
    return;
  }
  if (authBlocked) {
    cloudState = "TB: KIEM TRA TOKEN";
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    cloudState = "TB: CHO WI-FI";
    return;
  }
  const time_t epochNow = time(nullptr);
  if (epochNow < 1704067200) {
    cloudState = "TB: DONG BO GIO";

    // configTime() is asynchronous. Normally one request is enough, but some
    // routers/DNS paths drop the first NTP exchange. Re-arm SNTP periodically
    // instead of leaving the device in a stale wait state forever.
    const uint32_t ntpRetryMs = portalConnectionSession ? 10000UL : 30000UL;
    if (model::elapsed(now, ntpRetrySince, ntpRetryMs)) {
      configTime(0, 0, "time.cloudflare.com", "time.google.com", "pool.ntp.org");
      ntpRetrySince = now;
      Serial.println("TIME sync retry (NTP)");
    }
    return;
  }
  if (!hp20::sensor::fresh(now)) {
    cloudState = "TB: CHO CAM BIEN";
    return;
  }

  const uint32_t remaining = nextSendSeconds(now);
  if (remaining != 0) {
    // Do not leave the previous state (for example "CHO DONG HO") stuck on
    // screen after its condition has already cleared.
    if (strcmp(cloudState, "TB: DA NHAN") != 0) cloudState = "TB: CHO LICH GUI";
    return;
  }

  const hp20::sensor::Reading& env = hp20::sensor::state();
  const hp20::thermal::State& th = hp20::thermal::state();

  StaticJsonDocument<256> d;
  d["temperature"] = env.tempC;
  d["humidity"] = env.rh;
  if (isfinite(th.feelRaw)) d["heat_index_c"] = th.feelRaw;
  d["heat_index_valid"] = isfinite(th.feelRaw);

  String payload;
  serializeJson(d, payload);

  // Preserve baseline anti-spam rule: reserve next slot BEFORE transmission.
  sendMark = now;
  waitSeconds = config.intervalSeconds;
  persistCooldown(waitSeconds);

  if (!cloudReady) return;

  inFlight = cloudSubmit(config, payload);
  cloudState = inFlight ? "TB: DANG GUI" : "TB: LOI HANG DOI";
}

void controlsTick(uint32_t now) {
  const model::ButtonEvent event = bootButton.update(
    now,
    digitalRead(settings::BUTTON_PIN) == LOW
  );

  if (event == model::ButtonEvent::Hold) {
    portalBegin(&config);
    hp20::ui::showSetupPage(now);
    Serial.println("BOOT hold: setup requested");
  }
  else if (event == model::ButtonEvent::Click) {
    hp20::ui::nextPage(now);
    Serial.printf(
      "BOOT click: page %u/%u\n",
      unsigned(hp20::ui::page() + 1),
      unsigned(hp20::ui::PAGE_COUNT)
    );
  }

  const hp20::thermal::State& th = hp20::thermal::state();

  // Thermal reminders are suspended while setup/OTA owns the user's attention.
  if (portalActive() || hp20::ota::busy()) {
    remind = false;
    reminder = model::Reminder();
  } else {
    remind = reminder.update(
      now,
      hp20::sensor::fresh(now) ? th.feelRaw : NAN,
      config.reminder,
      config.threshold
    );
  }

  // During maintenance/setup the external green comfort signal is intentionally
  // suppressed so one LED never communicates two meanings at once.
  const bool greenOn = !portalActive() && !hp20::ota::busy() &&
                       hp20::indicator::greenLedOn(th.band, now);
  digitalWrite(settings::LED_PIN, greenOn ? HIGH : LOW);

  if (settings::BOARD_LED_ENABLED) {
    bool blue = false;

    if (now < 800) {
      blue = (now % 400) < 80;
    }
    else if (!hp20::sensor::fresh(now)) {
      const uint32_t p = now % 3000;
      blue = p < 80 || (p >= 200 && p < 280) || (p >= 400 && p < 480);
    }
    else if (hp20::ota::busy()) blue = (now % 500) < 250;
    else if (remind) blue = (now % 1000) < 150;
    else if (portalActive()) {
      const uint32_t p = now % 2000;
      blue = p < 80 || (p >= 200 && p < 280);
    }
    else if (WiFi.status() != WL_CONNECTED) blue = (now % 2000) < 80;
    else if (inFlight) blue = (now % 400) < 60;
    else blue = (now % 8000) < 50;

    digitalWrite(
      settings::BOARD_LED_PIN,
      blue == settings::BOARD_LED_ACTIVE_HIGH ? HIGH : LOW
    );
  }

  if (bootBeeping && model::elapsed(now, bootBeepSince, settings::BUZZER_BOOT_TEST_MS)) {
    buzzerOff();
    bootBeeping = false;
    Serial.println("BUZZER boot test: OFF");
  }

  if (buzzing && (!remind || !config.sound || model::elapsed(now, beepSince, 150))) {
    buzzerOff();
    buzzing = false;
  }

  if (remind && config.sound && !buzzing && !bootBeeping &&
      model::elapsed(now, lastBeep, 300000)) {
    lastBeep = beepSince = now;
    buzzing = true;

    if (settings::PASSIVE_BUZZER) tone(settings::BUZZER_PIN, 2200);
    else digitalWrite(settings::BUZZER_PIN, HIGH);
  }
}

void serialTick(uint32_t now) {
  static char command[16];
  static uint8_t used = 0;
  static bool overflow = false;

  for (unsigned count = 0; count < 32 && Serial.available(); ++count) {
    const char ch = char(Serial.read());

    if (ch == '\r' || ch == '\n') {
      if (overflow) {
        Serial.println("Command too long; use INFO, SETUP, BEEP or OTA");
      }
      else if (used) {
        command[used] = '\0';

        if (strcmp(command, "SETUP") == 0) {
          portalBegin(&config);
          hp20::ui::showSetupPage(now);

          if (portalActive()) {
            Serial.printf(
              "SETUP AP=%s OPEN_AP=YES URL=http://192.168.4.1\n",
              portalName().c_str()
            );
          } else {
            Serial.println("SETUP failed: could not start AP");
          }
        }
        else if (strcmp(command, "INFO") == 0) {
          statusSince = now - settings::SERIAL_STATUS_MS;
        }
        else if (strcmp(command, "BEEP") == 0) {
          startBootBuzzer(now);
          Serial.println("BUZZER test requested");
        }
        else if (strcmp(command, "OTA") == 0) {
          hp20::ota::requestCheck();
          Serial.println("OTA check requested");
        }
        else {
          Serial.println("Commands: INFO, SETUP, BEEP, OTA (New Line)");
        }
      }

      used = 0;
      overflow = false;
    }
    else if (!overflow) {
      if (used < sizeof(command) - 1) command[used++] = ch;
      else overflow = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf(
    "HP20 v%s boot | %s\n",
    hp20::version::STRING,
    hp20::thermal::MODEL_VERSION
  );
  Serial.println("Commands: INFO, SETUP, BEEP, OTA. Status every 10s.");
  Serial.printf(
    "DHT22 calibration: TEMP=%+.2f%%  RH=%+.2f%%  (safety limit +/-%.0f%%)\n",
    hp20::sensor::safeCorrectionPercent(hp20::sensor::TEMP_CORRECTION_PERCENT),
    hp20::sensor::safeCorrectionPercent(hp20::sensor::HUMIDITY_CORRECTION_PERCENT),
    hp20::sensor::CORRECTION_LIMIT_PERCENT
  );

  if (settings::BOARD_LED_ENABLED) {
    digitalWrite(
      settings::BOARD_LED_PIN,
      settings::BOARD_LED_ACTIVE_HIGH ? LOW : HIGH
    );
    pinMode(settings::BOARD_LED_PIN, OUTPUT);
  }

  digitalWrite(settings::LED_PIN, LOW);
  digitalWrite(settings::BUZZER_PIN, LOW);
  pinMode(settings::LED_PIN, OUTPUT);
  pinMode(settings::BUZZER_PIN, OUTPUT);
  pinMode(settings::BUTTON_PIN, INPUT_PULLUP);
  startBootBuzzer(millis());

  hp20::sensor::begin();
  hp20::thermal::reset();
  hp20::trend::reset();
  hp20::ui::begin();
  hp20::ota::setUxCallback(otaUxCallback);
  hp20::ota::begin();

  const bool configLoaded = loadConfig(config);
  const bool localSeededNow = hp20::provisioning::seedFromLocalSecrets(config);
  Serial.printf("Provisioning: NVS=%s local_secrets=%s seeded_now=%s wifi=%s\n",
                configLoaded ? "OK" : "EMPTY",
                hp20::provisioning::localSecretsCompiled() ? "YES" : "NO",
                localSeededNow ? "YES" : "NO",
                config.ssid.isEmpty() ? "EMPTY" : "READY");

  const bool storageOk = runtime.begin("room-runtime", false);
  if (storageOk) {
    notBefore = runtime.getULong64("notBefore", 0);
    authBlocked = runtime.getBool("auth", false);

    // A newly applied developer-local profile is an explicit credential change.
    // Clear stale cloud cooldown/auth state once so the new profile can be
    // validated immediately.
    if (localSeededNow) {
      notBefore = 0;
      authBlocked = false;
      runtime.putULong64("notBefore", 0);
      runtime.putBool("auth", false);
    }
  }

  cloudReady = storageOk && cloudBegin();
  if (!cloudReady) cloudState = "LOI BO NHO / CLOUD";

  // First boot/config validation should be observable quickly. After the first
  // telemetry attempt, cloudTick() restores config.intervalSeconds BEFORE send.
  sendMark = millis();
  waitSeconds = 10;

  WiFi.persistent(false);
  WiFi.setHostname("hp20-room");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  normalizeWifiProfiles(config);
  if (config.wifiProfileCount > 0) {
    restartWifiCycle(millis());
  } else {
    // First-use experience: open captive setup immediately. No AP password.
    portalBegin(&config);
  }
}

void loop() {
  const uint32_t now = millis();

  // ---------------------------------------------------------------------------
  // Main pipeline: SENSOR -> THERMAL -> TREND -> CONTROL/NET/CLOUD -> UI
  // ---------------------------------------------------------------------------
  // The first three calls are the clean domain path. Infrastructure/application
  // ticks remain below so networking changes cannot silently modify sensor logic.
  // ---------------------------------------------------------------------------
  if (hp20::sensor::tick(now)) {
    hp20::thermal::update(hp20::sensor::state());

    const hp20::thermal::State& th = hp20::thermal::state();
    if (hp20::sensor::state().valid && isfinite(th.feelUi)) {
      hp20::trend::push(now, th.feelUi);
    }
  }

  serialTick(now);
  controlsTick(now);
  networkTick(now);
  cloudTick(now);
  syncPortalConnectionStatus(now);
  portalUxBuzzerTick();
  hp20::ota::tick(now, config, cloudReady && !inFlight && !portalActive());
  hp20::ui::tick(
    now,
    hp20::sensor::state(),
    cloudState,
    hp20::ota::label(),
    wifiUiState.c_str(),
    wifiAttemptSsid.c_str(),
    wifiProfileCursor,
    config.wifiProfileCount
  );

  if (model::elapsed(now, statusSince, settings::SERIAL_STATUS_MS)) {
    statusSince = now;

    const hp20::sensor::Reading& env = hp20::sensor::state();
    const hp20::thermal::State& th = hp20::thermal::state();

    Serial.printf(
      "STATUS up=%lus sensor=%s Traw=%.1f Tcal=%.1f RHraw=%.1f RHcal=%.1f HI=%.1f UI=%.1f band=%s green=%u%% trend=%s d10=%.1f wifi=%s wifiState=%s profiles=%u portal=%s page=%u cloud=%s ca=%s epoch=%lld ota=%s otaPct=%u wait>=%lus\n",
      (unsigned long)(now / 1000),
      hp20::sensor::fresh(now) ? "OK" : "INVALID",
      env.rawTempC,
      env.tempC,
      env.rawRh,
      env.rh,
      th.feelRaw,
      th.feelUi,
      hp20::thermal::label(th.band),
      unsigned(hp20::indicator::greenPresencePercent(th.band)),
      hp20::trend::label(),
      hp20::trend::delta(),
      WiFi.status() == WL_CONNECTED ? "OK" : "OFFLINE",
      wifiUiState.c_str(),
      unsigned(config.wifiProfileCount),
      portalActive() ? "OPEN" : "CLOSED",
      unsigned(hp20::ui::page() + 1),
      cloudState,
      hp20::tbtrust::caSourceLabel(config),
      (long long)time(nullptr),
      hp20::ota::label(),
      unsigned(hp20::ota::progressPercent()),
      (unsigned long)nextSendSeconds(now)
    );
  }

  delay(2);
}
