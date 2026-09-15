#pragma once

#include "config.h"

namespace hp20 { namespace provisioning {

// Seeds NVS from an optional local secrets.h ONLY when no Wi-Fi config exists.
// Returns true when a local configuration was successfully persisted.
bool seedFromLocalSecrets(Config& config);

// Compile-time diagnostic: true when secrets.h was available to this build.
bool localSecretsCompiled();

} } // namespace hp20::provisioning
