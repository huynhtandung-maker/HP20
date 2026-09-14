#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
bool loadConfig(Config& c) {
  Preferences p;
  if (!p.begin("room-config", true)) return false;
  String raw = p.getString("json", ""); p.end();
  DynamicJsonDocument d(10000);
  if (deserializeJson(d, raw)) return false;
  c.ssid = d["ssid"].as<String>(); c.password = d["password"].as<String>();
  c.host = d["host"].as<String>(); c.token = d["token"].as<String>(); c.ca = d["ca"].as<String>();
  c.intervalSeconds = constrain(d["interval"] | 900U, 900U, 86400U);
  c.reminder = d["reminder"] | false; c.sound = d["sound"] | false;
  c.threshold = constrain(d["threshold"] | 35.0f, 27.0f, 60.0f);
  return true;
}
bool saveConfig(const Config& c) {
  DynamicJsonDocument d(10000);
  d["ssid"] = c.ssid; d["password"] = c.password; d["host"] = c.host;
  d["token"] = c.token; d["ca"] = c.ca; d["interval"] = c.intervalSeconds;
  d["reminder"] = c.reminder; d["sound"] = c.sound; d["threshold"] = c.threshold;
  if (d.overflowed()) return false;
  String raw; serializeJson(d, raw);
  Preferences p; if (!p.begin("room-config", false)) return false;
  bool ok = p.putString("json", raw) == raw.length(); p.end(); return ok;
}
