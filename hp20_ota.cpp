#include "hp20_ota.h"
#include "thingsboard_ca.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
#include <ctype.h>

#include "hp20_version.h"
#include "model.h"

namespace hp20 { namespace ota {
namespace {

// -----------------------------------------------------------------------------
// mbedTLS SHA-256 compatibility layer
// -----------------------------------------------------------------------------
// Arduino-ESP32 3.x ships mbedTLS 3.x, where the public SHA-256 API uses
// mbedtls_sha256_starts/update/finish (without the old _ret suffix).
// Older Arduino-ESP32 / mbedTLS 2.x builds may still expose the _ret names.
// Keep this compatibility in ONE place so the OTA logic below stays portable.
#if defined(MBEDTLS_VERSION_MAJOR) && (MBEDTLS_VERSION_MAJOR >= 3)
  #define HP20_SHA256_STARTS(ctx, is224) mbedtls_sha256_starts((ctx), (is224))
  #define HP20_SHA256_UPDATE(ctx, input, len) mbedtls_sha256_update((ctx), (input), (len))
  #define HP20_SHA256_FINISH(ctx, output) mbedtls_sha256_finish((ctx), (output))
#else
  #define HP20_SHA256_STARTS(ctx, is224) mbedtls_sha256_starts_ret((ctx), (is224))
  #define HP20_SHA256_UPDATE(ctx, input, len) mbedtls_sha256_update_ret((ctx), (input), (len))
  #define HP20_SHA256_FINISH(ctx, output) mbedtls_sha256_finish_ret((ctx), (output))
#endif

constexpr uint32_t FIRST_CHECK_DELAY_MS = 90000UL;
constexpr uint32_t HTTP_TIMEOUT_MS = 12000UL;
constexpr size_t DOWNLOAD_BUFFER = 2048;
constexpr size_t MAX_FIRMWARE_BYTES = 4UL * 1024UL * 1024UL;

State currentState = State::Waiting;
uint32_t lastCheckAt = 0;
bool forceCheck = false;
bool announced = false;
bool firstCheckDone = false;
uint8_t progress = 0;
String errorText;

struct Target {
  String title;
  String version;
  String checksum;
  String algorithm;
  size_t size = 0;
};

String urlEncode(const String& value) {
  String out;
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = uint8_t(value[i]);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += char(c);
    } else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

bool validHexSha256(const String& value) {
  if (value.length() != 64) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isxdigit((unsigned char)value[i])) return false;
  }
  return true;
}

bool parseVersion(const String& input, int& major, int& minor, int& patch) {
  String s = input;
  s.trim();
  if (s.startsWith("v") || s.startsWith("V")) s.remove(0, 1);

  int p1 = s.indexOf('.');
  int p2 = p1 >= 0 ? s.indexOf('.', p1 + 1) : -1;
  if (p1 <= 0 || p2 <= p1 + 1) return false;

  String a = s.substring(0, p1);
  String b = s.substring(p1 + 1, p2);
  String c = s.substring(p2 + 1);
  int suffix = c.indexOf('-');
  if (suffix >= 0) c = c.substring(0, suffix);
  suffix = c.indexOf('+');
  if (suffix >= 0) c = c.substring(0, suffix);
  if (a.isEmpty() || b.isEmpty() || c.isEmpty()) return false;

  for (size_t i = 0; i < a.length(); ++i) if (!isdigit((unsigned char)a[i])) return false;
  for (size_t i = 0; i < b.length(); ++i) if (!isdigit((unsigned char)b[i])) return false;
  for (size_t i = 0; i < c.length(); ++i) if (!isdigit((unsigned char)c[i])) return false;

  major = a.toInt();
  minor = b.toInt();
  patch = c.toInt();
  return true;
}

bool newerThanCurrent(const String& candidate) {
  int a = 0, b = 0, c = 0;
  if (!parseVersion(candidate, a, b, c)) return false;
  if (a != hp20::version::MAJOR) return a > hp20::version::MAJOR;
  if (b != hp20::version::MINOR) return b > hp20::version::MINOR;
  return c > hp20::version::PATCH;
}

void setError(const String& message) {
  errorText = message;
  currentState = State::Failed;
  Serial.printf("OTA error: %s\n", errorText.c_str());
}

bool openSecure(HTTPClient& http, WiFiClientSecure& client,
                const Config& config, const String& url) {
  const char* ca = hp20::tbtrust::effectiveCa(config);
  if (!ca) {
    setError("THIEU CA TLS");
    return false;
  }
  client.setCACert(ca);
  client.setHandshakeTimeout(8);
  http.setConnectTimeout(7000);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) {
    setError("KHONG MO HTTPS");
    return false;
  }
  return true;
}

bool postAttributes(const Config& config, const String& json) {
  if (WiFi.status() != WL_CONNECTED || config.host.isEmpty() ||
      config.token.isEmpty() || !hp20::tbtrust::hasEffectiveCa(config)) return false;

  WiFiClientSecure client;
  HTTPClient http;
  const String url = "https://" + config.host + "/api/v1/" +
                     config.token + "/attributes";
  if (!openSecure(http, client, config, url)) return false;
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(json);
  http.end();
  return code >= 200 && code < 300;
}

void reportState(const Config& config, const char* fwState, const String& error = "") {
  StaticJsonDocument<256> doc;
  doc["current_fw_title"] = hp20::version::TITLE;
  doc["current_fw_version"] = hp20::version::STRING;
  doc["fw_state"] = fwState;
  if (!error.isEmpty()) doc["fw_error"] = error;
  String body;
  serializeJson(doc, body);
  postAttributes(config, body);
}

bool fetchTarget(const Config& config, Target& target) {
  WiFiClientSecure client;
  HTTPClient http;
  String url = "https://" + config.host + "/api/v1/" + config.token +
               "/attributes?sharedKeys=fw_title,fw_version,fw_checksum,"
               "fw_checksum_algorithm,fw_size";
  if (!openSecure(http, client, config, url)) return false;

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("CHECK HTTP ") + code);
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(1536);
  if (deserializeJson(doc, body)) {
    setError("CHECK JSON");
    return false;
  }

  JsonObject shared = doc["shared"].as<JsonObject>();
  if (shared.isNull()) {
    // No assigned firmware is not an error.
    target = Target{};
    return true;
  }

  target.title = shared["fw_title"] | "";
  target.version = shared["fw_version"] | "";
  target.checksum = shared["fw_checksum"] | "";
  target.algorithm = shared["fw_checksum_algorithm"] | "";
  target.size = shared["fw_size"] | 0UL;
  return true;
}

String sha256Hex(const uint8_t hash[32]) {
  static const char* hex = "0123456789abcdef";
  char out[65];
  for (int i = 0; i < 32; ++i) {
    out[i * 2] = hex[(hash[i] >> 4) & 0x0F];
    out[i * 2 + 1] = hex[hash[i] & 0x0F];
  }
  out[64] = '\0';
  return String(out);
}

bool downloadAndApply(const Config& config, const Target& target) {
  if (target.title != hp20::version::TITLE) {
    setError("SAI FW TITLE");
    return false;
  }
  if (!newerThanCurrent(target.version)) {
    currentState = State::UpToDate;
    return true;
  }
  if (!target.algorithm.equalsIgnoreCase("SHA256") || !validHexSha256(target.checksum)) {
    setError("CAN SHA256");
    return false;
  }
  if (target.size < 65536 || target.size > MAX_FIRMWARE_BYTES) {
    setError("FW SIZE SAI");
    return false;
  }

  currentState = State::UpdateAvailable;
  reportState(config, "DOWNLOADING");
  currentState = State::Downloading;
  progress = 0;

  WiFiClientSecure client;
  HTTPClient http;
  const String url = "https://" + config.host + "/api/v1/" + config.token +
                     "/firmware?title=" + urlEncode(target.title) +
                     "&version=" + urlEncode(target.version);
  if (!openSecure(http, client, config, url)) return false;

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("FW HTTP ") + code);
    reportState(config, "FAILED", errorText);
    return false;
  }

  if (!Update.begin(target.size, U_FLASH)) {
    http.end();
    setError(String("UPDATE BEGIN: ") + Update.errorString());
    reportState(config, "FAILED", errorText);
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  if (HP20_SHA256_STARTS(&sha, 0) != 0) {
    mbedtls_sha256_free(&sha);
    http.end();
    Update.abort();
    setError("SHA256 START");
    reportState(config, "FAILED", errorText);
    return false;
  }

  auto* stream = http.getStreamPtr();
  uint8_t buffer[DOWNLOAD_BUFFER];
  size_t received = 0;
  uint32_t lastDataAt = millis();
  bool ok = true;

  while (received < target.size) {
    const int available = stream->available();
    if (available > 0) {
      size_t want = size_t(available);
      if (want > sizeof(buffer)) want = sizeof(buffer);
      if (want > target.size - received) want = target.size - received;
      const int got = stream->readBytes(buffer, want);
      if (got <= 0) continue;

      if (Update.write(buffer, size_t(got)) != size_t(got)) {
        setError(String("UPDATE WRITE: ") + Update.errorString());
        ok = false;
        break;
      }
      if (HP20_SHA256_UPDATE(&sha, buffer, size_t(got)) != 0) {
        setError("SHA256 UPDATE");
        ok = false;
        break;
      }
      received += size_t(got);
      lastDataAt = millis();
            size_t percent = (received * 100U) / target.size;
      if (percent > 100U) percent = 100U;
      progress = uint8_t(percent);
      delay(1);
    } else {
      if (!http.connected() && received < target.size) {
        setError("FW STREAM DUT");
        ok = false;
        break;
      }
      if (model::elapsed(millis(), lastDataAt, HTTP_TIMEOUT_MS)) {
        setError("FW TIMEOUT");
        ok = false;
        break;
      }
      delay(2);
    }
  }

  uint8_t digest[32] = {0};
  if (HP20_SHA256_FINISH(&sha, digest) != 0) {
    mbedtls_sha256_free(&sha);
    http.end();
    Update.abort();
    setError("SHA256 FINISH");
    reportState(config, "FAILED", errorText);
    return false;
  }
  mbedtls_sha256_free(&sha);
  http.end();

  if (!ok || received != target.size) {
    Update.abort();
    reportState(config, "FAILED", errorText);
    return false;
  }

  // ThingsBoard OTA state machine: binary transfer finished, checksum next.
  reportState(config, "DOWNLOADED");
  currentState = State::Verifying;
  const String calculated = sha256Hex(digest);
  String expected = target.checksum;
  expected.toLowerCase();
  if (calculated != expected) {
    Update.abort();
    setError("SHA256 KHONG KHOP");
    reportState(config, "FAILED", errorText);
    return false;
  }

  reportState(config, "VERIFIED");
  currentState = State::Applying;
  if (!Update.end(false)) {
    setError(String("UPDATE END: ") + Update.errorString());
    reportState(config, "FAILED", errorText);
    return false;
  }

  reportState(config, "UPDATING");
  Serial.printf("OTA verified: %s -> %s. Rebooting...\n",
                hp20::version::STRING, target.version.c_str());
  delay(800);
  ESP.restart();
  return true;
}

} // namespace

void begin() {
  currentState = State::Waiting;
  lastCheckAt = millis();
  forceCheck = false;
  announced = false;
  firstCheckDone = false;
  progress = 0;
  errorText = "";
}

void requestCheck() {
  forceCheck = true;
}

bool busy() {
  return currentState == State::Checking ||
         currentState == State::Downloading ||
         currentState == State::Verifying ||
         currentState == State::Applying;
}

State state() { return currentState; }
uint8_t progressPercent() { return progress; }
const char* lastError() { return errorText.c_str(); }

const char* label() {
  switch (currentState) {
    case State::Disabled:        return "OTA: TAT";
    case State::Waiting:         return "OTA: SAN SANG";
    case State::Checking:        return "OTA: DANG KIEM TRA";
    case State::UpToDate:        return "OTA: MOI NHAT";
    case State::UpdateAvailable: return "OTA: CO BAN MOI";
    case State::Downloading:     return "OTA: DANG TAI";
    case State::Verifying:       return "OTA: KIEM TRA SHA";
    case State::Applying:        return "OTA: DANG CAP NHAT";
    case State::Failed:          return "OTA: LOI";
  }
  return "OTA: ?";
}

void tick(uint32_t now, const Config& config, bool safeToRun) {
  if (!config.otaEnabled) {
    currentState = State::Disabled;
    return;
  }
  if (!safeToRun || busy()) return;
  if (WiFi.status() != WL_CONNECTED || config.host.isEmpty() ||
      config.token.isEmpty() || !hp20::tbtrust::hasEffectiveCa(config)) {
    currentState = State::Waiting;
    return;
  }

  // Publish firmware identity once per boot so ThingsBoard knows the active version.
  if (!announced) {
    StaticJsonDocument<192> doc;
    doc["current_fw_title"] = hp20::version::TITLE;
    doc["current_fw_version"] = hp20::version::STRING;
    String body;
    serializeJson(doc, body);
    if (postAttributes(config, body)) announced = true;
  }

  const uint32_t intervalMs = config.otaCheckSeconds * 1000UL;
  const bool firstDue = !firstCheckDone && now >= FIRST_CHECK_DELAY_MS;
  const bool periodicDue = firstCheckDone && model::elapsed(now, lastCheckAt, intervalMs);
  if (!forceCheck && !firstDue && !periodicDue) return;

  forceCheck = false;
  firstCheckDone = true;
  lastCheckAt = now;
  errorText = "";
  progress = 0;
  currentState = State::Checking;

  Target target;
  if (!fetchTarget(config, target)) {
    reportState(config, "FAILED", errorText);
    return;
  }

  if (target.title.isEmpty() || target.version.isEmpty()) {
    currentState = State::UpToDate;
    return;
  }

  if (target.title != hp20::version::TITLE) {
    setError("FW TITLE KHAC HP20");
    reportState(config, "FAILED", errorText);
    return;
  }

  if (!newerThanCurrent(target.version)) {
    currentState = State::UpToDate;
    reportState(config, "UPDATED");
    return;
  }

  downloadAndApply(config, target);
}

} } // namespace hp20::ota
