#pragma once
#include <cstdint>

namespace settings {
constexpr uint8_t DHT_PIN = 27, SDA_PIN = 21, SCL_PIN = 22;
constexpr uint8_t LED_PIN = 25, BUZZER_PIN = 26, BUTTON_PIN = 0;

// Onboard blue LED: GPIO2 is a provisional mapping; verify on your board.
constexpr bool BOARD_LED_ENABLED = true, BOARD_LED_ACTIVE_HIGH = true;
constexpr uint8_t BOARD_LED_PIN = 2;

constexpr uint32_t SERIAL_STATUS_MS = 10000;

constexpr bool PASSIVE_BUZZER = false; // true only for a passive piezo buzzer
constexpr bool BUZZER_BOOT_TEST = true;
constexpr uint32_t BUZZER_BOOT_TEST_MS = 120;

constexpr uint32_t SAMPLE_MS = 2500, STALE_MS = 10000;
constexpr uint32_t PORTAL_TIMEOUT_MS = 600000, WIFI_RETRY_MS = 30000;
constexpr uint32_t MIN_SEND_SECONDS = 300; // HP20 standard: 5 minutes; no catch-up
}
