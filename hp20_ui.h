#pragma once

#include <Arduino.h>
#include "hp20_sensor.h"

// =============================================================================
// COMPONENT - PRESENTATION / OLED UI
// =============================================================================
// Responsibility:
//   Render the already-computed sensor/domain state on the 128x64 OLED.
//
// RULE:
//   UI may display thermal/trend results but must NOT create new thresholds or
//   calculate its own Heat Index. This keeps domain logic out of presentation.
// =============================================================================
namespace hp20 { namespace ui {

constexpr uint8_t PAGE_COUNT = 7;
constexpr uint8_t SETUP_PAGE = 6;

void begin();
void tick(uint32_t now, const sensor::Reading& env, const char* cloudState);

void nextPage(uint32_t now);
void showSetupPage(uint32_t now);
uint8_t page();

} }  // namespace hp20::ui
