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
  c.intervalSeconds = constrain(d["interval"] | 300U, 300U, 86400U);
  c.reminder = d["reminder"] | false;
  c.sound = d["sound"] | false;
  c.threshold = constrain(d["threshold"] | 35.0f, 27.0f, 60.0f);

  // Backward-compatible migration: old configs simply get safe OTA defaults.
  c.otaEnabled = d["ota_enabled"] | false;
  c.otaCheckSeconds = constrain(d["ota_check"] | 21600U, 3600U, 86400U);
  c.localProfileRevision = d["local_revision"] | 0U;
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
