#pragma once
#include <Arduino.h>
struct Config {
  String ssid, password, host, token, ca;
  uint32_t intervalSeconds = 900;
  bool reminder = false, sound = false;
  float threshold = 35; // Inactive until explicitly enabled by the user.
};
bool loadConfig(Config& c);
bool saveConfig(const Config& c);
