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
  Failed
};

void begin();

// Call from loop. safeToRun should be false while another cloud transaction is active.
void tick(uint32_t now, const Config& config, bool safeToRun);

// Forces the next tick to check immediately when safe. Useful from Serial/portal later.
void requestCheck();

bool busy();
State state();
const char* label();
uint8_t progressPercent();
const char* lastError();

} } // namespace hp20::ota
