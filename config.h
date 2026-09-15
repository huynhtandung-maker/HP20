#pragma once
#include <Arduino.h>

struct Config {
  // Connectivity / cloud
  String ssid;
  String password;
  String host = "thingsboard.cloud";
  String token;
  String ca;

  // Telemetry / reminder
  uint32_t intervalSeconds = 300; // Standard HP20 cadence: 5 minutes.
  bool reminder = false;
  bool sound = false;
  float threshold = 35; // Inactive until explicitly enabled by the user.

  // OTA policy. Remote OTA is OFF until the user/developer enables it.
  bool otaEnabled = false;
  uint32_t otaCheckSeconds = 21600; // 6 h; polling is intentionally infrequent.

  // Developer-local provisioning profile revision already applied to NVS.
  // Portal/NVS wins until secrets.h intentionally increments PROFILE_REVISION.
  uint32_t localProfileRevision = 0;
};

bool loadConfig(Config& c);
bool saveConfig(const Config& c);
