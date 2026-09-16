#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>

namespace {

String cleanSsid(String value) {
  value.trim();
  if (value.length() > 32) value.remove(32);
  return value;
}

void syncLegacyPreferred(Config& c) {
  if (c.wifiProfileCount > 0) {
    c.ssid = c.wifiProfiles[0].ssid;
    c.password = c.wifiProfiles[0].password;
  } else {
    c.ssid = "";
    c.password = "";
  }
}

} // namespace

int wifiProfileIndex(const Config& c, const String& ssid) {
  if (ssid.isEmpty()) return -1;
  for (uint8_t i = 0; i < c.wifiProfileCount && i < MAX_WIFI_PROFILES; ++i) {
    if (c.wifiProfiles[i].ssid == ssid) return int(i);
  }
  return -1;
}

String wifiPasswordFor(const Config& c, const String& ssid) {
  const int idx = wifiProfileIndex(c, ssid);
  return idx >= 0 ? c.wifiProfiles[idx].password : String();
}

bool rememberWifiProfile(Config& c, const String& rawSsid,
                         const String& password, bool makePreferred) {
  const String ssid = cleanSsid(rawSsid);
  if (ssid.isEmpty()) return false;

  int idx = wifiProfileIndex(c, ssid);
  bool changed = false;

  if (idx >= 0) {
    if (c.wifiProfiles[idx].password != password) {
      c.wifiProfiles[idx].password = password;
      changed = true;
    }

    if (makePreferred && idx > 0) {
      const WifiProfile selected = c.wifiProfiles[idx];
      for (int i = idx; i > 0; --i) c.wifiProfiles[i] = c.wifiProfiles[i - 1];
      c.wifiProfiles[0] = selected;
      changed = true;
    }
  } else {
    WifiProfile selected;
    selected.ssid = ssid;
    selected.password = password;

    if (c.wifiProfileCount < MAX_WIFI_PROFILES) {
      ++c.wifiProfileCount;
    }

    if (makePreferred) {
      for (int i = int(c.wifiProfileCount) - 1; i > 0; --i)
        c.wifiProfiles[i] = c.wifiProfiles[i - 1];
      c.wifiProfiles[0] = selected;
    } else {
      c.wifiProfiles[c.wifiProfileCount - 1] = selected;
    }
    changed = true;
  }

  if (makePreferred) {
    if (c.ssid != ssid || c.password != password) changed = true;
    c.ssid = ssid;
    c.password = password;
  } else if (c.wifiProfileCount > 0 && c.ssid.isEmpty()) {
    syncLegacyPreferred(c);
    changed = true;
  }

  return changed;
}

bool forgetWifiProfile(Config& c, const String& ssid) {
  const int idx = wifiProfileIndex(c, ssid);
  if (idx < 0) return false;

  for (uint8_t i = uint8_t(idx); i + 1 < c.wifiProfileCount; ++i)
    c.wifiProfiles[i] = c.wifiProfiles[i + 1];

  if (c.wifiProfileCount > 0) {
    --c.wifiProfileCount;
    c.wifiProfiles[c.wifiProfileCount] = WifiProfile();
  }
  syncLegacyPreferred(c);
  return true;
}

void clearWifiProfiles(Config& c) {
  for (uint8_t i = 0; i < MAX_WIFI_PROFILES; ++i)
    c.wifiProfiles[i] = WifiProfile();
  c.wifiProfileCount = 0;
  c.ssid = "";
  c.password = "";
}

void normalizeWifiProfiles(Config& c) {
  if (c.wifiProfileCount > MAX_WIFI_PROFILES)
    c.wifiProfileCount = MAX_WIFI_PROFILES;

  // Drop blank and duplicate rows while preserving order.
  WifiProfile cleaned[MAX_WIFI_PROFILES];
  uint8_t used = 0;
  for (uint8_t i = 0; i < c.wifiProfileCount && used < MAX_WIFI_PROFILES; ++i) {
    String ssid = cleanSsid(c.wifiProfiles[i].ssid);
    if (ssid.isEmpty()) continue;

    bool duplicate = false;
    for (uint8_t j = 0; j < used; ++j) {
      if (cleaned[j].ssid == ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    cleaned[used].ssid = ssid;
    cleaned[used].password = c.wifiProfiles[i].password;
    ++used;
  }

  for (uint8_t i = 0; i < MAX_WIFI_PROFILES; ++i)
    c.wifiProfiles[i] = i < used ? cleaned[i] : WifiProfile();
  c.wifiProfileCount = used;

  // Backward migration from v0.9.14 and older single-network config.
  String legacySsid = cleanSsid(c.ssid);
  if (!legacySsid.isEmpty() && wifiProfileIndex(c, legacySsid) < 0) {
    rememberWifiProfile(c, legacySsid, c.password, true);
  } else if (c.wifiProfileCount > 0) {
    // If the legacy preferred row exists, move it to the front.
    const int idx = wifiProfileIndex(c, legacySsid);
    if (idx > 0) {
      const WifiProfile selected = c.wifiProfiles[idx];
      for (int i = idx; i > 0; --i) c.wifiProfiles[i] = c.wifiProfiles[i - 1];
      c.wifiProfiles[0] = selected;
    }
    syncLegacyPreferred(c);
  }
}

bool loadConfig(Config& c) {
  Preferences p;
  if (!p.begin("room-config", true)) return false;
  String raw = p.getString("json", "");
  p.end();
  if (raw.isEmpty()) return false;

  DynamicJsonDocument d(13000);
  if (deserializeJson(d, raw)) return false;

  c.ssid = d["ssid"].as<String>();
  c.password = d["password"].as<String>();

  c.wifiProfileCount = 0;
  JsonArrayConst profiles = d["wifi_profiles"].as<JsonArrayConst>();
  if (!profiles.isNull()) {
    for (JsonObjectConst row : profiles) {
      if (c.wifiProfileCount >= MAX_WIFI_PROFILES) break;
      String ssid = row["ssid"].as<String>();
      String pass = row["password"].as<String>();
      ssid.trim();
      if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 63) continue;
      c.wifiProfiles[c.wifiProfileCount].ssid = ssid;
      c.wifiProfiles[c.wifiProfileCount].password = pass;
      ++c.wifiProfileCount;
    }
  }

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

  normalizeWifiProfiles(c);
  return true;
}

bool saveConfig(const Config& c) {
  DynamicJsonDocument d(13000);
  d["ssid"] = c.ssid;
  d["password"] = c.password;

  JsonArray profiles = d["wifi_profiles"].to<JsonArray>();
  for (uint8_t i = 0; i < c.wifiProfileCount && i < MAX_WIFI_PROFILES; ++i) {
    if (c.wifiProfiles[i].ssid.isEmpty()) continue;
    JsonObject row = profiles.add<JsonObject>();
    row["ssid"] = c.wifiProfiles[i].ssid;
    row["password"] = c.wifiProfiles[i].password;
  }

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
