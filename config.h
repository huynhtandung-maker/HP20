#pragma once
#include <Arduino.h>

// =============================================================================
// HP20 PERSISTED CONFIGURATION
// =============================================================================
// Wi-Fi design:
//   - ssid/password remain as the "last good / preferred" pair for backward
//     compatibility with older firmware and local provisioning.
//   - wifiProfiles stores up to MAX_WIFI_PROFILES remembered networks in NVS.
//   - passwords never belong in Git; they are persisted only in device NVS or
//     an ignored local secrets.h.
// =============================================================================

constexpr uint8_t MAX_WIFI_PROFILES = 5;

struct WifiProfile {
  String ssid;
  String password;
};

struct Config {
  // Connectivity / cloud
  String ssid;
  String password;
  WifiProfile wifiProfiles[MAX_WIFI_PROFILES];
  uint8_t wifiProfileCount = 0;

  String host = "thingsboard.cloud";
  String token;
  String ca;

  // Telemetry / reminder
  uint32_t intervalSeconds = 300; // Standard HP20 cadence: 5 minutes.
  bool reminder = false;
  bool sound = false;
  float threshold = 35; // Inactive until explicitly enabled by the user.

  // OTA policy. Remote OTA is OFF until the user/developer enables it.
  uint32_t otaCheckSeconds = 21600; // 6 h; polling is intentionally infrequent.
  bool otaEnabled = false;

  // Developer-local provisioning profile revision already applied to NVS.
  // Portal/NVS wins until secrets.h intentionally increments PROFILE_REVISION.
  uint32_t localProfileRevision = 0;
};

bool loadConfig(Config& c);
bool saveConfig(const Config& c);

// Wi-Fi profile helpers. Index 0 is always the preferred / most recently
// successful network. Existing v0.9.14 ssid/password is migrated automatically.
int wifiProfileIndex(const Config& c, const String& ssid);
String wifiPasswordFor(const Config& c, const String& ssid);
bool rememberWifiProfile(Config& c, const String& ssid, const String& password,
                         bool makePreferred = true);
bool forgetWifiProfile(Config& c, const String& ssid);
void clearWifiProfiles(Config& c);
void normalizeWifiProfiles(Config& c);
