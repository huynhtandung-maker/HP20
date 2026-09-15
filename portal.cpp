#include "portal.h"
#include "settings.h"
#include "hp20_version.h"
#include "model.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <mbedtls/x509_crt.h>
static WebServer server(80);
static DNSServer dns;
static Config* current;
static bool active = false, saved = false, routes = false;
static uint32_t started;
static String name, password, csrf;
static String randomHex() { char b[17]; snprintf(b, sizeof(b), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random()); return b; }
static bool permitted() { return active && server.client().localIP() == WiFi.softAPIP(); }
static String escape(String s) {
  s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;");
  s.replace("\"", "&quot;"); s.replace("'", "&#39;"); return s;
}
static void reply(int code, const String& text) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
  server.send(code, "text/html; charset=utf-8", text);
}
static void home() {
  if (!permitted()) { reply(403, "Connect to the device setup Wi-Fi."); return; }
  String page = R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HP20 · Cài đặt</title>
<style>body{font:16px system-ui;background:#101b22;color:#edf6f5;margin:0;padding:24px}main{max-width:520px;margin:auto}h1{font-size:32px}p{color:#b8caca;line-height:1.5}label{display:block;margin:18px 0 6px}input,textarea,button{box-sizing:border-box;width:100%;padding:12px;border:1px solid #48636a;border-radius:10px;background:#182b34;color:white;font:inherit}input[type=checkbox]{width:auto}button{margin-top:24px;background:#8adac3;color:#10251f;font-weight:bold}small{color:#b8caca}fieldset{border:1px solid #48636a;border-radius:12px;margin-top:24px}</style>
<main><small>HP20 / ROOM MONITOR / %VERSION%</small><h1>Kết nối căn phòng</h1><p>Cấu hình được lưu trên ESP32 và giữ lại khi mất điện. Mật khẩu và token đã lưu không được hiển thị lại.</p><form action="/save" method="post">
)HTML";
  page.replace("%VERSION%", hp20::version::STRING);
  page += "<input type='hidden' name='csrf' value='" + csrf + "'>";
  page += "<label>Tên Wi-Fi 2.4 GHz</label><input name='ssid' maxlength='32' required value='" + escape(current->ssid) + "'>";
  page += "<label>Mật khẩu Wi-Fi mới</label><input name='pass' type='password' maxlength='63' autocomplete='new-password'><small>Để trống giữ mật khẩu cũ. Nếu đổi mạng, nhập mật khẩu mới.</small><label><input type='checkbox' name='open'> Mạng mới không có mật khẩu</label>";
  page += "<fieldset><legend>ThingsBoard · tùy chọn</legend><label>Hostname HTTPS</label><input name='host' maxlength='127' placeholder='thingsboard.cloud' value='" + escape(current->host) + "'>";
  page += "<label>Access token mới</label><input type='password' name='token' maxlength='128' autocomplete='new-password'><small>Để trống giữ token cũ.</small><label><input type='checkbox' name='clearToken'> Xóa token / ngừng gửi</label>";
  page += "<label>Chứng chỉ CA gốc PEM</label><textarea name='ca' maxlength='5999' rows='4' placeholder='-----BEGIN CERTIFICATE-----'></textarea><small>Dán CA tin cậy từ tài liệu máy chủ; để trống giữ CA cũ. Không dùng chứng chỉ tùy ý.</small>";
  page += "<label>Chu kỳ gửi (phút, 15–1440)</label><input name='minutes' type='number' min='15' max='1440' required value='" + String(current->intervalSeconds/60) + "'></fieldset>";
  page += "<fieldset><legend>Nhắc làm mát · tự chọn, không phải ngưỡng y tế</legend><label><input name='reminder' type='checkbox' " + String(current->reminder ? "checked" : "") + "> Bật nhắc khi cảm nhận vượt ngưỡng 2 phút</label>";
  page += "<label>Ngưỡng cảm nhận (°C)</label><input name='threshold' type='number' min='27' max='60' step='0.5' required value='" + String(current->threshold,1) + "'>";
  page += "<label><input name='sound' type='checkbox' " + String(current->sound ? "checked" : "") + "> Cho phép còi nhắc</label><small>Không áp dụng bảng phân loại nguy cơ của Mỹ/Châu Âu. Chưa có thang y học Việt Nam được xác thực cho thiết bị này.</small></fieldset><button>Lưu cấu hình</button></form><p>Giữ BOOT 3 giây để mở lại cài đặt. Trang này chỉ hoạt động qua Wi-Fi cấu hình của ESP32.</p></main></html>";
  reply(200, page);
}
static bool hostname(const String& h) {
  if (h.isEmpty()) return true;
  if (h.length()>127 || h.indexOf('.')<0) return false;
  for (unsigned i=0;i<h.length();++i) if (!isalnum((unsigned char)h[i]) && h[i]!='.' && h[i]!='-') return false;
  return h[0]!='.' && h[h.length()-1]!='.';
}
static void save() {
  if (!permitted() || server.arg("csrf") != csrf) { reply(403,"Phiên không hợp lệ."); return; }
  if (server.header("Content-Length").toInt()>10000) { reply(413,"Dữ liệu quá lớn."); return; }
  Config next = *current;
  next.ssid=server.arg("ssid"); String pass=server.arg("pass");
  if (next.ssid.isEmpty() || next.ssid.length()>32 || pass.length()>63 || (!pass.isEmpty() && pass.length()<8)) { reply(400,"Kiểm tra tên và mật khẩu Wi-Fi."); return; }
  if (server.hasArg("open")) next.password="";
  else if (!pass.isEmpty()) next.password=pass;
  else if (next.ssid!=current->ssid) { reply(400,"Nhập mật khẩu cho Wi-Fi mới hoặc chọn mạng không mật khẩu."); return; }
  next.host=server.arg("host"); next.host.trim();
  if (!hostname(next.host)) { reply(400,"Chỉ nhập hostname, không có https://, cổng hoặc đường dẫn."); return; }
  String token=server.arg("token"); token.trim();
  if (token.length()>128) { reply(400,"Token quá dài."); return; }
  for (unsigned i=0;i<token.length();i++) if (!isalnum((unsigned char)token[i]) && token[i]!='-' && token[i]!='_') { reply(400,"Token chứa ký tự không hỗ trợ."); return; }
  if (server.hasArg("clearToken")) next.token=""; else if (!token.isEmpty()) next.token=token;
  String ca=server.arg("ca"); ca.trim();
  if (!ca.isEmpty()) {
    if (ca.length()>5998) { reply(400,"CA quá dài."); return; }
    mbedtls_x509_crt cert; mbedtls_x509_crt_init(&cert);
    int rc=mbedtls_x509_crt_parse(&cert,(const unsigned char*)ca.c_str(),ca.length()+1);
    #if defined(MBEDTLS_PRIVATE)
        bool isCa = cert.MBEDTLS_PRIVATE(ca_istrue) != 0;
    #else
        bool isCa = cert.ca_istrue != 0;
    #endif
      mbedtls_x509_crt_free(&cert);

    if (rc!=0 || !isCa) { reply(400,"Cần chứng chỉ CA gốc PEM hợp lệ."); return; }
    next.ca=ca;
  }
  long minutes=server.arg("minutes").toInt();
  float threshold=server.arg("threshold").toFloat();
  if (minutes<15 || minutes>1440 || !isfinite(threshold) || threshold<27 || threshold>60) { reply(400,"Chu kỳ hoặc ngưỡng không hợp lệ."); return; }
  next.intervalSeconds=minutes*60; next.threshold=threshold;
  next.reminder=server.hasArg("reminder"); next.sound=server.hasArg("sound");
  if (!saveConfig(next)) { reply(500,"Không lưu được. Cấu hình cũ được giữ lại."); return; }
  *current=next; saved=true;
  reply(200,"<meta charset='utf-8'><h1>Đã lưu</h1><p>Thiết bị sẽ thử kết nối. Theo dõi OLED; nếu kết nối không được, Wi-Fi cấu hình vẫn mở để sửa.</p><a href='/'>Quay lại</a>");
}
void portalBegin(Config* c) {
  current=c;
  if (active) { started=millis(); return; }
  if (!routes) {
    const char* headers[]={"Content-Length"}; server.collectHeaders(headers,1);
    server.on("/",HTTP_GET,home); server.on("/save",HTTP_POST,save);
    server.onNotFound(home); routes=true;
  }
  char suffix[7]; snprintf(suffix,sizeof(suffix),"%06lx",(unsigned long)(ESP.getEfuseMac()&0xffffff));
  name="HP20-"+String(suffix); password=randomHex().substring(0,12); csrf=randomHex();
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(name.c_str(),password.c_str())) return;
  dns.start(53,"*",WiFi.softAPIP()); server.begin(); started=millis(); active=true;
}
void portalTick() { if (active) { dns.processNextRequest(); server.handleClient(); if (model::elapsed(millis(),started,settings::PORTAL_TIMEOUT_MS)) portalClose(); } }
bool portalSaved() { bool result=saved; saved=false; return result; }
bool portalActive() { return active; }
void portalClose() { if (!active) return; server.stop(); dns.stop(); WiFi.softAPdisconnect(true); active=false; }
String portalName() { return name; }
String portalPassword() { return password; }
