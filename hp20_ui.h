#pragma once

#include <Arduino.h>
#include "hp20_sensor.h"
#include "hp20_ota.h"

// =============================================================================
// COMPONENT - PRESENTATION / OLED UI
// =============================================================================
// Responsibility:
//   Render the already-computed sensor/domain state on the 128x64 OLED.
//
// RULE:
//   UI may display thermal/trend/OTA state but must NOT own thermal thresholds,
//   OTA transport, firmware integrity logic, or Heat Index calculations.
// =============================================================================
namespace hp20 { namespace ui {

constexpr uint8_t PAGE_COUNT = 7;
constexpr uint8_t SETUP_PAGE = 6;

void begin();
void tick(uint32_t now,
          const sensor::Reading& env,
          const char* cloudState,
          const char* otaState,
          const char* wifiState,
          const char* wifiSsid,
          uint8_t wifiProfileIndex,
          uint8_t wifiProfileCount);

void nextPage(uint32_t now);
void showSetupPage(uint32_t now);
uint8_t page();

// Full-screen OTA human-feedback overlay.
// This function also renders immediately so progress remains visible while the
// OTA downloader is blocking the normal application loop.
void showOtaState(uint32_t now,
                  ota::State state,
                  uint8_t progress,
                  const char* targetVersion,
                  const char* errorText);

} }  // namespace hp20::ui
