#pragma once

#include <cstdint>

// =============================================================================
// HP20 FIRMWARE VERSION - SINGLE SOURCE OF TRUTH
// =============================================================================
// Change version ONLY here when preparing a new firmware release.
//
// Semantic Versioning used by HP20:
//   MAJOR.MINOR.PATCH
//   MAJOR = incompatible product/config architecture change.
//   MINOR = new backward-compatible capability or subsystem.
//   PATCH = bug fix, UX refinement, documentation/repository cleanup.
//
// Git tag should match this value exactly with a leading "v".
// =============================================================================
namespace hp20 { namespace version {

constexpr uint8_t MAJOR = 0;
constexpr uint8_t MINOR = 9;
constexpr uint8_t PATCH = 6;

constexpr const char* TITLE = "HP20";   // ThingsBoard OTA package title must match.
constexpr const char* STRING = "0.9.6";
constexpr const char* TAG = "v0.9.6";

} }  // namespace hp20::version
