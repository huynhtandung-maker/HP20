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
bool autoPortal = false;
bool wasConnected = false;
bool ignoreOldResult = false;
bool buzzing = false;
bool bootBeeping = false;

enum class SetupFeedback : uint8_t { Idle, Connecting, Connected, Failed };
SetupFeedback setupFeedback = SetupFeedback::Idle;
uint32_t setupFeedbackSince = 0;
uint32_t setupBeepMark = 0;
uint8_t setupBeepsRemaining = 0;
bool setupBeepOn = false;

uint32_t wifiAttempt = 0;
uint32_t offlineSince = 0;
uint32_t connectedSince = 0;
uint32_t beepSince = 0;
uint32_t lastBeep = 0;
uint32_t bootBeepSince = 0;
uint32_t sendMark = 0;
uint32_t waitSeconds = 300;
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

void buzzerOn(uint16_t frequency = 2200) {
  if (settings::PASSIVE_BUZZER) tone(settings::BUZZER_PIN, frequency);
  else digitalWrite(settings::BUZZER_PIN, HIGH);
}

void queueSetupBeeps(uint8_t count, uint32_t now) {
  if (!count || bootBeeping) return;
  if (buzzing) {
    buzzerOff();
    buzzing = false;
  }
  setupBeepsRemaining = count;
  setupBeepOn = true;
  setupBeepMark = now;
  buzzerOn(2350);
}

void setSetupFeedback(SetupFeedback state, uint32_t now) {
  if (setupFeedback == state) return;
  setupFeedback = state;
  setupFeedbackSince = now;

  if (state == SetupFeedback::Connecting) queueSetupBeeps(1, now);
  else if (state == SetupFeedback::Connected) queueSetupBeeps(2, now);
  else if (state == SetupFeedback::Failed) queueSetupBeeps(3, now);
}

void serviceSetupBeeps(uint32_t now) {
  if (bootBeeping || setupBeepsRemaining == 0) return;

  if (setupBeepOn) {
    if (model::elapsed(now, setupBeepMark, 95)) {
      buzzerOff();
      setupBeepOn = false;
      setupBeepMark = now;
      if (setupBeepsRemaining) --setupBeepsRemaining;
    }
  } else if (setupBeepsRemaining && model::elapsed(now, setupBeepMark, 110)) {
    setupBeepOn = true;
    setupBeepMark = now;
    buzzerOn(2350);
  }
}

void startBootBuzzer(uint32_t now) {
  if (!settings::BUZZER_BOOT_TEST || bootBeeping) return;
  bootBeeping = true;
  bootBeepSince = now;
  lastBeep = now;
  buzzerOn(2200);
  Serial.println("BUZZER boot test: ON");
}

void connectWifi() {
  if (config.ssid.isEmpty()) return;
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  wifiAttempt = millis();
}

void persistCooldown(uint32_t seconds) {
  notBefore = (uint64_t)time(nullptr) + seconds;
  if (runtime.putULong64("notBefore", notBefore) != sizeof(uint64_t)) {
    cloudReady = false;
    cloudState = "LOI LUU HAN GUI";
  }
}

void networkTick(uint32_t now) {
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !wasConnected) {
    connectedSince = now;
    autoPortal = false;
    if (setupFeedback == SetupFeedback::Connecting || setupFeedback == SetupFeedback::Failed) {
      setSetupFeedback(SetupFeedback::Connected, now);
      Serial.println("SETUP feedback: Wi-Fi connected");
    }
    configTime(0, 0, "time.cloudflare.com", "time.google.com", "pool.ntp.org");
    ntpRetrySince = now;
    Serial.println("TIME sync requested (NTP)");
  }

  if (!connected && wasConnected) offlineSince = now;
  wasConnected = connected;

  if (!connected && !config.ssid.isEmpty() &&
      model::elapsed(now, wifiAttempt, settings::WIFI_RETRY_MS)) {
    connectWifi();
  }

  // Security/UX rule: an open setup AP is never exposed automatically after
  // ordinary Wi-Fi loss. First boot with no credentials and physical BOOT hold
  // are the only automatic/user-authorized ways to open the portal.

  static bool closeOnConnect = false;
  portalTick();

  if (portalSaved()) {
    setSetupFeedback(SetupFeedback::Connecting, now);
    Serial.println("SETUP feedback: saved, connecting Wi-Fi");
    ignoreOldResult = inFlight;
    authBlocked = false;
    if (runtime.putBool("auth", false) != sizeof(bool)) {
      cloudReady = false;
      cloudState = "LOI LUU TRANG THAI";
    }

    failures = 0;

    // Explicit user save = onboarding/configuration intent. Allow one prompt
    // telemetry attempt after reconnect instead of making the user wait a full
    // telemetry interval. The normal anti-spam interval is restored before send.
    sendMark = now;
    waitSeconds = 5;
    notBefore = 0;
    runtime.putULong64("notBefore", 0);

    remind = false;
    reminder = model::Reminder();

    WiFi.disconnect();
    wasConnected = false;
    offlineSince = now;
    connectWifi();
    if (config.otaEnabled) hp20::ota::requestCheck();

    closeOnConnect = true;
    connected = false;
  }

  if (!connected && autoPortal && !portalActive()) {
    autoPortal = false;
    offlineSince = now;
  }

  if (closeOnConnect && connected && wasConnected &&
      model::elapsed(now, connectedSince, 30000)) {
    portalClose();
    closeOnConnect = false;
  }

  if (setupFeedback == SetupFeedback::Connecting && !connected &&
      model::elapsed(now, setupFeedbackSince, 20000)) {
    setSetupFeedback(SetupFeedback::Failed, now);
    Serial.println("SETUP feedback: Wi-Fi not connected yet");
  }
  if (setupFeedback == SetupFeedback::Connected &&
      model::elapsed(now, setupFeedbackSince, 2600)) {
    setupFeedback = SetupFeedback::Idle;
  }
  if (setupFeedback == SetupFeedback::Failed &&
      model::elapsed(now, setupFeedbackSince, 30000)) {
    setupFeedback = SetupFeedback::Idle;
  }
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
    if (model::elapsed(now, ntpRetrySince, 30000)) {
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

  // Existing user-selected reminder behavior remains unchanged.
  remind = reminder.update(
    now,
    hp20::sensor::fresh(now) ? th.feelRaw : NAN,
    config.reminder,
    config.threshold
  );

  // External GREEN LED = human thermal-comfort indicator.
  // More green presence means more comfort; less green means comfort is falling.
  // Blink timing is centralized in hp20_indicator.cpp so the orchestrator stays clean.
  const bool greenOn = hp20::indicator::greenLedOn(th.band, now);
  digitalWrite(settings::LED_PIN, greenOn ? HIGH : LOW);

  if (settings::BOARD_LED_ENABLED) {
    bool blue = false;

    if (now < 800) {
      blue = (now % 400) < 80;
    }
    else if (setupFeedback == SetupFeedback::Connecting) blue = (now % 400) < 200;
    else if (setupFeedback == SetupFeedback::Connected) blue = true;
    else if (setupFeedback == SetupFeedback::Failed) {
      const uint32_t p = now % 1500;
      blue = p < 100 || (p >= 220 && p < 320) || (p >= 440 && p < 540);
    }
    else if (!hp20::sensor::fresh(now)) {
      const uint32_t p = now % 3000;
      blue = p < 80 || (p >= 200 && p < 280) || (p >= 400 && p < 480);
    }
    else if (remind) blue = (now % 1000) < 150;
    else if (portalActive()) {
      const uint32_t p = now % 2000;
      blue = p < 80 || (p >= 200 && p < 280);
    }
    else if (WiFi.status() != WL_CONNECTED) blue = (now % 2000) < 80;
    else if (hp20::ota::busy()) blue = (now % 500) < 250;
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

  serviceSetupBeeps(now);

  if (buzzing && (!remind || !config.sound || model::elapsed(now, beepSince, 150))) {
    buzzerOff();
    buzzing = false;
  }

  if (remind && config.sound && !buzzing && !bootBeeping &&
      setupBeepsRemaining == 0 && !setupBeepOn &&
      model::elapsed(now, lastBeep, 300000)) {
    lastBeep = beepSince = now;
    buzzing = true;

    buzzerOn(2200);
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
              "SETUP AP=%s OPEN URL=http://192.168.4.1\n",
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

  connectWifi();

  if (config.ssid.isEmpty()) {
    portalBegin(&config);
    autoPortal = true;
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
  hp20::ota::tick(now, config, cloudReady && !inFlight && !portalActive());
  hp20::ui::tick(now, hp20::sensor::state(), cloudState, hp20::ota::label());

  if (model::elapsed(now, statusSince, settings::SERIAL_STATUS_MS)) {
    statusSince = now;

    const hp20::sensor::Reading& env = hp20::sensor::state();
    const hp20::thermal::State& th = hp20::thermal::state();

    Serial.printf(
      "STATUS up=%lus sensor=%s Traw=%.1f Tcal=%.1f RHraw=%.1f RHcal=%.1f HI=%.1f UI=%.1f band=%s green=%u%% trend=%s d10=%.1f wifi=%s portal=%s page=%u cloud=%s ca=%s epoch=%lld ota=%s otaPct=%u wait>=%lus\n",
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
