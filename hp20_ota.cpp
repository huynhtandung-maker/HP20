#include "hp20_ota.h"
#include "thingsboard_ca.h"
#include "github_ca.h"

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
constexpr size_t MAX_MANIFEST_BYTES = 16UL * 1024UL;

constexpr uint32_t RPC_POLL_INTERVAL_MS = 10000UL;
constexpr uint32_t RPC_SERVER_WAIT_MS = 300UL;
constexpr uint32_t RPC_HTTP_TIMEOUT_MS = 2500UL;

State currentState = State::Waiting;
uint32_t lastCheckAt = 0;
uint32_t lastRpcPollAt = 0;
bool forceCheck = false;
bool announced = false;
bool firstCheckDone = false;
uint8_t progress = 0;
String errorText;

struct Target {
  String title;
  String version;
  String url;
  String checksum;
  String algorithm;
  size_t size = 0;
};

struct GitHubReleaseRef {
  String owner;
  String repo;
  String tag;
  String asset;
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

bool openThingsBoardSecure(HTTPClient& http, WiFiClientSecure& client,
                           const Config& config, const String& url,
                           bool quiet = false) {
  const char* ca = hp20::tbtrust::effectiveCa(config);
  if (!ca) {
    if (!quiet) setError("THIEU CA TLS");
    return false;
  }

  client.setCACert(ca);
  client.setHandshakeTimeout(8);
  http.setConnectTimeout(quiet ? 4000 : 7000);
  http.setTimeout(quiet ? RPC_HTTP_TIMEOUT_MS : HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(client, url)) {
    if (!quiet) setError("KHONG MO HTTPS");
    return false;
  }
  return true;
}

bool openPublicSecure(HTTPClient& http, WiFiClientSecure& client,
                      const String& url) {
  // GitHub public HTTPS uses its own trust domain, separate from ThingsBoard.
  // Arduino-ESP32 3.3.11 does not expose useBuiltinCACertBundle(), so HP20
  // verifies GitHub with an explicit trusted root CA.
  client.setCACert(hp20::ghtrust::GITHUB_ROOT_CA);
  client.setHandshakeTimeout(10);
  http.setConnectTimeout(8000);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, url)) {
    setError("KHONG MO PUBLIC HTTPS");
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
  if (!openThingsBoardSecure(http, client, config, url)) return false;
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(json);
  http.end();
  return code >= 200 && code < 300;
}

void reportState(const Config& config, const char* fwState, const String& error = "") {
  StaticJsonDocument<320> doc;
  doc["current_fw_title"] = hp20::version::TITLE;
  doc["current_fw_version"] = hp20::version::STRING;
  doc["fw_state"] = fwState;
  doc["fw_progress"] = progress;
  doc["fw_error"] = error;
  String body;
  serializeJson(doc, body);
  postAttributes(config, body);
}

bool postRpcReply(const Config& config, long requestId, const String& json) {
  WiFiClientSecure client;
  HTTPClient http;
  const String url = "https://" + config.host + "/api/v1/" +
                     config.token + "/rpc/" + String(requestId);
  if (!openThingsBoardSecure(http, client, config, url, true)) return false;
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(json);
  http.end();
  return code >= 200 && code < 300;
}

bool handleRpcCommand(const Config& config, const String& body) {
  DynamicJsonDocument request(768);
  if (deserializeJson(request, body)) return false;

  const long requestId = request["id"] | -1L;
  const String method = request["method"] | "";
  if (requestId < 0 || method.isEmpty()) return false;

  StaticJsonDocument<384> reply;
  reply["currentVersion"] = hp20::version::STRING;
  reply["otaEnabled"] = config.otaEnabled;

  if (method == "updateFirmware" || method == "checkFirmware") {
    if (!config.otaEnabled) {
      reply["accepted"] = false;
      reply["result"] = "OTA_DISABLED";
    } else {
      forceCheck = true;
      reply["accepted"] = true;
      reply["result"] = "OTA_CHECK_SCHEDULED";
      reply["message"] = "HP20 will check the assigned ThingsBoard firmware now";
      Serial.printf("TB RPC %s accepted (id=%ld)\n", method.c_str(), requestId);
    }
  } else if (method == "getDeviceInfo") {
    reply["accepted"] = true;
    reply["title"] = hp20::version::TITLE;
    reply["version"] = hp20::version::STRING;
    reply["otaState"] = label();
    reply["progress"] = progress;
    reply["lastError"] = errorText;
  } else {
    reply["accepted"] = false;
    reply["result"] = "UNSUPPORTED_METHOD";
    reply["method"] = method;
    Serial.printf("TB RPC unsupported method=%s id=%ld\n", method.c_str(), requestId);
  }

  String response;
  serializeJson(reply, response);
  return postRpcReply(config, requestId, response);
}

void pollRpc(uint32_t now, const Config& config) {
  if (!model::elapsed(now, lastRpcPollAt, RPC_POLL_INTERVAL_MS)) return;
  lastRpcPollAt = now;

  WiFiClientSecure client;
  HTTPClient http;
  const String url = "https://" + config.host + "/api/v1/" + config.token +
                     "/rpc?timeout=" + String(RPC_SERVER_WAIT_MS);
  if (!openThingsBoardSecure(http, client, config, url, true)) return;

  const int code = http.GET();
  if (code == HTTP_CODE_OK) {
    const String body = http.getString();
    http.end();
    if (!body.isEmpty()) handleRpcCommand(config, body);
    return;
  }
  http.end();
}

bool fetchTarget(const Config& config, Target& target) {
  WiFiClientSecure client;
  HTTPClient http;
  String url = "https://" + config.host + "/api/v1/" + config.token +
               "/attributes?sharedKeys=fw_title,fw_version,fw_url,fw_checksum,"
               "fw_checksum_algorithm,fw_size";
  if (!openThingsBoardSecure(http, client, config, url)) return false;

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("CHECK HTTP ") + code);
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, body)) {
    setError("CHECK JSON");
    return false;
  }

  JsonObject shared = doc["shared"].as<JsonObject>();
  if (shared.isNull()) {
    target = Target{};
    return true;
  }

  target.title = shared["fw_title"] | "";
  target.version = shared["fw_version"] | "";
  target.url = shared["fw_url"] | "";
  target.checksum = shared["fw_checksum"] | "";
  target.algorithm = shared["fw_checksum_algorithm"] | "";
  target.size = shared["fw_size"] | 0UL;
  return true;
}

bool parseGitHubReleaseUrl(const String& url, GitHubReleaseRef& out) {
  const String prefix = "https://github.com/";
  if (!url.startsWith(prefix)) return false;

  String rest = url.substring(prefix.length());
  const int ownerEnd = rest.indexOf('/');
  if (ownerEnd <= 0) return false;
  const int repoEnd = rest.indexOf('/', ownerEnd + 1);
  if (repoEnd <= ownerEnd + 1) return false;

  out.owner = rest.substring(0, ownerEnd);
  out.repo = rest.substring(ownerEnd + 1, repoEnd);

  const String marker = "/releases/download/";
  String suffix = rest.substring(repoEnd);
  if (!suffix.startsWith(marker)) return false;

  String releasePart = suffix.substring(marker.length());
  const int tagEnd = releasePart.indexOf('/');
  if (tagEnd <= 0 || tagEnd >= int(releasePart.length()) - 1) return false;

  out.tag = releasePart.substring(0, tagEnd);
  out.asset = releasePart.substring(tagEnd + 1);
  return !out.owner.isEmpty() && !out.repo.isEmpty() &&
         !out.tag.isEmpty() && !out.asset.isEmpty();
}

bool resolveGitHubManifest(Target& target) {
  GitHubReleaseRef ref;
  if (!parseGitHubReleaseUrl(target.url, ref)) {
    setError("FW URL KHONG PHAI GITHUB RELEASE");
    return false;
  }

  const String expectedTag = "v" + target.version;
  if (ref.tag != expectedTag && ref.tag != target.version) {
    setError("FW URL VERSION KHONG KHOP");
    return false;
  }

  // Integrity metadata comes from the release manifest itself.
  // This avoids a separate dependency on api.github.com while preserving
  // end-to-end SHA-256 verification of the exact release asset.
  const String manifestUrl =
      "https://github.com/" + ref.owner + "/" + ref.repo +
      "/releases/download/" + ref.tag + "/SHA256SUMS.txt";

  WiFiClientSecure client;
  HTTPClient http;
  if (!openPublicSecure(http, client, manifestUrl)) return false;

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("GITHUB SUMS HTTP ") + code);
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength > 0 && size_t(contentLength) > MAX_MANIFEST_BYTES) {
    http.end();
    setError("GITHUB SUMS QUA LON");
    return false;
  }

  String manifest = http.getString();
  http.end();

  if (manifest.isEmpty()) {
    setError("GITHUB SUMS RONG");
    return false;
  }
  if (manifest.length() > MAX_MANIFEST_BYTES) {
    setError("GITHUB SUMS QUA LON");
    return false;
  }

  int cursor = 0;
  while (cursor < int(manifest.length())) {
    int lineEnd = manifest.indexOf('\n', cursor);
    if (lineEnd < 0) lineEnd = manifest.length();

    String line = manifest.substring(cursor, lineEnd);
    line.trim();

    if (line.length() >= 64) {
      String sha = line.substring(0, 64);
      if (validHexSha256(sha)) {
        String filename = line.substring(64);
        filename.trim();

        // Accept both common sha256sum formats:
        // <sha>  filename
        // <sha> *filename
        if (filename.startsWith("*")) {
          filename.remove(0, 1);
          filename.trim();
        }

        if (filename == ref.asset) {
          sha.toLowerCase();
          target.algorithm = "SHA256";
          target.checksum = sha;

          // For external releases, size is established from the actual binary
          // HTTP response rather than duplicated in ThingsBoard metadata.
          target.size = 0;

          Serial.printf("OTA manifest OK: %s sha256=%s\n",
                        ref.asset.c_str(), target.checksum.c_str());
          return true;
        }
      }
    }

    cursor = lineEnd + 1;
  }

  setError("KHONG TIM THAY SHA256");
  return false;
}

bool prepareTarget(Target& target) {
  if (!target.url.isEmpty()) {
    return resolveGitHubManifest(target);
  }

  // Backward compatibility for packages whose binary is stored in ThingsBoard.
  if (!target.algorithm.equalsIgnoreCase("SHA256") ||
      !validHexSha256(target.checksum)) {
    setError("CAN SHA256");
    return false;
  }
  if (target.size < 65536 || target.size > MAX_FIRMWARE_BYTES) {
    setError("FW SIZE SAI");
    return false;
  }
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

bool downloadAndApply(const Config& config, Target target) {
  if (target.title != hp20::version::TITLE) {
    setError("SAI FW TITLE");
    return false;
  }
  if (!newerThanCurrent(target.version)) {
    currentState = State::UpToDate;
    return true;
  }
  if (!prepareTarget(target)) {
    reportState(config, "FAILED", errorText);
    return false;
  }

  currentState = State::UpdateAvailable;
  reportState(config, "DOWNLOADING");
  currentState = State::Downloading;
  progress = 0;

  WiFiClientSecure client;
  HTTPClient http;
  String downloadUrl;

  if (!target.url.isEmpty()) {
    downloadUrl = target.url;
    if (!openPublicSecure(http, client, downloadUrl)) {
      reportState(config, "FAILED", errorText);
      return false;
    }
  } else {
    downloadUrl = "https://" + config.host + "/api/v1/" + config.token +
                  "/firmware?title=" + urlEncode(target.title) +
                  "&version=" + urlEncode(target.version);
    if (!openThingsBoardSecure(http, client, config, downloadUrl)) {
      reportState(config, "FAILED", errorText);
      return false;
    }
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    setError(String("FW HTTP ") + code);
    reportState(config, "FAILED", errorText);
    return false;
  }

  const int contentLength = http.getSize();

  if (!target.url.isEmpty()) {
    // External GitHub release: the actual binary response is authoritative
    // for size. SHA-256 remains authoritative for content integrity.
    if (contentLength <= 0) {
      http.end();
      setError("FW KHONG CO CONTENT LENGTH");
      reportState(config, "FAILED", errorText);
      return false;
    }

    if (size_t(contentLength) < 65536UL ||
        size_t(contentLength) > MAX_FIRMWARE_BYTES) {
      http.end();
      setError("FW CONTENT LENGTH SAI");
      reportState(config, "FAILED", errorText);
      return false;
    }

    target.size = size_t(contentLength);
  } else {
    // Backward compatibility for ThingsBoard-hosted binary packages.
    if (contentLength > 0 && size_t(contentLength) != target.size) {
      http.end();
      setError("FW CONTENT LENGTH KHONG KHOP");
      reportState(config, "FAILED", errorText);
      return false;
    }
  }

  if (target.size < 65536UL || target.size > MAX_FIRMWARE_BYTES) {
    http.end();
    setError("FW SIZE SAI");
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

  progress = 100;
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
  Serial.printf("OTA verified: %s -> %s via %s. Rebooting...\n",
                hp20::version::STRING,
                target.version.c_str(),
                target.url.isEmpty() ? "ThingsBoard" : "GitHub");
  delay(800);
  ESP.restart();
  return true;
}

} // namespace

void begin() {
  currentState = State::Waiting;
  lastCheckAt = millis();
  lastRpcPollAt = millis();
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

  if (!announced) {
    StaticJsonDocument<320> doc;
    doc["current_fw_title"] = hp20::version::TITLE;
    doc["current_fw_version"] = hp20::version::STRING;
    doc["ota_rpc_supported"] = true;
    doc["ota_rpc_method"] = "updateFirmware";
    String body;
    serializeJson(doc, body);
    if (postAttributes(config, body)) announced = true;
  }

  pollRpc(now, config);

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
  reportState(config, "CHECKING");

  Target target;
  if (!fetchTarget(config, target)) {
    reportState(config, "FAILED", errorText);
    return;
  }

  if (target.title.isEmpty() || target.version.isEmpty()) {
    currentState = State::UpToDate;
    reportState(config, "NO_FIRMWARE_ASSIGNED");
    return;
  }

  if (target.title != hp20::version::TITLE) {
    setError("FW TITLE KHAC HP20");
    reportState(config, "FAILED", errorText);
    return;
  }

  if (!newerThanCurrent(target.version)) {
    currentState = State::UpToDate;
    progress = 100;
    reportState(config, "UPDATED");
    return;
  }

  downloadAndApply(config, target);
}

} } // namespace hp20::ota
