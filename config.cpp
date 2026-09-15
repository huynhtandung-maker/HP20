#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>

bool loadConfig(Config& c) {
  Preferences p;
  if (!p.begin("room-config", true)) return false;
  String raw = p.getString("json", "");
  p.end();
  if (raw.isEmpty()) return false;

  DynamicJsonDocument d(11000);
  if (deserializeJson(d, raw)) return false;

  c.ssid = d["ssid"].as<String>();
  c.password = d["password"].as<String>();
  c.host = d["host"] | "thingsboard.cloud";
  c.token = d["token"].as<String>();
  c.ca = d["ca"].as<String>();

  // v0.9.6 cadence migration: older HP20 builds used 15 minutes as the
  // default/minimum. The product standard is now 5 minutes. Migrate the old
  // 15-minute baseline once, then persist a marker so a user may later choose
  // 15 minutes intentionally without it being changed again on reboot.
  const uint32_t storedInterval = d["interval"] | 300U;
  const uint32_t cadenceVersion = d["cadence_v"] | 0U;
  const bool migrateCadence = cadenceVersion < 1U;
  const uint32_t effectiveInterval =
    (migrateCadence && storedInterval == 900U) ? 300U : storedInterval;
  c.intervalSeconds = constrain(effectiveInterval, 300U, 86400U);
  c.reminder = d["reminder"] | false;
  c.sound = d["sound"] | false;
  c.threshold = constrain(d["threshold"] | 35.0f, 27.0f, 60.0f);

  // Backward-compatible migration: old configs simply get safe OTA defaults.
  c.otaEnabled = d["ota_enabled"] | false;
  c.otaCheckSeconds = constrain(d["ota_check"] | 3600U, 900U, 86400U);
  c.localProfileRevision = d["local_revision"] | 0U;

  if (migrateCadence) {
    if (!saveConfig(c)) {
      // Runtime config is still valid even if the migration marker cannot be
      // persisted this boot; do not make boot depend on a maintenance write.
    }
  }
  return true;
}

bool saveConfig(const Config& c) {
  DynamicJsonDocument d(11000);
  d["ssid"] = c.ssid;
  d["password"] = c.password;
  d["host"] = c.host;
  d["token"] = c.token;
  d["ca"] = c.ca;
  d["interval"] = c.intervalSeconds;
  d["cadence_v"] = 1U;
  d["reminder"] = c.reminder;
  d["sound"] = c.sound;
  d["threshold"] = c.threshold;
  d["ota_enabled"] = c.otaEnabled;
  d["ota_check"] = c.otaCheckSeconds;
  d["local_revision"] = c.localProfileRevision;

  if (d.overflowed()) return false;
  String raw;
  serializeJson(d, raw);

  Preferences p;
  if (!p.begin("room-config", false)) return false;
  const bool ok = p.putString("json", raw) == raw.length();
  p.end();
  return ok;
}
