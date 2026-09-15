#include "portal.h"
#include "thingsboard_ca.h"

#include "hp20_version.h"
#include "model.h"
#include "settings.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <mbedtls/x509_crt.h>

static WebServer server(80);
static DNSServer dns;
static Config* current = nullptr;
static bool active = false, saved = false, routes = false;
static uint32_t started = 0;
static String name, password, csrf;

static String randomHex() {
  char b[17];
  snprintf(b, sizeof(b), "%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random());
  return b;
}

static bool permitted() {
  return active && server.client().localIP() == WiFi.softAPIP();
}

static String escape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

static String tokenHint(const String& token) {
  if (token.isEmpty()) return "Chưa có token";
  const int n = token.length();
  return String("Đã lưu ••••") + token.substring(max(0, n - 4));
}

static void reply(int code, const String& text) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader(
    "Content-Security-Policy",
    "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
    "form-action 'self'; frame-ancestors 'none'"
  );
  server.send(code, "text/html; charset=utf-8", text);
}

static String wifiOptions() {
  String out;
  const int count = WiFi.scanNetworks(false, true);
  if (count <= 0) return out;

  // Datalist keeps the UI simple: select a discovered SSID or type one manually.
  for (int i = 0; i < count && i < 18; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    out += "<option value='" + escape(ssid) + "'>";
    out += String(WiFi.RSSI(i)) + " dBm</option>";
  }
  WiFi.scanDelete();
  return out;
}

static void home() {
  if (!permitted()) {
    reply(403, "Connect to the HP20 setup Wi-Fi.");
    return;
  }

  const String storedWifi = current && !current->ssid.isEmpty()
                          ? String("Đang lưu: ") + escape(current->ssid)
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
:root{color-scheme:dark;--bg:#071319;--card:#10232b;--line:#274650;--muted:#9fb6ba;--text:#eef7f5;--accent:#72dfbd;--accent2:#193c36;--warn:#ffd479}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(180deg,#071319,#0b1c23);color:var(--text);font:15px system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:560px;margin:auto;padding:18px 16px 34px}.brand{display:flex;align-items:center;justify-content:space-between;margin:4px 0 18px}.brand b{font-size:20px}.pill{padding:6px 10px;border:1px solid var(--line);border-radius:999px;color:var(--muted);font-size:12px}
.hero{margin-bottom:18px}.hero h1{font-size:28px;margin:0 0 8px}.hero p{color:var(--muted);line-height:1.45;margin:0}
.steps{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;margin:18px 0}.step{border:1px solid var(--line);border-radius:12px;padding:9px 6px;text-align:center;color:var(--muted);font-size:12px}.step.on{background:var(--accent2);color:var(--accent);border-color:#3c8272}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:18px;box-shadow:0 12px 35px #0005}.pane{display:none}.pane.on{display:block}.kicker{color:var(--accent);font-size:12px;font-weight:700;letter-spacing:.08em}.card h2{font-size:22px;margin:5px 0 4px}.sub{color:var(--muted);margin:0 0 16px;line-height:1.4}
label{display:block;margin:15px 0 6px;font-weight:650}.field{position:relative}input,textarea,button,select{width:100%;font:inherit;border-radius:12px;border:1px solid #365761;background:#0b1b22;color:var(--text);padding:13px 44px 13px 13px;outline:none}input:focus,textarea:focus{border-color:var(--accent);box-shadow:0 0 0 3px #72dfbd1c}textarea{min-height:115px;resize:vertical;padding-right:13px}.eye{position:absolute;right:5px;top:5px;width:40px;height:38px;padding:0;border:0;background:transparent;color:var(--muted);font-size:18px}.hint{display:flex;justify-content:space-between;gap:8px;color:var(--muted);font-size:12px;margin-top:6px}.ok{color:var(--accent)}.warn{color:var(--warn)}
.actions{display:flex;gap:10px;margin-top:22px}.actions button{padding:13px}.primary{background:var(--accent);color:#062019;border-color:transparent;font-weight:800}.secondary{background:transparent;color:var(--text)}
.review{display:grid;gap:10px}.row{display:flex;justify-content:space-between;gap:12px;border-bottom:1px solid var(--line);padding:10px 0}.row span:first-child{color:var(--muted)}details{margin-top:16px;border-top:1px solid var(--line);padding-top:14px}summary{cursor:pointer;color:var(--muted)}
.check{display:flex;gap:10px;align-items:flex-start;margin:14px 0}.check input{width:auto;margin-top:3px}.check label{margin:0;font-weight:500}.note{border:1px solid #48612d;background:#1c2616;padding:11px 12px;border-radius:12px;color:#dbe8c8;font-size:13px;line-height:1.4}
footer{color:var(--muted);font-size:12px;text-align:center;margin-top:16px;line-height:1.5}
</style></head><body><main>
<div class="brand"><b>HP20</b><span class="pill">v%VERSION%</span></div>
<div class="hero"><h1>Thiết lập thiết bị</h1><p>Một lần để kết nối. Cấu hình được lưu trong ESP32 và vẫn còn sau khi mất điện.</p></div>
<div class="steps"><div id="s1" class="step on">1 · Wi-Fi</div><div id="s2" class="step">2 · Cloud</div><div id="s3" class="step">3 · Xác nhận</div></div>
<form action="/save" method="post" onsubmit="return finalise()"><input type="hidden" name="csrf" value="%CSRF%">
<div class="card">
<section id="p1" class="pane on"><div class="kicker">BƯỚC 1 / 3</div><h2>Kết nối Wi-Fi</h2><p class="sub">Chọn mạng 2.4 GHz hoặc nhập thủ công.</p>
<label>Tên Wi-Fi</label><input id="ssid" name="ssid" list="nets" maxlength="32" required value="%SSID%" oninput="refreshReview()"><datalist id="nets">%NETWORKS%</datalist><div class="hint"><span>%WIFI_STATUS%</span><span>2.4 GHz</span></div>
<label>Mật khẩu Wi-Fi</label><div class="field"><input id="pass" name="pass" type="password" maxlength="63" autocomplete="new-password"><button class="eye" type="button" onclick="toggleSecret('pass',this)" aria-label="Hiện mật khẩu">◉</button></div><div class="hint"><span>Để trống = giữ mật khẩu đang lưu</span><span></span></div>
<div class="check"><input id="open" name="open" type="checkbox"><label for="open">Mạng này không có mật khẩu</label></div>
<div class="actions"><button class="primary" type="button" onclick="go(2)">Tiếp tục</button></div></section>

<section id="p2" class="pane"><div class="kicker">BƯỚC 2 / 3</div><h2>ThingsBoard Cloud</h2><p class="sub">Token xác định đúng thiết bị HP20. Token đã lưu không bao giờ được hiện lại đầy đủ.</p>
<label>Device access token</label><div class="field"><input id="token" name="token" type="password" maxlength="128" autocomplete="new-password"><button class="eye" type="button" onclick="toggleSecret('token',this)" aria-label="Hiện token">◉</button></div><div class="hint"><span class="ok">%TOKEN_STATUS%</span><span>Để trống = giữ token cũ</span></div>
<div class="check"><input id="clearToken" name="clearToken" type="checkbox"><label for="clearToken">Xóa token và ngừng gửi cloud</label></div>
<details><summary>Cài đặt nâng cao</summary>
<label>ThingsBoard host</label><input id="host" name="host" maxlength="127" value="%HOST%" oninput="refreshReview()"><div class="hint"><span>Chỉ hostname, không nhập https://</span></div>
<label>Chu kỳ gửi telemetry</label><select id="minutes" name="minutes" onchange="refreshReview()"><option value="15">15 phút</option><option value="30">30 phút</option><option value="60">60 phút</option><option value="180">3 giờ</option><option value="360">6 giờ</option><option value="1440">24 giờ</option></select>
<label>CA TLS tùy chỉnh (chỉ cho server riêng)</label><textarea name="ca" maxlength="5999" placeholder="-----BEGIN CERTIFICATE-----"></textarea><div class="hint"><span>%CA_STATUS%</span><span>ThingsBoard Cloud: không cần nhập CA</span></div>
<div class="check"><input id="otaEnabled" name="otaEnabled" type="checkbox" %OTA_CHECKED%><label for="otaEnabled"><b>Cho phép cập nhật firmware từ xa qua ThingsBoard OTA</b><br><small>Chỉ nhận firmware có title HP20, version mới hơn và SHA-256 hợp lệ.</small></label></div>
<label>Kiểm tra bản cập nhật</label><select id="otaHours" name="otaHours"><option value="1">Mỗi 1 giờ</option><option value="3">Mỗi 3 giờ</option><option value="6">Mỗi 6 giờ</option><option value="12">Mỗi 12 giờ</option><option value="24">Mỗi 24 giờ</option></select>
</details>
<div class="actions"><button class="secondary" type="button" onclick="go(1)">Quay lại</button><button class="primary" type="button" onclick="go(3)">Xác nhận</button></div></section>

<section id="p3" class="pane"><div class="kicker">BƯỚC 3 / 3</div><h2>Kiểm tra trước khi lưu</h2><p class="sub">HP20 sẽ thử kết nối ngay sau khi lưu. Nếu thất bại, AP cài đặt vẫn còn để sửa.</p><div class="review"><div class="row"><span>Wi-Fi</span><b id="rWifi">—</b></div><div class="row"><span>ThingsBoard</span><b id="rHost">—</b></div><div class="row"><span>Token</span><b>%TOKEN_STATUS%</b></div><div class="row"><span>Gửi dữ liệu</span><b id="rInterval">—</b></div><div class="row"><span>OTA</span><b id="rOta">—</b></div></div>
<details><summary>Nhắc làm mát</summary><div class="check"><input name="reminder" id="reminder" type="checkbox" %REMINDER_CHECKED%><label for="reminder">Bật nhắc khi FEEL vượt ngưỡng liên tục</label></div><label>Ngưỡng FEEL (°C)</label><input name="threshold" type="number" min="27" max="60" step="0.5" value="%THRESHOLD%"><div class="check"><input name="sound" id="sound" type="checkbox" %SOUND_CHECKED%><label for="sound">Cho phép còi nhắc</label></div></details>
<div class="note">Mật khẩu Wi-Fi và token thật không được gửi lên GitHub. Chúng chỉ nằm trong NVS của ESP32 hoặc file local <b>secrets.h</b> bị Git bỏ qua.</div>
<div class="actions"><button class="secondary" type="button" onclick="go(2)">Quay lại</button><button class="primary" type="submit">Lưu & kết nối</button></div></section>
</div></form><footer>Giữ BOOT 3 giây để mở lại cài đặt bất cứ lúc nào.<br>AP cài đặt tự đóng sau thời gian chờ.</footer>
<script>
function go(n){for(let i=1;i<=3;i++){document.getElementById('p'+i).classList.toggle('on',i===n);document.getElementById('s'+i).classList.toggle('on',i===n)}if(n===3)refreshReview();scrollTo(0,0)}
function toggleSecret(id,b){const e=document.getElementById(id);e.type=e.type==='password'?'text':'password';b.textContent=e.type==='password'?'◉':'◎'}
function refreshReview(){const s=document.getElementById('ssid').value||'—';const h=document.getElementById('host').value||'—';const m=document.getElementById('minutes');document.getElementById('rWifi').textContent=s;document.getElementById('rHost').textContent=h;document.getElementById('rInterval').textContent=m.options[m.selectedIndex].text;document.getElementById('rOta').textContent=document.getElementById('otaEnabled').checked?'Bật':'Tắt'}
function finalise(){refreshReview();return true}
refreshReview();
</script></main></body></html>)HTML";

  page.replace("%VERSION%", hp20::version::STRING);
  page.replace("%CSRF%", csrf);
  page.replace("%SSID%", current ? escape(current->ssid) : "");
  page.replace("%NETWORKS%", wifiOptions());
  page.replace("%WIFI_STATUS%", storedWifi);
  page.replace("%TOKEN_STATUS%", escape(storedToken));
  page.replace("%HOST%", current ? escape(current->host) : "thingsboard.cloud");
  page.replace("%CA_STATUS%", caStatus);
  page.replace("%OTA_CHECKED%", current && current->otaEnabled ? "checked" : "");
  page.replace("%REMINDER_CHECKED%", current && current->reminder ? "checked" : "");
  page.replace("%SOUND_CHECKED%", current && current->sound ? "checked" : "");
  page.replace("%THRESHOLD%", current ? String(current->threshold, 1) : "35.0");

  // Restore stored select values without adding another server-side template engine.
  const int minutes = current ? int(current->intervalSeconds / 60) : 15;
  const int otaHours = current ? int(current->otaCheckSeconds / 3600) : 6;
  page.replace("<option value=\"" + String(minutes) + "\">",
               "<option value=\"" + String(minutes) + "\" selected>");
  page.replace("<option value=\"" + String(otaHours) + "\">",
               "<option value=\"" + String(otaHours) + "\" selected>");

  reply(200, page);
}

static bool hostname(const String& h) {
  if (h.isEmpty()) return true;
  if (h.length() > 127 || h.indexOf('.') < 0) return false;
  for (unsigned i = 0; i < h.length(); ++i) {
    if (!isalnum((unsigned char)h[i]) && h[i] != '.' && h[i] != '-') return false;
  }
  return h[0] != '.' && h[h.length() - 1] != '.';
}

static void save() {
  if (!permitted() || server.arg("csrf") != csrf) {
    reply(403, "Phiên không hợp lệ.");
    return;
  }
  if (server.header("Content-Length").toInt() > 10000) {
    reply(413, "Dữ liệu quá lớn.");
    return;
  }

  Config next = *current;
  next.ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (next.ssid.isEmpty() || next.ssid.length() > 32 || pass.length() > 63 ||
      (!pass.isEmpty() && pass.length() < 8)) {
    reply(400, "Kiểm tra tên và mật khẩu Wi-Fi.");
    return;
  }
  if (server.hasArg("open")) next.password = "";
  else if (!pass.isEmpty()) next.password = pass;
  else if (next.ssid != current->ssid) {
    reply(400, "Nhập mật khẩu cho Wi-Fi mới hoặc chọn mạng không mật khẩu.");
    return;
  }

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
  if (minutes < 15 || minutes > 1440 || !isfinite(threshold) ||
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

  String done = R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font:16px system-ui;background:#071319;color:#eef7f5;padding:26px}main{max-width:480px;margin:auto;background:#10232b;border:1px solid #274650;border-radius:18px;padding:22px}h1{color:#72dfbd}p{color:#b5c9cb;line-height:1.5}a{display:block;margin-top:20px;padding:13px;text-align:center;border-radius:12px;background:#72dfbd;color:#062019;text-decoration:none;font-weight:800}</style><main><h1>Đã lưu cấu hình</h1><p>HP20 đang thử kết nối Wi-Fi và ThingsBoard. Theo dõi màn hình OLED. Nếu thông tin chưa đúng, AP cài đặt vẫn mở trong thời gian chuyển tiếp.</p><p><b>Mất điện không làm mất cấu hình.</b> Thiết bị sẽ dùng lại NVS ở lần khởi động sau.</p><a href='/'>Xem lại cài đặt</a></main></html>)HTML";
  reply(200, done);
}

void portalBegin(Config* c) {
  current = c;
  if (active) {
    started = millis();
    return;
  }

  if (!routes) {
    const char* headers[] = {"Content-Length"};
    server.collectHeaders(headers, 1);
    server.on("/", HTTP_GET, home);
    server.on("/save", HTTP_POST, save);
    server.onNotFound(home);
    routes = true;
  }

  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06lx",
           (unsigned long)(ESP.getEfuseMac() & 0xffffff));
  name = "HP20-" + String(suffix);
  password = randomHex().substring(0, 12);
  csrf = randomHex();

  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(name.c_str(), password.c_str())) return;
  dns.start(53, "*", WiFi.softAPIP());
  server.begin();
  started = millis();
  active = true;
}

void portalTick() {
  if (!active) return;
  dns.processNextRequest();
  server.handleClient();
  if (model::elapsed(millis(), started, settings::PORTAL_TIMEOUT_MS)) portalClose();
}

bool portalSaved() {
  const bool result = saved;
  saved = false;
  return result;
}

bool portalActive() { return active; }

void portalClose() {
  if (!active) return;
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  active = false;
}

String portalName() { return name; }
String portalPassword() { return password; }
