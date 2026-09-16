#pragma once

#include <Arduino.h>
#include "config.h"

namespace hp20 { namespace ota {

enum class State : uint8_t {
  Disabled,
  Waiting,
  Checking,
  UpToDate,
  UpdateAvailable,
  Downloading,
  Verifying,
  Applying,
  Restarting,
  Failed
};

// Human-feedback bridge. The OTA engine remains presentation-agnostic:
// the application may register a callback that renders OLED/buzzer feedback.
using UxCallback = void (*)(State state,
                            uint8_t progress,
                            const char* targetVersion,
                            const char* errorText);

void begin();

// Call from loop. safeToRun should be false while another cloud transaction is active.
void tick(uint32_t now, const Config& config, bool safeToRun);

// Forces the next tick to check immediately when safe.
void requestCheck();

// Register the application/UI callback used during blocking OTA work.
void setUxCallback(UxCallback callback);

bool busy();
State state();
const char* label();
uint8_t progressPercent();
const char* lastError();
const char* targetVersion();

} } // namespace hp20::ota
