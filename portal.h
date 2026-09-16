#pragma once
#include <Arduino.h>
#include "config.h"

// =============================================================================
// HP20 CAPTIVE SETUP PORTAL
// =============================================================================
// Product rule:
//   - Setup AP is intentionally open (no throw-away password).
//   - It is exposed only on first provisioning or an explicit local BOOT hold.
//   - Normal transient Wi-Fi loss must NOT silently expose an open setup AP.
// =============================================================================

enum class PortalPhase : uint8_t {
  Idle,
  Ready,
  ScanningNetworks,
  Saved,
  ConnectingWifi,
  WifiConnected,
  SyncingTime,
  ConnectingCloud,
  Success,
  Failed
};

void portalBegin(Config* config);
bool portalActive();
bool portalSaved();
void portalTick();
void portalClose();
void portalCancelScan();

String portalName();
String portalPassword(); // Kept for compatibility; v0.9.15 returns empty.
IPAddress portalIp();

PortalPhase portalPhase();
uint8_t portalProgress();
String portalStatusTitle();
String portalStatusDetail();
String portalStatusSsid();
String portalStatusIp();

void portalSetStatus(PortalPhase phase,
                     uint8_t progress,
                     const String& title,
                     const String& detail = String(),
                     const String& ssid = String(),
                     const String& ip = String());
