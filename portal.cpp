#include "portal.h"
#include "thingsboard_ca.h"

#include "hp20_version.h"
#include "model.h"
#include "settings.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_wifi_types.h>
#include <mbedtls/x509_crt.h>

static WebServer server(80);
static DNSServer dns;
static Config* current = nullptr;
static bool active = false, saved = false, routes = false;
static uint32_t started = 0;
static uint32_t saveRequestedAt = 0;
static String name, csrf;

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
  if (token.isEmpty()) return "Chưa liên kết cloud";
  const int n = token.length();
  return String("Đã lưu ••••") + token.substring(max(0, n - 4));
}

static void securityHeaders() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Referrer-Policy", "no-referrer");
  server.sendHeader(
    "Content-Security-Policy",
    "default-src 'none'; connect-src 'self'; style-src 'unsafe-inline'; "
    "script-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'"
  );
}

static void reply(int code, const String& text) {
  securityHeaders();
  server.send(code, "text/html; charset=utf-8", text);
}

static void redirectHome() {
  if (!active) {
    reply(503, "HP20 setup is closed.");
    return;
  }
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.sendHeader("Cache-Control", "no-store");
  server.send(302, "text/plain", "");
}

struct NetItem {
  String ssid;
  int32_t rssi;
  bool open;
};

static const char* signalLabel(int32_t rssi) {
  if (rssi >= -55) return "Rất tốt";
  if (rssi >= -67) return "Tốt";
  if (rssi >= -75) return "Trung bình";
  return "Yếu";
}

static const char* signalClass(int32_t rssi) {
  if (rssi >= -67) return "good";
  if (rssi >= -75) return "mid";
  return "weak";
}

static String wifiCards() {
  NetItem nets[18];
  int used = 0;
  const int count = WiFi.scanNetworks(false, true);

  if (count > 0) {
    for (int i = 0; i < count; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;

      int existing = -1;
      for (int j = 0; j < used; ++j) {
        if (nets[j].ssid == ssid) {
          existing = j;
          break;
        }
      }

      const int32_t rssi = WiFi.RSSI(i);
      const bool isOpen = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;

      if (existing >= 0) {
        if (rssi > nets[existing].rssi) {
          nets[existing].rssi = rssi;
          nets[existing].open = isOpen;
        }
      } else if (used < 18) {
        nets[used++] = {ssid, rssi, isOpen};
      }
    }
  }
  WiFi.scanDelete();

  // Strongest networks first. The user should not have to interpret RSSI numbers.
  for (int i = 0; i < used - 1; ++i) {
    int best = i;
    for (int j = i + 1; j < used; ++j) {
      if (nets[j].rssi > nets[best].rssi) best = j;
    }
    if (best != i) {
      NetItem t = nets[i];
      nets[i] = nets[best];
      nets[best] = t;
    }
  }

  if (used == 0) {
    return "<div class='empty'>Chưa tìm thấy mạng. Có thể nhập tên Wi-Fi thủ công bên dưới.</div>";
  }

  String out;
  for (int i = 0; i < used; ++i) {
    const String ssid = escape(nets[i].ssid);
    const bool remembered = current && current->ssid == nets[i].ssid;
    out += "<button type='button' class='net' data-ssid=\"" + ssid + "\" data-open='" +
           String(nets[i].open ? "1" : "0") + "' onclick='chooseNet(this)'>";
    out += "<span class='netmain'><b>" + ssid + "</b><small>";
    out += nets[i].open ? "Không mật khẩu" : "Có mật khẩu";
    if (remembered) out += " · Đã nhớ";
    out += "</small></span>";
    out += "<span class='signal " + String(signalClass(nets[i].rssi)) + "'>";
    out += signalLabel(nets[i].rssi);
    out += "<small>" + String(nets[i].rssi) + " dBm</small></span></button>";
  }
  return out;
}

static void home() {
  if (!permitted()) {
    reply(403, "Hãy kết nối vào Wi-Fi cài đặt HP20.");
    return;
  }

  const String storedWifi = current && !current->ssid.isEmpty()
                          ? String("Đã nhớ: ") + escape(current->ssid)
                          : "Chưa lưu Wi-Fi";
  const String storedToken = current ? tokenHint(current->token) : "Chưa liên kết cloud";

  String caStatus = "Chưa có CA TLS tùy chỉnh";
  if (current) {
    if (!current->ca.isEmpty()) caStatus = "CA TLS tùy chỉnh đã lưu";
    else if (hp20::tbtrust::isDefaultCloudHost(current->host))
      caStatus = "ThingsBoard Cloud: chứng chỉ tin cậy có sẵn trong firmware";
  }

  String page = R"HTML(<!doctype html><html lang="vi"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>HP20 · Kết nối</title>
<style>
:root{color-scheme:dark;--bg:#071319;--card:#10232b;--line:#274650;--muted:#9fb6ba;--text:#eef7f5;--accent:#72dfbd;--accent2:#173b35;--warn:#ffd479;--bad:#ff9b9b}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(180deg,#071319,#0b1c23);color:var(--text);font:15px system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:590px;margin:auto;padding:18px 16px 38px}.brand{display:flex;align-items:center;justify-content:space-between;margin:4px 0 14px}.brand b{font-size:20px}.pill{padding:6px 10px;border:1px solid var(--line);border-radius:999px;color:var(--muted);font-size:12px}
.hero h1{font-size:28px;margin:0 0 7px}.hero p{margin:0;color:var(--muted);line-height:1.45}.card{margin-top:18px;background:var(--card);border:1px solid var(--line);border-radius:20px;padding:18px;box-shadow:0 12px 35px #0005}
.kicker{color:var(--accent);font-size:12px;font-weight:800;letter-spacing:.08em}.card h2{font-size:21px;margin:5px 0 4px}.sub{color:var(--muted);margin:0 0 14px;line-height:1.4}.saved{display:inline-block;margin-top:8px;color:var(--accent);font-size:12px}
.networks{display:grid;gap:8px;margin:12px 0}.net{display:flex;justify-content:space-between;align-items:center;gap:12px;width:100%;padding:12px;border:1px solid #31525b;background:#0b1b22;color:var(--text);border-radius:14px;text-align:left}.net:hover,.net.sel{border-color:var(--accent);background:#102c2b}.netmain{min-width:0}.netmain b{display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.netmain small,.signal small{display:block;color:var(--muted);margin-top:3px;font-size:11px}.signal{text-align:right;font-weight:800;font-size:12px;white-space:nowrap}.signal.good{color:var(--accent)}.signal.mid{color:var(--warn)}.signal.weak{color:var(--bad)}.empty{padding:12px;border:1px dashed var(--line);border-radius:12px;color:var(--muted)}
label{display:block;margin:15px 0 6px;font-weight:650}.field{position:relative}input,textarea,select,button{font:inherit}input,textarea,select{width:100%;border-radius:12px;border:1px solid #365761;background:#0b1b22;color:var(--text);padding:13px 44px 13px 13px;outline:none}input:focus,textarea:focus,select:focus{border-color:var(--accent);box-shadow:0 0 0 3px #72dfbd1c}textarea{min-height:105px;resize:vertical;padding-right:13px}.eye{position:absolute;right:5px;top:5px;width:40px;height:38px;border:0;background:transparent;color:var(--muted);font-size:18px}.hint{display:flex;justify-content:space-between;gap:8px;color:var(--muted);font-size:12px;margin-top:6px}.check{display:flex;align-items:flex-start;gap:9px;margin:12px 0}.check input{width:auto;margin-top:3px}.check label{margin:0;font-weight:500}.actions{margin-top:20px}.primary{width:100%;border:0;border-radius:13px;padding:14px;background:var(--accent);color:#062019;font-weight:850}.primary:disabled{opacity:.55}.secondary{width:100%;border:1px solid var(--line);border-radius:12px;padding:11px;background:transparent;color:var(--text)}
details{margin-top:18px;border-top:1px solid var(--line);padding-top:15px}summary{cursor:pointer;color:var(--muted);font-weight:700}.advanced-note{margin:10px 0 0;padding:10px 11px;border-radius:12px;background:#0b1b22;color:var(--muted);font-size:12px;line-height:1.4}.toggleline{border:1px solid #31525b;border-radius:14px;padding:12px;margin:15px 0;background:#0b1b22}.toggleline .check{margin:0}.ok{color:var(--accent)}.warn{color:var(--warn)}
footer{text-align:center;color:var(--muted);font-size:12px;line-height:1.5;margin-top:16px}
</style></head><body><main>
<div class="brand"><b>HP20</b><span class="pill">v%VERSION%</span></div>
<div class="hero"><h1>Kết nối HP20</h1><p>Chọn Wi-Fi, nhập mật khẩu và lưu. HP20 sẽ tự nhớ sau khi mất điện.</p><span class="saved">%WIFI_STATUS%</span></div>
<form action="/save" method="post" onsubmit="return beginSave()"><input type="hidden" name="csrf" value="%CSRF%">
<div class="card"><div class="kicker">WI-FI GẦN ĐÂY</div><h2>Chọn mạng 2.4 GHz</h2><p class="sub">Mạng mạnh hơn được ưu tiên lên đầu. Chạm vào tên mạng để chọn.</p>
<div class="networks">%NETWORKS%</div>
<label>Tên Wi-Fi</label><input id="ssid" name="ssid" maxlength="32" required value="%SSID%" placeholder="Hoặc nhập tên mạng thủ công">
<label>Mật khẩu Wi-Fi</label><div class="field"><input id="pass" name="pass" type="password" maxlength="63" autocomplete="current-password" placeholder="Nhập mật khẩu Wi-Fi"><button class="eye" type="button" onclick="toggleSecret('pass',this)" aria-label="Hiện mật khẩu">◉</button></div>
<div class="hint"><span id="passHint">Để trống nếu dùng lại mạng đã nhớ</span><span>HP20 hỗ trợ 2.4 GHz</span></div>
<div class="check"><input id="open" name="open" type="checkbox"><label for="open">Mạng này không có mật khẩu</label></div>

<details><summary>Cài đặt nâng cao — ThingsBoard, chu kỳ gửi, OTA</summary>
<div class="advanced-note">Người dùng thông thường không cần sửa mục này. Token, CA và OTA là cấu hình kỹ thuật.</div>
<label>ThingsBoard host</label><input id="host" name="host" maxlength="127" value="%HOST%"><div class="hint"><span>Chỉ hostname, không nhập https://</span><span></span></div>
<label>Device access token</label><div class="field"><input id="token" name="token" type="password" maxlength="128" autocomplete="new-password" placeholder="Để trống = giữ token đang lưu"><button class="eye" type="button" onclick="toggleSecret('token',this)" aria-label="Hiện token">◉</button></div><div class="hint"><span class="ok">%TOKEN_STATUS%</span><span></span></div>
<div class="check"><input id="clearToken" name="clearToken" type="checkbox"><label for="clearToken">Xóa token và ngừng gửi ThingsBoard</label></div>
<label>Chu kỳ gửi dữ liệu lên ThingsBoard</label><select id="minutes" name="minutes"><option value="5">5 phút (chuẩn HP20)</option><option value="15">15 phút</option><option value="30">30 phút</option><option value="60">60 phút</option><option value="180">3 giờ</option><option value="360">6 giờ</option><option value="1440">24 giờ</option></select>
<label>CA TLS tùy chỉnh — chỉ server riêng</label><textarea name="ca" maxlength="5999" placeholder="-----BEGIN CERTIFICATE-----"></textarea><div class="hint"><span>%CA_STATUS%</span><span></span></div>
<div class="toggleline"><div class="check"><input id="otaEnabled" name="otaEnabled" type="checkbox" %OTA_CHECKED%><label for="otaEnabled"><b>Cho phép cập nhật firmware từ xa (OTA)</b><br><small>OTA = cập nhật phần mềm HP20 qua mạng, không cần cắm USB.</small></label></div></div>
<label>Chu kỳ kiểm tra bản cập nhật OTA</label><select id="otaMinutes" name="otaMinutes"><option value="15">15 phút</option><option value="30">30 phút</option><option value="60">1 giờ (khuyến nghị)</option><option value="180">3 giờ</option><option value="360">6 giờ</option><option value="720">12 giờ</option><option value="1440">24 giờ</option></select>
<label>Ngưỡng FEEL nhắc làm mát (°C)</label><input name="threshold" type="number" min="27" max="60" step="0.5" value="%THRESHOLD%">
<div class="check"><input name="reminder" id="reminder" type="checkbox" %REMINDER_CHECKED%><label for="reminder">Bật nhắc khi FEEL vượt ngưỡng liên tục</label></div>
<div class="check"><input name="sound" id="sound" type="checkbox" %SOUND_CHECKED%><label for="sound">Cho phép còi nhắc</label></div>
</details>
<div class="actions"><button id="saveBtn" class="primary" type="submit">Lưu & kết nối</button></div></div></form>
<footer>Giữ BOOT khoảng 3 giây để mở lại cài đặt bất cứ lúc nào.<br>Wi-Fi cài đặt HP20 không cần mật khẩu và tự đóng sau thời gian chờ.</footer>
<script>
function toggleSecret(id,b){const e=document.getElementById(id);e.type=e.type==='password'?'text':'password';b.textContent=e.type==='password'?'◉':'◎'}
function chooseNet(b){document.querySelectorAll('.net').forEach(x=>x.classList.remove('sel'));b.classList.add('sel');document.getElementById('ssid').value=b.dataset.ssid;const isOpen=b.dataset.open==='1';document.getElementById('open').checked=isOpen;document.getElementById('pass').disabled=isOpen;document.getElementById('passHint').textContent=isOpen?'Mạng mở — không cần mật khẩu':'Nhập mật khẩu Wi-Fi; để trống nếu là mạng đã nhớ'}
document.getElementById('open').addEventListener('change',e=>{document.getElementById('pass').disabled=e.target.checked});
function beginSave(){const b=document.getElementById('saveBtn');b.disabled=true;b.textContent='Đang lưu…';return true}
</script></main></body></html>)HTML";

  page.replace("%VERSION%", hp20::version::STRING);
  page.replace("%CSRF%", csrf);
  page.replace("%SSID%", current ? escape(current->ssid) : "");
  page.replace("%NETWORKS%", wifiCards());
  page.replace("%WIFI_STATUS%", storedWifi);
  page.replace("%TOKEN_STATUS%", escape(storedToken));
  page.replace("%HOST%", current ? escape(current->host) : "thingsboard.cloud");
  page.replace("%CA_STATUS%", caStatus);
  page.replace("%OTA_CHECKED%", current && current->otaEnabled ? "checked" : "");
  page.replace("%REMINDER_CHECKED%", current && current->reminder ? "checked" : "");
  page.replace("%SOUND_CHECKED%", current && current->sound ? "checked" : "");
  page.replace("%THRESHOLD%", current ? String(current->threshold, 1) : "35.0");

  const int minutes = current ? int(current->intervalSeconds / 60U) : 5;
  const int otaMinutes = current ? int(current->otaCheckSeconds / 60U) : 60;
  page.replace("<option value=\"" + String(minutes) + "\">",
               "<option value=\"" + String(minutes) + "\" selected>");
  page.replace("<option value=\"" + String(otaMinutes) + "\">",
               "<option value=\"" + String(otaMinutes) + "\" selected>");

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

static void statusJson() {
  if (!permitted()) {
    securityHeaders();
    server.send(403, "application/json", "{\"state\":\"closed\"}");
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint32_t age = saveRequestedAt ? (millis() - saveRequestedAt) : 0;
  const char* state = connected ? "connected" : (age < 20000U ? "connecting" : "retry");

  StaticJsonDocument<256> d;
  d["state"] = state;
  d["ssid"] = connected ? WiFi.SSID() : (current ? current->ssid : String(""));
  d["rssi"] = connected ? WiFi.RSSI() : 0;
  d["ip"] = connected ? WiFi.localIP().toString() : String("");
  String body;
  serializeJson(d, body);

  securityHeaders();
  server.send(200, "application/json; charset=utf-8", body);
}

static void save() {
  if (!permitted() || server.arg("csrf") != csrf) {
    reply(403, "Phiên cài đặt không hợp lệ.");
    return;
  }
  if (server.header("Content-Length").toInt() > 10000) {
    reply(413, "Dữ liệu quá lớn.");
    return;
  }

  Config next = *current;
  next.ssid = server.arg("ssid");
  next.ssid.trim();
  String pass = server.arg("pass");

  if (next.ssid.isEmpty() || next.ssid.length() > 32 || pass.length() > 63 ||
      (!pass.isEmpty() && pass.length() < 8)) {
    reply(400, "Kiểm tra tên và mật khẩu Wi-Fi.");
    return;
  }

  if (server.hasArg("open")) next.password = "";
  else if (!pass.isEmpty()) next.password = pass;
  else if (next.ssid != current->ssid) {
    reply(400, "Wi-Fi mới cần mật khẩu. Nếu là mạng mở, hãy chọn 'không có mật khẩu'.");
    return;
  }

  next.host = server.arg("host");
  next.host.trim();
  if (!hostname(next.host)) {
    reply(400, "ThingsBoard host chỉ gồm hostname, không nhập https://, cổng hoặc đường dẫn.");
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
  const long otaMinutes = server.arg("otaMinutes").toInt();
  const float threshold = server.arg("threshold").toFloat();

  if (minutes < 5 || minutes > 1440 || otaMinutes < 15 || otaMinutes > 1440 ||
      !isfinite(threshold) || threshold < 27 || threshold > 60) {
    reply(400, "Chu kỳ gửi, chu kỳ OTA hoặc ngưỡng không hợp lệ.");
    return;
  }

  next.intervalSeconds = uint32_t(minutes) * 60U;
  next.threshold = threshold;
  next.reminder = server.hasArg("reminder");
  next.sound = server.hasArg("sound");
  next.otaEnabled = server.hasArg("otaEnabled");
  next.otaCheckSeconds = uint32_t(otaMinutes) * 60U;

  if (!saveConfig(next)) {
    reply(500, "Không lưu được. Cấu hình cũ vẫn được giữ lại.");
    return;
  }

  *current = next;
  saved = true;
  saveRequestedAt = millis();

  String done = R"HTML(<!doctype html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><title>HP20 · Đang kết nối</title><style>
:root{color-scheme:dark;--bg:#071319;--card:#10232b;--line:#274650;--muted:#9fb6ba;--text:#eef7f5;--accent:#72dfbd;--warn:#ffd479;--bad:#ff9b9b}*{box-sizing:border-box}body{margin:0;background:linear-gradient(180deg,#071319,#0b1c23);color:var(--text);font:16px system-ui,-apple-system,Segoe UI,sans-serif;padding:22px}main{max-width:500px;margin:auto;background:var(--card);border:1px solid var(--line);border-radius:20px;padding:24px}.spinner{width:46px;height:46px;border:4px solid #274650;border-top-color:var(--accent);border-radius:50%;animation:s 1s linear infinite;margin:8px 0 20px}@keyframes s{to{transform:rotate(360deg)}}h1{margin:0 0 8px}p{color:var(--muted);line-height:1.5}.status{margin-top:18px;padding:13px;border:1px solid var(--line);border-radius:13px;background:#0b1b22}.ok{color:var(--accent)}.warn{color:var(--warn)}.bad{color:var(--bad)}a{display:block;margin-top:18px;padding:13px;text-align:center;border-radius:12px;background:var(--accent);color:#062019;text-decoration:none;font-weight:800}</style></head><body><main><div id="spin" class="spinner"></div><h1 id="title">Đã lưu — đang kết nối</h1><p id="msg">HP20 đang thử Wi-Fi vừa chọn. Đèn xanh dương trên bo mạch sẽ nháy nhanh trong lúc chờ.</p><div id="detail" class="status">Đang kiểm tra…</div><a id="back" href="/" style="display:none">Quay lại cài đặt</a></main><script>
let tries=0;async function poll(){tries++;try{const r=await fetch('/status',{cache:'no-store'});const d=await r.json();const box=document.getElementById('detail');if(d.state==='connected'){document.getElementById('spin').style.display='none';document.getElementById('title').textContent='Kết nối Wi-Fi thành công';document.getElementById('msg').textContent='HP20 đang xác nhận ThingsBoard. Nếu token hợp lệ, dữ liệu đầu tiên thường xuất hiện trong vài giây.';box.className='status ok';box.textContent='Wi-Fi: '+(d.ssid||'—')+' · '+(d.rssi||0)+' dBm · IP '+(d.ip||'—');return}if(d.state==='retry'){document.getElementById('title').textContent='Chưa kết nối được';document.getElementById('msg').textContent='Kiểm tra lại mật khẩu hoặc chắc chắn đây là Wi-Fi 2.4 GHz. HP20 vẫn giữ cổng cài đặt để bạn sửa.';box.className='status bad';box.textContent='Có thể quay lại và thử lại mà không cần cắm USB.';document.getElementById('back').style.display='block';return}box.className='status warn';box.textContent='Đang kết nối Wi-Fi… ('+tries+'s)'}catch(e){document.getElementById('detail').textContent='Điện thoại đang chuyển mạng. Hãy chờ HP20 hoàn tất kết nối.'}setTimeout(poll,1000)}setTimeout(poll,700);
</script></body></html>)HTML";
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
    server.on("/status", HTTP_GET, statusJson);

    // Common captive-portal probe endpoints used by Android/iOS/Windows.
    server.on("/generate_204", HTTP_GET, redirectHome);
    server.on("/gen_204", HTTP_GET, redirectHome);
    server.on("/hotspot-detect.html", HTTP_GET, redirectHome);
    server.on("/library/test/success.html", HTTP_GET, redirectHome);
    server.on("/connecttest.txt", HTTP_GET, redirectHome);
    server.on("/ncsi.txt", HTTP_GET, redirectHome);
    server.on("/fwlink", HTTP_GET, redirectHome);
    server.onNotFound(redirectHome);
    routes = true;
  }

  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06lx",
           (unsigned long)(ESP.getEfuseMac() & 0xffffff));
  name = "HP20-" + String(suffix);
  csrf = randomHex();
  saveRequestedAt = 0;

  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(name.c_str())) return;
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
