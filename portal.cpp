#include "portal.h"
#include "thingsboard_ca.h"

#include "hp20_version.h"
#include "model.h"
#include "settings.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <mbedtls/x509_crt.h>

static WebServer server(80);
static DNSServer dns;
static Config* current = nullptr;

static bool active = false;
static bool saved = false;
static bool routes = false;
static uint32_t started = 0;
static String name;
static String csrf;

struct PortalRuntimeStatus {
  PortalPhase phase = PortalPhase::Idle;
  uint8_t progress = 0;
  String title;
  String detail;
  String ssid;
  String ip;
};
static PortalRuntimeStatus statusView;

struct ScanEntry {
  String ssid;
  int32_t rssi = -100;
  uint8_t channel = 0;
  bool open = false;
};
constexpr uint8_t MAX_SCAN_RESULTS = 20;
static ScanEntry scanCache[MAX_SCAN_RESULTS];
static uint8_t scanCount = 0;
static bool scanRunning = false;
static uint32_t scanStarted = 0;

namespace {

String randomHex() {
  char b[17];
  snprintf(b, sizeof(b), "%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random());
  return b;
}

bool permitted() {
  return active && server.client().localIP() == WiFi.softAPIP();
}

String escape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

String tokenHint(const String& token) {
  if (token.isEmpty()) return "Chưa có token";
  const int n = token.length();
  return String("Đã lưu ••••") + token.substring(max(0, n - 4));
}

void commonHeaders() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
}

void reply(int code, const String& text) {
  commonHeaders();
  server.sendHeader(
    "Content-Security-Policy",
    "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
    "connect-src 'self'; form-action 'self'; frame-ancestors 'none'"
  );
  server.send(code, "text/html; charset=utf-8", text);
}

void replyJson(const String& text) {
  commonHeaders();
  server.send(200, "application/json; charset=utf-8", text);
}

void captiveRedirect() {
  if (!active) {
    server.send(404, "text/plain", "HP20 setup portal is closed");
    return;
  }
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

bool hostname(const String& h) {
  if (h.isEmpty()) return true;
  if (h.length() > 127 || h.indexOf('.') < 0) return false;
  for (unsigned i = 0; i < h.length(); ++i) {
    if (!isalnum((unsigned char)h[i]) && h[i] != '.' && h[i] != '-') return false;
  }
  return h[0] != '.' && h[h.length() - 1] != '.';
}

const char* phaseCode(PortalPhase phase) {
  switch (phase) {
    case PortalPhase::Ready:             return "READY";
    case PortalPhase::ScanningNetworks:  return "SCANNING";
    case PortalPhase::Saved:             return "SAVED";
    case PortalPhase::ConnectingWifi:    return "CONNECTING_WIFI";
    case PortalPhase::WifiConnected:     return "WIFI_CONNECTED";
    case PortalPhase::SyncingTime:       return "SYNCING_TIME";
    case PortalPhase::ConnectingCloud:   return "CONNECTING_CLOUD";
    case PortalPhase::Success:           return "SUCCESS";
    case PortalPhase::Failed:            return "FAILED";
    case PortalPhase::Idle:
    default:                             return "IDLE";
  }
}

void clearScanCache() {
  for (uint8_t i = 0; i < MAX_SCAN_RESULTS; ++i) scanCache[i] = ScanEntry();
  scanCount = 0;
}

void sortScanCache() {
  for (uint8_t i = 1; i < scanCount; ++i) {
    ScanEntry key = scanCache[i];
    int j = int(i) - 1;
    while (j >= 0 && scanCache[j].rssi < key.rssi) {
      scanCache[j + 1] = scanCache[j];
      --j;
    }
    scanCache[j + 1] = key;
  }
}

void beginScan() {
  if (!active || scanRunning) return;

  const int state = WiFi.scanComplete();
  if (state >= 0 || state == WIFI_SCAN_FAILED) WiFi.scanDelete();

  clearScanCache();
  const int result = WiFi.scanNetworks(true, true);
  scanRunning = result == WIFI_SCAN_RUNNING;
  scanStarted = millis();

  if (scanRunning && statusView.phase == PortalPhase::Ready) {
    portalSetStatus(
      PortalPhase::ScanningNetworks, 10,
      "Đang tìm mạng Wi-Fi",
      "HP20 đang quét các mạng 2.4 GHz xung quanh."
    );
  }
}

void harvestScan() {
  if (!scanRunning) return;

  const int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    if (model::elapsed(millis(), scanStarted, 15000UL)) {
      WiFi.scanDelete();
      scanRunning = false;
      if (statusView.phase == PortalPhase::ScanningNetworks) {
        portalSetStatus(PortalPhase::Ready, 0, "Sẵn sàng cài đặt",
                        "Không quét được mạng. Có thể nhập tên Wi-Fi thủ công.");
      }
    }
    return;
  }

  scanRunning = false;
  if (count < 0) {
    WiFi.scanDelete();
    if (statusView.phase == PortalPhase::ScanningNetworks) {
      portalSetStatus(PortalPhase::Ready, 0, "Sẵn sàng cài đặt",
                      "Không quét được mạng. Có thể nhập tên Wi-Fi thủ công.");
    }
    return;
  }

  clearScanCache();
  for (int i = 0; i < count && scanCount < MAX_SCAN_RESULTS; ++i) {
    String ssid = WiFi.SSID(i);
    ssid.trim();
    if (ssid.isEmpty()) continue;

    int existing = -1;
    for (uint8_t j = 0; j < scanCount; ++j) {
      if (scanCache[j].ssid == ssid) {
        existing = j;
        break;
      }
    }

    const int32_t rssi = WiFi.RSSI(i);
    const bool isOpen = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    if (existing >= 0) {
      if (rssi > scanCache[existing].rssi) {
        scanCache[existing].rssi = rssi;
        scanCache[existing].channel = uint8_t(WiFi.channel(i));
        scanCache[existing].open = isOpen;
      }
      continue;
    }

    scanCache[scanCount].ssid = ssid;
    scanCache[scanCount].rssi = rssi;
    scanCache[scanCount].channel = uint8_t(WiFi.channel(i));
    scanCache[scanCount].open = isOpen;
    ++scanCount;
  }
  WiFi.scanDelete();
  sortScanCache();

  if (statusView.phase == PortalPhase::ScanningNetworks) {
    portalSetStatus(PortalPhase::Ready, 0, "Chọn mạng Wi-Fi",
                    scanCount ? "Danh sách đã cập nhật." : "Không tìm thấy mạng; có thể nhập thủ công.");
  }
}

String signalLabel(int32_t rssi) {
  if (rssi >= -55) return "Rất mạnh";
  if (rssi >= -67) return "Tốt";
  if (rssi >= -75) return "Trung bình";
  return "Yếu";
}

void apiNetworks() {
  if (!permitted()) {
    server.send(403, "application/json", "{\"error\":\"forbidden\"}");
    return;
  }

  DynamicJsonDocument doc(6000);
  doc["scanning"] = scanRunning;
  doc["count"] = scanCount;

  JsonArray savedItems = doc["saved"].to<JsonArray>();
  if (current) {
    for (uint8_t i = 0; i < current->wifiProfileCount && i < MAX_WIFI_PROFILES; ++i) {
      if (current->wifiProfiles[i].ssid.isEmpty()) continue;
      JsonObject row = savedItems.add<JsonObject>();
      row["ssid"] = current->wifiProfiles[i].ssid;
      row["preferred"] = i == 0;
    }
  }

  JsonArray items = doc["networks"].to<JsonArray>();
  for (uint8_t i = 0; i < scanCount; ++i) {
    JsonObject item = items.add<JsonObject>();
    item["ssid"] = scanCache[i].ssid;
    item["rssi"] = scanCache[i].rssi;
    item["channel"] = scanCache[i].channel;
    item["open"] = scanCache[i].open;
    item["known"] = current && wifiProfileIndex(*current, scanCache[i].ssid) >= 0;
    item["preferred"] = current && current->wifiProfileCount > 0 &&
                        current->wifiProfiles[0].ssid == scanCache[i].ssid;
    item["signal"] = signalLabel(scanCache[i].rssi);
    item["recommended"] = i == 0 && scanCache[i].rssi >= -75;
  }

  String json;
  serializeJson(doc, json);
  replyJson(json);
}

void apiScan() {
  if (!permitted()) {
    server.send(403, "application/json", "{\"error\":\"forbidden\"}");
    return;
  }
  beginScan();
  replyJson("{\"ok\":true}");
}

void apiStatus() {
  if (!permitted()) {
    server.send(403, "application/json", "{\"error\":\"forbidden\"}");
    return;
  }

  DynamicJsonDocument doc(1200);
  doc["phase"] = phaseCode(statusView.phase);
  doc["progress"] = statusView.progress;
  doc["title"] = statusView.title;
  doc["detail"] = statusView.detail;
  doc["ssid"] = statusView.ssid;
  doc["ip"] = statusView.ip;
  doc["connected"] = WiFi.status() == WL_CONNECTED;
  if (WiFi.status() == WL_CONNECTED) {
    doc["rssi"] = WiFi.RSSI();
    doc["local_ip"] = WiFi.localIP().toString();
  }

  String json;
  serializeJson(doc, json);
  replyJson(json);
}

void apiForget() {
  if (!permitted() || server.arg("csrf") != csrf || !current) {
    server.send(403, "application/json", "{\"error\":\"forbidden\"}");
    return;
  }

  const String ssid = server.arg("ssid");
  Config next = *current;
  if (ssid.isEmpty() || !forgetWifiProfile(next, ssid) || !saveConfig(next)) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  *current = next;
  saved = true; // Let application reconnect to the new preferred profile.
  started = millis();
  replyJson("{\"ok\":true}");
}

void apiResetWifi() {
  if (!permitted() || server.arg("csrf") != csrf || !current) {
    server.send(403, "application/json", "{\"error\":\"forbidden\"}");
    return;
  }

  Config next = *current;
  clearWifiProfiles(next);
  if (!saveConfig(next)) {
    server.send(500, "application/json", "{\"ok\":false}");
    return;
  }

  *current = next;
  saved = true;
  started = millis();
  portalSetStatus(PortalPhase::Ready, 0, "Đã quên các mạng Wi-Fi",
                  "Chọn một mạng mới để kết nối.");
  replyJson("{\"ok\":true}");
}

String waitPage() {
  String page = R"HTML(<!doctype html><html lang="vi"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>HP20 · Đang kết nối</title>
<style>
:root{--cream:#fff8ec;--cream2:#f6ead6;--ink:#332419;--muted:#7d695a;--orange:#df6d2d;--orange2:#f09a45;--line:#ead8bf;--good:#7b7b35;--bad:#b94b35}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(165deg,#fff9ef,#f4dfc3);color:var(--ink);font:15px system-ui,-apple-system,Segoe UI,sans-serif;min-height:100vh}
main{max-width:520px;margin:auto;padding:22px 16px 40px}.brand{display:flex;justify-content:space-between;align-items:center;margin-bottom:22px}.brand b{font-size:20px}.pill{border:1px solid var(--line);background:#fffaf2;border-radius:999px;padding:6px 10px;color:var(--muted);font-size:12px}
.card{background:#fffdf8;border:1px solid var(--line);border-radius:24px;padding:22px;box-shadow:0 18px 55px #8f5f3218}.eyebrow{font-size:12px;font-weight:800;letter-spacing:.1em;color:var(--orange);text-transform:uppercase}.card h1{font-size:28px;line-height:1.08;margin:7px 0 9px}.lead{color:var(--muted);line-height:1.5;margin:0 0 22px}
.track{height:12px;background:#f0dfc9;border-radius:999px;overflow:hidden}.bar{height:100%;width:8%;background:linear-gradient(90deg,var(--orange),var(--orange2));border-radius:999px;transition:width .55s ease}
.stage{display:grid;grid-template-columns:30px 1fr;gap:11px;align-items:start;margin-top:22px}.dot{width:28px;height:28px;border-radius:50%;background:#fff0dc;border:1px solid #efcda6;display:grid;place-items:center;color:var(--orange);font-weight:900}.stage h2{font-size:19px;margin:2px 0 4px}.stage p{margin:0;color:var(--muted);line-height:1.45}
.meta{margin-top:19px;border-top:1px solid var(--line);padding-top:14px;display:grid;gap:8px}.row{display:flex;justify-content:space-between;gap:12px}.row span{color:var(--muted)}.row b{text-align:right;word-break:break-word}
.actions{display:flex;gap:10px;margin-top:22px}.btn{flex:1;padding:13px;border-radius:14px;border:1px solid var(--line);background:#fffaf1;color:var(--ink);text-decoration:none;text-align:center;font-weight:750}.btn.primary{background:var(--orange);color:white;border-color:var(--orange)}
.note{margin-top:16px;padding:11px 12px;border-radius:13px;background:#fff3df;color:#76502d;font-size:13px;line-height:1.45}
</style></head><body><main>
<div class="brand"><b>HP20</b><span class="pill">v%VERSION%</span></div>
<div class="card"><div class="eyebrow">Thiết lập kết nối</div><h1 id="title">Đã lưu cấu hình</h1>
<p id="detail" class="lead">HP20 đang bắt đầu kết nối. Không cần thao tác thêm.</p>
<div class="track"><div id="bar" class="bar"></div></div>
<div class="stage"><div id="dot" class="dot">1</div><div><h2 id="phase">Đang chuẩn bị</h2><p id="hint">Giữ trang này mở; trạng thái được lấy trực tiếp từ thiết bị.</p></div></div>
<div class="meta"><div class="row"><span>Wi-Fi</span><b id="ssid">—</b></div><div class="row"><span>IP</span><b id="ip">—</b></div></div>
<div class="actions"><a class="btn" href="/">Quay lại cài đặt</a></div>
<div class="note">OLED và còi trên HP20 cũng phản hồi theo cùng tiến trình. Nếu kết nối thất bại, AP cài đặt vẫn mở để sửa ngay.</div></div>
<script>
const labels={READY:['Sẵn sàng','Chọn hoặc nhập mạng Wi-Fi'],SCANNING:['Đang tìm mạng','HP20 đang quét mạng 2.4 GHz'],SAVED:['Đã lưu','Chuẩn bị kết nối'],CONNECTING_WIFI:['Đang kết nối Wi-Fi','Đang xác thực với router'],WIFI_CONNECTED:['Wi-Fi đã kết nối','Đã nhận địa chỉ mạng'],SYNCING_TIME:['Đồng bộ thời gian','Chuẩn bị kết nối HTTPS an toàn'],CONNECTING_CLOUD:['Kết nối ThingsBoard','Đang xác nhận cloud'],SUCCESS:['Hoàn tất','Thiết bị đã sẵn sàng'],FAILED:['Chưa kết nối được','Kiểm tra mật khẩu hoặc chọn mạng khác']};
let stopped=false;
async function poll(){if(stopped)return;try{const r=await fetch('/api/status',{cache:'no-store'});const s=await r.json();const x=labels[s.phase]||[s.title||'Đang xử lý',s.detail||'Vui lòng chờ'];document.getElementById('title').textContent=s.title||x[0];document.getElementById('detail').textContent=s.detail||x[1];document.getElementById('phase').textContent=x[0];document.getElementById('hint').textContent=x[1];document.getElementById('bar').style.width=Math.max(4,Math.min(100,s.progress||0))+'%';document.getElementById('ssid').textContent=s.ssid||'—';document.getElementById('ip').textContent=s.ip||s.local_ip||'—';document.getElementById('dot').textContent=s.phase==='SUCCESS'?'✓':s.phase==='FAILED'?'!':'•';if(s.phase==='SUCCESS'){stopped=true;document.querySelector('.note').textContent='Kết nối thành công. HP20 sẽ tự đóng mạng cài đặt sau ít giây.'}else if(s.phase==='FAILED'){document.querySelector('.note').textContent='Kết nối chưa thành công. AP vẫn mở; nhấn “Quay lại cài đặt” để sửa mà không phải bắt đầu lại.'}}catch(e){}if(!stopped)setTimeout(poll,800)}
poll();
</script></main></body></html>)HTML";
  page.replace("%VERSION%", hp20::version::STRING);
  return page;
}

void home() {
  if (!permitted()) {
    reply(403, "Connect to the HP20 setup Wi-Fi.");
    return;
  }

  const String storedWifi = current && current->wifiProfileCount
                          ? String("Đã nhớ ") + String(current->wifiProfileCount) + " mạng"
                          : "Chưa có Wi-Fi được lưu";
  const String storedToken = current ? tokenHint(current->token) : "Chưa có token";
  String caStatus = "Chưa có CA TLS";
  if (current) {
    if (!current->ca.isEmpty()) caStatus = "CA TLS tùy chỉnh đã lưu";
    else if (hp20::tbtrust::isDefaultCloudHost(current->host))
      caStatus = "CA ThingsBoard Cloud có sẵn trong firmware";
  }

  String page = R"HTML(<!doctype html><html lang="vi"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>HP20 · Thiết lập</title>
<style>
:root{--cream:#fff8ec;--cream2:#f5e6cf;--paper:#fffdf8;--ink:#322318;--muted:#806c5d;--orange:#dc6b2b;--orange2:#f09a45;--orange3:#fff0dc;--line:#ead7bd;--shadow:#8f5f3218;--danger:#b94b35;--olive:#77752f}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 90% 0,#ffe7c5 0,transparent 34%),linear-gradient(180deg,#fff9ef,#f3dfc4);color:var(--ink);font:15px system-ui,-apple-system,Segoe UI,sans-serif;min-height:100vh}
main{max-width:560px;margin:auto;padding:18px 15px 38px}.brand{display:flex;align-items:center;justify-content:space-between;margin:4px 2px 20px}.brand b{font-size:21px;letter-spacing:.01em}.pill{padding:6px 10px;border:1px solid var(--line);border-radius:999px;background:#fffaf2;color:var(--muted);font-size:12px}
.hero{margin:0 2px 18px}.hero .eyebrow{color:var(--orange);font-weight:850;font-size:12px;letter-spacing:.11em}.hero h1{font-size:30px;line-height:1.05;margin:6px 0 9px}.hero p{color:var(--muted);line-height:1.5;margin:0;max-width:48ch}
.steps{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;margin:18px 0}.step{border:1px solid var(--line);background:#fff8ef;border-radius:13px;padding:9px 6px;text-align:center;color:var(--muted);font-size:12px}.step.on{background:var(--orange3);color:#9d4c1d;border-color:#efc391;font-weight:800}
.card{background:var(--paper);border:1px solid var(--line);border-radius:23px;padding:19px;box-shadow:0 18px 55px var(--shadow)}.pane{display:none}.pane.on{display:block}.kicker{color:var(--orange);font-size:12px;font-weight:850;letter-spacing:.09em}.card h2{font-size:23px;margin:5px 0 5px}.sub{color:var(--muted);margin:0 0 16px;line-height:1.45}
label{display:block;margin:15px 0 6px;font-weight:720}.field{position:relative}input,textarea,button,select{width:100%;font:inherit;border-radius:13px;border:1px solid #dfc7aa;background:#fffaf2;color:var(--ink);padding:13px 44px 13px 13px;outline:none}input:focus,textarea:focus,select:focus{border-color:var(--orange);box-shadow:0 0 0 3px #dc6b2b18}textarea{min-height:105px;resize:vertical;padding-right:13px}.eye{position:absolute;right:5px;top:5px;width:40px;height:38px;padding:0;border:0;background:transparent;color:#9f6d4b;font-size:18px}
.hint{display:flex;justify-content:space-between;gap:8px;color:var(--muted);font-size:12px;margin-top:6px}.ok{color:var(--olive)}.warn{color:#a86b20}.actions{display:flex;gap:10px;margin-top:22px}.actions button{padding:13px}.primary{background:linear-gradient(135deg,var(--orange),var(--orange2));color:white;border-color:transparent;font-weight:850}.secondary{background:#fffaf2;color:var(--ink)}
.netHead{display:flex;align-items:center;justify-content:space-between;margin:15px 0 8px}.netHead label{margin:0}.refresh{width:auto;padding:8px 11px;border-radius:999px;background:#fff4e4;color:#9f4d1d;border-color:#efc79a;font-size:12px;font-weight:800}.networks{display:grid;gap:8px}.network{width:100%;display:grid;grid-template-columns:1fr auto;gap:7px;text-align:left;padding:12px;border:1px solid var(--line);background:#fffaf2;border-radius:15px;color:var(--ink)}.network strong{font-size:14px}.network small{display:block;color:var(--muted);margin-top:3px}.badges{display:flex;gap:5px;flex-wrap:wrap;margin-top:5px}.badge{font-size:10px;border-radius:999px;padding:3px 6px;background:#f7e6d1;color:#8f5a32}.badge.hot{background:#ffe1bc;color:#9d4c1d;font-weight:800}.bars{display:flex;gap:2px;align-items:end;height:18px}.bars i{display:block;width:3px;background:#d8bea0;border-radius:3px}.bars i:nth-child(1){height:5px}.bars i:nth-child(2){height:9px}.bars i:nth-child(3){height:13px}.bars i:nth-child(4){height:17px}.bars i.off{background:#eee0cf}
.empty{padding:13px;border:1px dashed #dbc0a0;border-radius:14px;color:var(--muted);background:#fffaf2;text-align:center}.check{display:flex;gap:10px;align-items:flex-start;margin:14px 0}.check input{width:auto;margin-top:3px}.check label{margin:0;font-weight:520}.review{display:grid;gap:10px}.row{display:flex;justify-content:space-between;gap:12px;border-bottom:1px solid var(--line);padding:10px 0}.row span:first-child{color:var(--muted)}details{margin-top:16px;border-top:1px solid var(--line);padding-top:14px}summary{cursor:pointer;color:#8f5a32;font-weight:700}
.note{border:1px solid #edc99e;background:#fff1de;padding:11px 12px;border-radius:13px;color:#76502d;font-size:13px;line-height:1.45}.danger{border-color:#efc0b8;background:#fff4f1;color:var(--danger)}.manage{display:grid;gap:8px;margin-top:12px}.manage button{padding:11px;background:#fffaf2}.manage .dangerBtn{color:var(--danger);border-color:#ecc3bb}
footer{color:var(--muted);font-size:12px;text-align:center;margin-top:16px;line-height:1.55}
.busy{position:fixed;inset:0;background:#fff6e9e8;backdrop-filter:blur(8px);display:none;place-items:center;z-index:9;padding:20px}.busy.on{display:grid}.busyCard{width:min(420px,100%);background:var(--paper);border:1px solid var(--line);border-radius:22px;padding:22px;text-align:center;box-shadow:0 18px 55px var(--shadow)}.spinner{width:34px;height:34px;margin:0 auto 13px;border:4px solid #f0dcc4;border-top-color:var(--orange);border-radius:50%;animation:spin .8s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}
</style></head><body><main>
<div class="brand"><b>HP20</b><span class="pill">v%VERSION%</span></div>
<div class="hero"><div class="eyebrow">THIẾT LẬP THIẾT BỊ</div><h1>Kết nối nhẹ nhàng hơn.</h1><p>HP20 nhớ mạng đã dùng, tự kết nối lại và chỉ cần mở trang này khi bạn muốn đổi mạng hoặc cấu hình cloud.</p></div>
<div class="steps"><div id="s1" class="step on">1 · Wi-Fi</div><div id="s2" class="step">2 · Cloud</div><div id="s3" class="step">3 · Xác nhận</div></div>
<form action="/save" method="post" onsubmit="return finalise()"><input type="hidden" name="csrf" value="%CSRF%">
<div class="card">
<section id="p1" class="pane on"><div class="kicker">BƯỚC 1 / 3</div><h2>Chọn mạng Wi-Fi</h2><p class="sub">Ưu tiên mạng 2.4 GHz có tín hiệu tốt. Mạng đã từng dùng sẽ không cần nhập lại mật khẩu.</p>
<div class="netHead"><label>Mạng xung quanh</label><button class="refresh" type="button" onclick="rescan()">Quét lại</button></div>
<div id="networks" class="networks"><div class="empty">Đang tìm mạng Wi-Fi…</div></div>
<label>Tên Wi-Fi</label><input id="ssid" name="ssid" maxlength="32" required value="%SSID%" oninput="manualSsid()">
<div class="hint"><span>%WIFI_STATUS%</span><span id="selectionHint">2.4 GHz</span></div>
<label>Mật khẩu Wi-Fi</label><div class="field"><input id="pass" name="pass" type="password" maxlength="63" autocomplete="new-password"><button class="eye" type="button" onclick="toggleSecret('pass',this)" aria-label="Hiện mật khẩu">◉</button></div>
<div class="hint"><span id="passHint">Để trống nếu HP20 đã nhớ mạng này</span><span></span></div>
<div class="check"><input id="open" name="open" type="checkbox"><label for="open">Mạng này không có mật khẩu</label></div>
<div class="actions"><button class="primary" type="button" onclick="go(2)">Tiếp tục</button></div></section>

<section id="p2" class="pane"><div class="kicker">BƯỚC 2 / 3</div><h2>ThingsBoard Cloud</h2><p class="sub">Token đã lưu không được hiện lại đầy đủ. Bạn có thể để trống để giữ token cũ.</p>
<label>Device access token</label><div class="field"><input id="token" name="token" type="password" maxlength="128" autocomplete="new-password"><button class="eye" type="button" onclick="toggleSecret('token',this)" aria-label="Hiện token">◉</button></div><div class="hint"><span class="ok">%TOKEN_STATUS%</span><span>Để trống = giữ token cũ</span></div>
<div class="check"><input id="clearToken" name="clearToken" type="checkbox"><label for="clearToken">Xóa token và ngừng gửi cloud</label></div>
<details><summary>Cài đặt nâng cao</summary>
<label>ThingsBoard host</label><input id="host" name="host" maxlength="127" value="%HOST%" oninput="refreshReview()"><div class="hint"><span>Chỉ hostname, không nhập https://</span></div>
<label>Chu kỳ gửi telemetry</label><select id="minutes" name="minutes" onchange="refreshReview()"><option value="5">5 phút (chuẩn)</option><option value="15">15 phút</option><option value="30">30 phút</option><option value="60">60 phút</option><option value="180">3 giờ</option><option value="360">6 giờ</option><option value="1440">24 giờ</option></select>
<label>CA TLS tùy chỉnh (chỉ server riêng)</label><textarea name="ca" maxlength="5999" placeholder="-----BEGIN CERTIFICATE-----"></textarea><div class="hint"><span>%CA_STATUS%</span><span>ThingsBoard Cloud: không cần nhập CA</span></div>
<div class="check"><input id="otaEnabled" name="otaEnabled" type="checkbox" %OTA_CHECKED%><label for="otaEnabled"><b>Cho phép cập nhật firmware từ xa</b><br><small>HP20 chỉ nhận firmware title đúng, version mới hơn và SHA-256 hợp lệ.</small></label></div>
<label>Kiểm tra bản cập nhật</label><select id="otaHours" name="otaHours"><option value="1">Mỗi 1 giờ</option><option value="3">Mỗi 3 giờ</option><option value="6">Mỗi 6 giờ</option><option value="12">Mỗi 12 giờ</option><option value="24">Mỗi 24 giờ</option></select>
</details>
<div class="actions"><button class="secondary" type="button" onclick="go(1)">Quay lại</button><button class="primary" type="button" onclick="go(3)">Xác nhận</button></div></section>

<section id="p3" class="pane"><div class="kicker">BƯỚC 3 / 3</div><h2>Kiểm tra trước khi lưu</h2><p class="sub">Sau khi lưu, trang trạng thái sẽ theo dõi trực tiếp các bước Wi-Fi → IP → đồng bộ giờ → cloud.</p>
<div class="review"><div class="row"><span>Wi-Fi</span><b id="rWifi">—</b></div><div class="row"><span>ThingsBoard</span><b id="rHost">—</b></div><div class="row"><span>Token</span><b>%TOKEN_STATUS%</b></div><div class="row"><span>Gửi dữ liệu</span><b id="rInterval">—</b></div><div class="row"><span>OTA</span><b id="rOta">—</b></div></div>
<details><summary>Nhắc làm mát</summary><div class="check"><input name="reminder" id="reminder" type="checkbox" %REMINDER_CHECKED%><label for="reminder">Bật nhắc khi FEEL vượt ngưỡng liên tục</label></div><label>Ngưỡng FEEL (°C)</label><input name="threshold" type="number" min="27" max="60" step="0.5" value="%THRESHOLD%"><div class="check"><input name="sound" id="sound" type="checkbox" %SOUND_CHECKED%><label for="sound">Cho phép còi nhắc</label></div></details>
<div class="note">Mật khẩu Wi-Fi và token chỉ lưu trong NVS của ESP32 hoặc file local <b>secrets.h</b> bị Git bỏ qua.</div>
<details><summary>Quản lý mạng đã nhớ</summary><div id="savedNetworks" class="manage"></div><button class="dangerBtn" type="button" onclick="resetWifi()">Quên tất cả mạng Wi-Fi</button></details>
<div class="actions"><button class="secondary" type="button" onclick="go(2)">Quay lại</button><button class="primary" type="submit">Lưu & kết nối</button></div></section>
</div></form>
<footer>Mạng cài đặt HP20 không cần mật khẩu và chỉ mở khi thiết bị chưa cấu hình hoặc bạn giữ BOOT 3 giây.<br>Nếu captive portal không tự bật, mở 192.168.4.1.</footer>
</main>
<div id="busy" class="busy"><div class="busyCard"><div class="spinner"></div><h2>Đang lưu cấu hình</h2><p>HP20 sẽ bắt đầu kết nối ngay. Vui lòng chờ trang trạng thái.</p></div></div>
<script>
const csrf='%CSRF%',known=new Set();
function go(n){for(let i=1;i<=3;i++){document.getElementById('p'+i).classList.toggle('on',i===n);document.getElementById('s'+i).classList.toggle('on',i===n)}if(n===3)refreshReview();scrollTo(0,0)}
function toggleSecret(id,b){const e=document.getElementById(id);e.type=e.type==='password'?'text':'password';b.textContent=e.type==='password'?'◉':'◎'}
function bars(r){const on=r>=-55?4:r>=-67?3:r>=-75?2:1;return '<span class="bars">'+[1,2,3,4].map(i=>'<i class="'+(i<=on?'':'off')+'"></i>').join('')+'</span>'}
function choose(n){document.getElementById('ssid').value=n.ssid;document.getElementById('open').checked=!!n.open;document.getElementById('selectionHint').textContent=n.signal+' · '+n.rssi+' dBm';document.getElementById('passHint').textContent=n.known?'Đã nhớ mật khẩu · để trống để dùng lại':(n.open?'Mạng mở · không cần mật khẩu':'Nhập mật khẩu của mạng này');refreshReview();go(1)}
function manualSsid(){document.getElementById('selectionHint').textContent='Tên mạng nhập thủ công';refreshReview()}
function renderNetworks(data){const box=document.getElementById('networks');box.innerHTML='';known.clear();if(data.scanning){box.innerHTML='<div class="empty">Đang tìm mạng Wi-Fi…</div>';setTimeout(loadNetworks,700);return}if(!data.networks||!data.networks.length){box.innerHTML='<div class="empty">Chưa thấy mạng. Nhấn “Quét lại” hoặc nhập SSID thủ công.</div>';return}for(const n of data.networks){if(n.known)known.add(n.ssid);const b=document.createElement('button');b.type='button';b.className='network';const left=document.createElement('div');const name=document.createElement('strong');name.textContent=n.ssid;left.appendChild(name);const small=document.createElement('small');small.textContent=n.signal+' · '+n.rssi+' dBm · kênh '+n.channel;left.appendChild(small);const tags=document.createElement('div');tags.className='badges';if(n.recommended){const x=document.createElement('span');x.className='badge hot';x.textContent='Khuyến nghị';tags.appendChild(x)}if(n.known){const x=document.createElement('span');x.className='badge';x.textContent=n.preferred?'Đã nhớ · ưu tiên':'Đã nhớ';tags.appendChild(x)}const lock=document.createElement('span');lock.className='badge';lock.textContent=n.open?'Mạng mở':'Có mật khẩu';tags.appendChild(lock);left.appendChild(tags);b.appendChild(left);const right=document.createElement('span');right.innerHTML=bars(n.rssi);b.appendChild(right);b.onclick=()=>choose(n);box.appendChild(b)}renderSaved(data.saved)}
function renderSaved(items){const box=document.getElementById('savedNetworks');box.innerHTML='';const rows=items||[];if(!rows.length){box.innerHTML='<div class="empty">Chưa có mạng đã nhớ.</div>';return}for(const n of rows){const b=document.createElement('button');b.type='button';b.textContent='Quên '+n.ssid+(n.preferred?' · đang ưu tiên':'');b.onclick=()=>forgetWifi(n.ssid);box.appendChild(b)}}
async function loadNetworks(){try{const r=await fetch('/api/networks',{cache:'no-store'});renderNetworks(await r.json())}catch(e){document.getElementById('networks').innerHTML='<div class="empty">Không tải được danh sách. Có thể nhập SSID thủ công.</div>'}}
async function rescan(){document.getElementById('networks').innerHTML='<div class="empty">Đang quét lại…</div>';await fetch('/api/scan',{cache:'no-store'});setTimeout(loadNetworks,500)}
async function forgetWifi(ssid){if(!confirm('Quên mạng '+ssid+'?'))return;const body='csrf='+encodeURIComponent(csrf)+'&ssid='+encodeURIComponent(ssid);await fetch('/api/forget',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});rescan()}
async function resetWifi(){if(!confirm('Quên toàn bộ mạng Wi-Fi đã lưu?'))return;const body='csrf='+encodeURIComponent(csrf);await fetch('/api/reset-wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});document.getElementById('ssid').value='';document.getElementById('pass').value='';rescan()}
function refreshReview(){const s=document.getElementById('ssid').value||'—';const h=document.getElementById('host').value||'—';const m=document.getElementById('minutes');document.getElementById('rWifi').textContent=s;document.getElementById('rHost').textContent=h;document.getElementById('rInterval').textContent=m.options[m.selectedIndex].text;document.getElementById('rOta').textContent=document.getElementById('otaEnabled').checked?'Bật':'Tắt'}
function finalise(){refreshReview();document.getElementById('busy').classList.add('on');return true}
loadNetworks();refreshReview();
</script></body></html>)HTML";

  page.replace("%VERSION%", hp20::version::STRING);
  page.replace("%CSRF%", csrf);
  page.replace("%SSID%", current ? escape(current->ssid) : "");
  page.replace("%WIFI_STATUS%", storedWifi);
  page.replace("%TOKEN_STATUS%", escape(storedToken));
  page.replace("%HOST%", current ? escape(current->host) : "thingsboard.cloud");
  page.replace("%CA_STATUS%", caStatus);
  page.replace("%OTA_CHECKED%", current && current->otaEnabled ? "checked" : "");
  page.replace("%REMINDER_CHECKED%", current && current->reminder ? "checked" : "");
  page.replace("%SOUND_CHECKED%", current && current->sound ? "checked" : "");
  page.replace("%THRESHOLD%", current ? String(current->threshold, 1) : "35.0");

  const int minutes = current ? int(current->intervalSeconds / 60) : 5;
  const int otaHours = current ? int(current->otaCheckSeconds / 3600) : 6;
  page.replace("<option value=\"" + String(minutes) + "\">",
               "<option value=\"" + String(minutes) + "\" selected>");
  page.replace("<option value=\"" + String(otaHours) + "\">",
               "<option value=\"" + String(otaHours) + "\" selected>");

  reply(200, page);
}

void save() {
  if (!permitted() || server.arg("csrf") != csrf || !current) {
    reply(403, "Phiên không hợp lệ.");
    return;
  }
  if (server.header("Content-Length").toInt() > 10000) {
    reply(413, "Dữ liệu quá lớn.");
    return;
  }

  Config next = *current;
  String nextSsid = server.arg("ssid");
  nextSsid.trim();
  String pass = server.arg("pass");

  if (nextSsid.isEmpty() || nextSsid.length() > 32 || pass.length() > 63 ||
      (!pass.isEmpty() && pass.length() < 8)) {
    reply(400, "Kiểm tra tên và mật khẩu Wi-Fi.");
    return;
  }

  const bool openNetwork = server.hasArg("open");
  String effectivePassword;
  if (openNetwork) {
    effectivePassword = "";
  } else if (!pass.isEmpty()) {
    effectivePassword = pass;
  } else {
    effectivePassword = wifiPasswordFor(next, nextSsid);
    if (effectivePassword.isEmpty()) {
      reply(400, "Mạng này chưa được nhớ. Hãy nhập mật khẩu hoặc chọn “mạng không có mật khẩu”.");
      return;
    }
  }
  rememberWifiProfile(next, nextSsid, effectivePassword, true);

  next.host = server.arg("host");
  next.host.trim();
  if (!hostname(next.host)) {
    reply(400, "Chỉ nhập hostname, không có https://, cổng hoặc đường dẫn.");
    return;
  }

  String token = server.arg("token");
  token.trim();
  if (token.length() > 128) {
    reply(400, "Token quá dài.");
    return;
  }
  for (unsigned i = 0; i < token.length(); ++i) {
    if (!isalnum((unsigned char)token[i]) && token[i] != '-' && token[i] != '_') {
      reply(400, "Token chứa ký tự không hỗ trợ.");
      return;
    }
  }
  if (server.hasArg("clearToken")) next.token = "";
  else if (!token.isEmpty()) next.token = token;

  String ca = server.arg("ca");
  ca.trim();
  if (!ca.isEmpty()) {
    if (ca.length() > 5998) {
      reply(400, "CA quá dài.");
      return;
    }
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);
    const int rc = mbedtls_x509_crt_parse(
      &cert, (const unsigned char*)ca.c_str(), ca.length() + 1
    );
    #if defined(MBEDTLS_PRIVATE)
      const bool isCa = cert.MBEDTLS_PRIVATE(ca_istrue) != 0;
    #else
      const bool isCa = cert.ca_istrue != 0;
    #endif
    mbedtls_x509_crt_free(&cert);
    if (rc != 0 || !isCa) {
      reply(400, "Cần chứng chỉ CA gốc PEM hợp lệ.");
      return;
    }
    next.ca = ca;
  }

  const long minutes = server.arg("minutes").toInt();
  const float threshold = server.arg("threshold").toFloat();
  const long otaHours = server.arg("otaHours").toInt();
  if (minutes < 5 || minutes > 1440 || !isfinite(threshold) ||
      threshold < 27 || threshold > 60 || otaHours < 1 || otaHours > 24) {
    reply(400, "Chu kỳ hoặc ngưỡng không hợp lệ.");
    return;
  }

  next.intervalSeconds = uint32_t(minutes) * 60U;
  next.threshold = threshold;
  next.reminder = server.hasArg("reminder");
  next.sound = server.hasArg("sound");
  next.otaEnabled = server.hasArg("otaEnabled");
  next.otaCheckSeconds = uint32_t(otaHours) * 3600U;

  if (!saveConfig(next)) {
    reply(500, "Không lưu được. Cấu hình cũ được giữ lại.");
    return;
  }

  *current = next;
  saved = true;
  started = millis();
  portalSetStatus(PortalPhase::Saved, 12, "Đã lưu cấu hình",
                  "HP20 đang chuẩn bị kết nối Wi-Fi.", next.ssid);
  reply(200, waitPage());
}

void registerRoutes() {
  if (routes) return;

  const char* headers[] = {"Content-Length"};
  server.collectHeaders(headers, 1);

  server.on("/", HTTP_GET, home);
  server.on("/save", HTTP_POST, save);
  server.on("/api/networks", HTTP_GET, apiNetworks);
  server.on("/api/scan", HTTP_GET, apiScan);
  server.on("/api/status", HTTP_GET, apiStatus);
  server.on("/api/forget", HTTP_POST, apiForget);
  server.on("/api/reset-wifi", HTTP_POST, apiResetWifi);

  // Common captive portal probes used by Android, iOS/macOS and Windows.
  server.on("/generate_204", HTTP_GET, captiveRedirect);
  server.on("/gen_204", HTTP_GET, captiveRedirect);
  server.on("/hotspot-detect.html", HTTP_GET, captiveRedirect);
  server.on("/library/test/success.html", HTTP_GET, captiveRedirect);
  server.on("/ncsi.txt", HTTP_GET, captiveRedirect);
  server.on("/connecttest.txt", HTTP_GET, captiveRedirect);
  server.on("/connecttest.htm", HTTP_GET, captiveRedirect);
  server.on("/redirect", HTTP_GET, captiveRedirect);

  server.onNotFound(captiveRedirect);
  routes = true;
}

} // namespace

void portalBegin(Config* c) {
  current = c;
  if (active) {
    started = millis();
    if (!scanRunning && scanCount == 0) beginScan();
    return;
  }

  registerRoutes();

  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06lx",
           (unsigned long)(ESP.getEfuseMac() & 0xffffff));
  name = "HP20-" + String(suffix);
  csrf = randomHex();

  // v0.9.15: no throw-away AP password. Security boundary is physical setup
  // intent: first provisioning or local BOOT hold. The application no longer
  // opens this AP automatically after ordinary transient Wi-Fi loss.
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(name.c_str())) return;

  dns.start(53, "*", WiFi.softAPIP());
  server.begin();
  started = millis();
  active = true;

  portalSetStatus(PortalPhase::Ready, 0, "Sẵn sàng cài đặt",
                  "Điện thoại sẽ tự mở trang cài đặt; 192.168.4.1 là phương án dự phòng.");
  beginScan();
}

void portalTick() {
  if (!active) return;

  harvestScan();
  dns.processNextRequest();
  server.handleClient();

  // First-time provisioning stays available until completed. A manually opened
  // portal on an already provisioned device still times out normally.
  const bool hasRememberedWifi = current && current->wifiProfileCount > 0;
  if (hasRememberedWifi &&
      statusView.phase != PortalPhase::Success &&
      model::elapsed(millis(), started, settings::PORTAL_TIMEOUT_MS)) {
    portalClose();
  }
}

bool portalSaved() {
  const bool result = saved;
  saved = false;
  return result;
}

bool portalActive() { return active; }

void portalCancelScan() {
  if (scanRunning || WiFi.scanComplete() >= 0) WiFi.scanDelete();
  scanRunning = false;
}

void portalClose() {
  if (!active) return;
  portalCancelScan();
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  active = false;
  statusView = PortalRuntimeStatus();
}

String portalName() { return name; }
String portalPassword() { return String(); }
IPAddress portalIp() { return WiFi.softAPIP(); }

PortalPhase portalPhase() { return statusView.phase; }
uint8_t portalProgress() { return statusView.progress; }
String portalStatusTitle() { return statusView.title; }
String portalStatusDetail() { return statusView.detail; }
String portalStatusSsid() { return statusView.ssid; }
String portalStatusIp() { return statusView.ip; }

void portalSetStatus(PortalPhase phase,
                     uint8_t progress,
                     const String& title,
                     const String& detail,
                     const String& ssid,
                     const String& ip) {
  statusView.phase = phase;
  statusView.progress = progress > 100 ? 100 : progress;
  statusView.title = title;
  statusView.detail = detail;
  statusView.ssid = ssid;
  statusView.ip = ip;
}
