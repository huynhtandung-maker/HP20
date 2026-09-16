#include "hp20_provisioning.h"

#include <Arduino.h>

#if __has_include("secrets.h")
  #include "secrets.h"
  #define HP20_HAS_LOCAL_SECRETS 1
#else
  #define HP20_HAS_LOCAL_SECRETS 0
#endif

namespace hp20 { namespace provisioning {

bool localSecretsCompiled() {
  return HP20_HAS_LOCAL_SECRETS != 0;
}

bool seedFromLocalSecrets(Config& config) {
#if HP20_HAS_LOCAL_SECRETS
  const uint32_t revision = uint32_t(hp20_local::PROFILE_REVISION);
  if (revision == 0 || revision <= config.localProfileRevision) return false;

  String ssid = hp20_local::WIFI_SSID; ssid.trim();
  String host = hp20_local::TB_HOST; host.trim();
  String token = hp20_local::TB_TOKEN; token.trim();
  String ca = hp20_local::TB_CA_PEM; ca.trim();

  // An accidentally copied-but-empty secrets.h must NOT consume the revision.
  const bool hasIntent = !ssid.isEmpty() || !token.isEmpty() || !ca.isEmpty() ||
                         hp20_local::OTA_ENABLED;
  if (!hasIntent) return false;

  // Revisioned local profile = explicit developer intent. Apply non-empty
  // local fields once, persist the revision, then NVS/portal becomes
  // authoritative again until PROFILE_REVISION is incremented.
  if (!ssid.isEmpty()) {
    config.ssid = ssid;
    config.password = hp20_local::WIFI_PASSWORD;
    rememberWifiProfile(config, config.ssid, config.password, true);
  }
  if (!host.isEmpty()) config.host = host;
  if (!token.isEmpty()) config.token = token;
  if (!ca.isEmpty()) config.ca = ca;

  config.otaEnabled = hp20_local::OTA_ENABLED;
  config.otaCheckSeconds = constrain(
    uint32_t(hp20_local::OTA_CHECK_SECONDS), 3600U, 86400U
  );

  const uint32_t oldRevision = config.localProfileRevision;
  config.localProfileRevision = revision;

  if (!saveConfig(config)) {
    Serial.println("PROVISION local profile: failed to persist NVS");
    config.localProfileRevision = oldRevision;
    return false;
  }

  Serial.printf("PROVISION local profile: revision %lu applied to NVS once\n",
                (unsigned long)revision);
  return true;
#else
  (void)config;
  return false;
#endif
}
} } // namespace hp20::provisioning
