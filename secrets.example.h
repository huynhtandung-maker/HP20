#pragma once

// =============================================================================
// HP20 LOCAL DEVELOPMENT SECRETS - TEMPLATE ONLY
// =============================================================================
// 1) Copy this file to: secrets.h
// 2) Fill the real values ONLY in secrets.h
// 3) Never commit secrets.h. The repository .gitignore already blocks it.
//
// Local secrets are used ONLY when NVS has no Wi-Fi configuration yet.
// Once the user changes Wi-Fi/token from the setup portal, NVS wins on reboot.
// =============================================================================
namespace hp20_local {

// Increase this number ONLY when you intentionally want this local profile to
// overwrite the currently saved NVS network/token once on the next boot.
constexpr uint32_t PROFILE_REVISION = 1;

constexpr const char* WIFI_SSID = "";
constexpr const char* WIFI_PASSWORD = "";

constexpr const char* TB_HOST = "thingsboard.cloud";
constexpr const char* TB_TOKEN = "";

// ThingsBoard HTTPS trust anchor in PEM form.
// Keep empty if you prefer to enter/replace it from the Advanced setup section.
constexpr const char* TB_CA_PEM = R"PEM(
)PEM";

// Remote ThingsBoard OTA is opt-in.
constexpr bool OTA_ENABLED = false;
constexpr uint32_t OTA_CHECK_SECONDS = 21600; // 6 hours

} // namespace hp20_local
