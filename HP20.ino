#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "settings.h"
#include "model.h"
#include "config.h"
#include "cloud.h"
#include "portal.h"

#ifndef OLED_SH1106
#define OLED_SH1106 1 // Set to 0 for SSD1306; both assume 128x64 I2C.
#endif
#if OLED_SH1106
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0,U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0,U8X8_PIN_NONE);
#endif
DHT dht(settings::DHT_PIN,DHT22);
Config config;
Preferences runtime;
model::Reminder reminder;
float temperature=NAN, humidity=NAN;
double feel=NAN;
bool sampleOk=false, displayOk=false, cloudReady=false, inFlight=false;
bool authBlocked=false, remind=false, autoPortal=false, wasConnected=false;
bool ignoreOldResult=false;
bool buzzing=false, bootBeeping=false;
uint32_t sampled=0, drawn=0, wifiAttempt=0, offlineSince=0, connectedSince=0;
uint32_t beepSince=0, lastBeep=0, bootBeepSince=0, sendMark=0, waitSeconds=900;
uint64_t notBefore=0;
unsigned failures=0;
const char* cloudState="CHUA CAU HINH";
model::Button bootButton;
uint32_t statusSince=0;
bool portalWasActive=false;
uint8_t uiPage=0;
uint32_t uiPageSince=0;

bool fresh(uint32_t now) { return sampleOk && !model::elapsed(now,sampled,settings::STALE_MS); }
void drawText(int y, const String& s) { oled.drawStr(2,y,s.c_str()); }
model::RoomBand currentBand() { return model::roomBand(temperature,humidity,feel); }
const char* bandLabel(model::RoomBand b) {
  switch (b) {
    case model::RoomBand::Comfortable: return "DE CHIU";
    case model::RoomBand::Humid: return "AM CAO";
    case model::RoomBand::Warm: return "BAT DAU NONG";
    case model::RoomBand::Hot: return "KHO CHIU";
    case model::RoomBand::SevereHeat: return "RAT KHO CHIU";
    case model::RoomBand::Cool: return "MAT";
    default: return "CHO CAM BIEN";
  }
}
void buzzerOff() {
  if (settings::PASSIVE_BUZZER) noTone(settings::BUZZER_PIN);
  digitalWrite(settings::BUZZER_PIN,LOW);
}
void startBootBuzzer(uint32_t now) {
  if (!settings::BUZZER_BOOT_TEST || bootBeeping) return;
  bootBeeping=true; bootBeepSince=now; lastBeep=now;
  if (settings::PASSIVE_BUZZER) tone(settings::BUZZER_PIN,2200);
  else digitalWrite(settings::BUZZER_PIN,HIGH);
  Serial.println("BUZZER boot test: ON");
}
// Character-wise scrolling stays inside its own line, including long statuses.
void scrollLine(int y, String text, uint8_t columns, uint32_t now) {
  for (unsigned i=0;i<text.length();i++)
    if ((uint8_t)text[i]<32 || (uint8_t)text[i]>126) text.setCharAt(i,'?');
  unsigned start=0;
  if (text.length()>columns) {
    unsigned distance=text.length()-columns;
    unsigned phase=(uint32_t(now-uiPageSince)/450)%(distance*2+8);
    if (phase<4) start=0;
    else if (phase<distance+4) start=phase-4;
    else if (phase<distance+8) start=distance;
    else start=distance*2+8-phase;
  }
  drawText(y,text.substring(start,start+columns));
}
void title(const char* label) {
  oled.setFont(u8g2_font_5x7_tf); drawText(8,label);
  if (WiFi.status()==WL_CONNECTED) {
    int rssi=WiFi.RSSI();
    for (int i=0;i<3;i++)
      if (i==0 || (i==1 && rssi>-80) || (i==2 && rssi>-65)) oled.drawBox(112+i*5,7-i*2,3,2+i*2);
  } else { oled.drawLine(117,2,123,8); oled.drawLine(117,8,123,2); }
  oled.drawHLine(0,11,128);
}
void footer(const String& label, uint8_t page) {
  oled.drawHLine(0,54,128); oled.setFont(u8g2_font_5x7_tf);
  scrollLine(63,label,20,millis());
  oled.drawStr(110,63,(String(page+1)+"/4").c_str());
}
uint32_t nextSendSeconds(uint32_t now) {
  return model::remainingSeconds(now,sendMark,waitSeconds,(uint64_t)time(nullptr),notBefore);
}
uint32_t pageDuration(uint8_t page) {
  // Advice stays visible longer because its text is deliberately read at a calm pace.
  switch (page) {
    case 1: return 18000;
    case 3: return 14000;
    default: return 10000;
  }
}
const char* bandMessage(model::RoomBand b) {
  switch (b) {
    case model::RoomBand::SevereHeat: return "Nong am rat cao: de met, mat nuoc va giam tap trung.";
    case model::RoomBand::Hot: return "Nong am: co the kho thoat nhiet khi ngoi lam viec lau.";
    case model::RoomBand::Warm: return "Phong bat dau nong: nen giam tich nhiet som.";
    case model::RoomBand::Humid: return "Do am cao: de bich, ngam va tang nguy co moc.";
    case model::RoomBand::Cool: return "Phong mat: uu tien luong gio de tranh lanh cuc bo.";
    case model::RoomBand::Comfortable: return "Dieu kien phu hop cho cong viec ban giay trong phong.";
    default: return "Chua co so do tin cay tu DHT22.";
  }
}
const char* bandAction(model::RoomBand b) {
  switch (b) {
    case model::RoomBand::SevereHeat: return "Bat dieu hoa/quat ngay; nghi noi mat, uong nuoc tung ngum. Chong mat: goi ho tro.";
    case model::RoomBand::Hot: return "Lam mat phong, tranh viec gang suc; nghi ngan va uong nuoc theo nhu cau.";
    case model::RoomBand::Warm: return "Bat quat, tang luu thong khi; theo doi met moi trong ca lam viec.";
    case model::RoomBand::Humid: return "Thong gio khi khong khi ngoai sach; dung hut am neu co, kiem tra moc.";
    case model::RoomBand::Cool: return "Chinh huong quat/AC va trang phuc theo cam nhan cua nguoi trong phong.";
    case model::RoomBand::Comfortable: return "Duy tri muc hien tai; thong gio theo chat luong khong khi ngoai troi.";
    default: return "Kiem tra DHT22, day du lieu va nguon cap cho cam bien.";
  }
}
void displayTick(uint32_t now) {
  bool setupOpen=portalActive();
  if (setupOpen && !portalWasActive) { uiPage=3; uiPageSince=now; }
  portalWasActive=setupOpen;
  if (!displayOk || !model::elapsed(now,drawn,200)) return;
  drawn=now;
  if (!(setupOpen && uiPage==3) && model::elapsed(now,uiPageSince,pageDuration(uiPage))) {
    uiPage=(uiPage+1)%4; uiPageSince=now;
  }
  model::RoomBand b=currentBand();
  oled.clearBuffer();
  if (uiPage==0) {
    title("HP20-VN / NGAY");
    oled.setFont(u8g2_font_5x7_tf);
    drawText(20,String("TRANG THAI: ")+bandLabel(b));
    oled.setFont(u8g2_font_logisoso24_tn);
    String value=fresh(now) && isfinite(feel)?String(feel,1):"--";
    int width=oled.getStrWidth(value.c_str()), x=(128-width-15)/2;
    oled.drawStr(x,45,value.c_str());
    oled.setFont(u8g2_font_5x7_tf);
    if (fresh(now) && isfinite(feel)) { oled.drawStr(x+width+3,34,"oC HI"); }
    if (fresh(now)) {
      drawText(52,"T "+String(temperature,1)+" C");
      oled.drawStr(74,52,("RH "+String(humidity,1)+"%").c_str());
    } else drawText(52,"DHT22: CHO / LOI SO DO");
    footer("CHI SO CHINH / BOOT DOI TAB",uiPage);
  } else if (uiPage==1) {
    title("KHUYEN NGHI / REAL-TIME"); oled.setFont(u8g2_font_5x7_tf);
    drawText(21,String("VN OFFICE: ")+bandLabel(b));
    scrollLine(33,String("NHAN DINH: ")+bandMessage(b),24,now);
    scrollLine(45,String("HANH DONG: ")+bandAction(b),24,now+900);
    drawText(52,fresh(now)?"T "+String(temperature,1)+"C  RH "+String(humidity,0)+"%":"DHT22 CAN KIEM TRA");
    footer("DOC 18s / BOOT DOI TAB",uiPage);
  } else if (uiPage==2) {
    title("MOI TRUONG / SO DO"); oled.setFont(u8g2_font_6x10_tf);
    if (fresh(now)) {
      drawText(25,"NHET: "+String(temperature,1)+" C");
      drawText(38,"DO AM: "+String(humidity,1)+" %");
      drawText(51,isfinite(feel)?"CAM NHAN HI: "+String(feel,1)+" C":"HI: NGOAI MIEN UOC TINH");
    } else { drawText(29,"DHT22 CHUA CO SO DO"); drawText(43,"KIEM TRA DAY / NGUON"); }
    footer("DHT22: CHUA DO CO2 / PM / VOC",uiPage);
  } else {
    title(setupOpen?"CAI DAT WI-FI":"KET NOI / CAI DAT"); oled.setFont(u8g2_font_5x7_tf);
    if (setupOpen) {
      drawText(23,"AP: "+portalName()); drawText(34,"PW: "+portalPassword());
      drawText(45,"Mo 192.168.4.1");
      footer("BOOT DOI TAB / AP VAN MO",uiPage);
    } else {
      scrollLine(23,WiFi.status()==WL_CONNECTED?"WI-FI: "+WiFi.SSID():"WI-FI: DANG THU KET NOI",24,now);
      scrollLine(34,String(cloudState),24,now);
      drawText(45,"GUI TOI THIEU "+String(config.intervalSeconds/60)+" PHUT");
      footer("GIU BOOT 3s: DOI WI-FI",uiPage);
    }
  }
  oled.sendBuffer();
}
void connectWifi() {
  if (config.ssid.isEmpty()) return;
  WiFi.begin(config.ssid.c_str(),config.password.c_str()); wifiAttempt=millis();
}
void persistCooldown(uint32_t seconds) {
  notBefore=(uint64_t)time(nullptr)+seconds;
  if (runtime.putULong64("notBefore",notBefore)!=sizeof(uint64_t)) {
    cloudReady=false; cloudState="LOI LUU HAN GUI";
  }
}
void networkTick(uint32_t now) {
  bool connected=WiFi.status()==WL_CONNECTED;
  if (connected && !wasConnected) {
    connectedSince=now; autoPortal=false;
    configTime(0,0,"pool.ntp.org","time.google.com");
  }
  if (!connected && wasConnected) offlineSince=now;
  wasConnected=connected;
  if (!connected && !config.ssid.isEmpty() && model::elapsed(now,wifiAttempt,settings::WIFI_RETRY_MS)) connectWifi();
  if (!connected && !autoPortal && model::elapsed(now,offlineSince,120000)) { portalBegin(&config); autoPortal=true; }
  // A saved portal closes after a successful association grace period.
  static bool closeOnConnect=false;
  portalTick();
  if (portalSaved()) {
    ignoreOldResult=inFlight;
    // Reset authentication latch only after a deliberate configuration save.
    authBlocked=false;
    if (runtime.putBool("auth",false)!=sizeof(bool)) { cloudReady=false; cloudState="LOI LUU TRANG THAI"; }
    failures=0; sendMark=now; waitSeconds=config.intervalSeconds;
    remind=false; reminder=model::Reminder();
    WiFi.disconnect(); wasConnected=false; offlineSince=now; connectWifi();
    closeOnConnect=true; connected=false;
  }
  if (!connected && autoPortal && !portalActive()) { autoPortal=false; offlineSince=now; }
  if (closeOnConnect && connected && wasConnected && model::elapsed(now,connectedSince,30000)) {
    portalClose(); closeOnConnect=false;
  }
}
void cloudTick(uint32_t now) {
  int result;
  if (cloudResult(result)) {
    inFlight=false;
    if (ignoreOldResult && result!=429) { ignoreOldResult=false; return; }
    ignoreOldResult=false;
    if (result>=200 && result<300) { failures=0; cloudState="TB: DA NHAN"; }
    else if (result==401 || result==403) {
      authBlocked=true; cloudState="TB: KIEM TRA TOKEN";
      if (runtime.putBool("auth",true)!=sizeof(bool)) { cloudReady=false; cloudState="LOI LUU TRANG THAI"; }
    } else if (result==429) {
      waitSeconds=86400; sendMark=now; persistCooldown(waitSeconds); cloudState="TB: TAM DUNG 24H";
    } else {
      failures++; waitSeconds=model::retrySeconds(config.intervalSeconds,failures);
      sendMark=now; persistCooldown(waitSeconds); cloudState="TB: LOI / CHO LAI";
    }
  }
  if (!cloudReady || inFlight) return;
  if (config.host.isEmpty() || config.token.isEmpty()) { cloudState="TB: CHUA CAU HINH"; return; }
  if (config.ca.isEmpty()) { cloudState="TB: THIEU CA TLS"; return; }
  if (authBlocked) { cloudState="TB: KIEM TRA TOKEN"; return; }
  if (WiFi.status()!=WL_CONNECTED) { cloudState="TB: CHO WI-FI"; return; }
  if (time(nullptr)<1704067200) { cloudState="TB: CHO DONG HO"; return; }
  if (!fresh(now)) { cloudState="TB: CHO CAM BIEN"; return; }
  if (nextSendSeconds(now)!=0) return;
  StaticJsonDocument<256> d;
  d["temperature"]=temperature; d["humidity"]=humidity;
  if (isfinite(feel)) d["heat_index_c"]=feel;
  d["heat_index_valid"]=isfinite(feel);
  String payload; serializeJson(d,payload);
  // Reserve next slot BEFORE transmission; reboot cannot replay a backlog.
  sendMark=now; waitSeconds=config.intervalSeconds;
  persistCooldown(waitSeconds);
  if (!cloudReady) return;
  inFlight=cloudSubmit(config,payload);
  cloudState=inFlight?"TB: DANG GUI":"TB: LOI HANG DOI";
}
void controlsTick(uint32_t now) {
  model::ButtonEvent event=bootButton.update(now,digitalRead(settings::BUTTON_PIN)==LOW);
  if (event==model::ButtonEvent::Hold) {
    portalBegin(&config); uiPage=3; uiPageSince=now;
    Serial.println("BOOT hold: setup requested");
  } else if (event==model::ButtonEvent::Click) {
    uiPage=(uiPage+1)%4; uiPageSince=now;
    Serial.printf("BOOT click: page %u/4\n",unsigned(uiPage+1));
  }
  remind=reminder.update(now,fresh(now)?feel:NAN,config.reminder,config.threshold);
  // GPIO25 is the human-visible thermal signal. The blue board LED remains technical.
  bool led=false;
  switch (currentBand()) {
    case model::RoomBand::Comfortable: led=true; break; // steady: comfortable
    case model::RoomBand::Warm: led=(now%500)<250; break; // quick, even blink
    case model::RoomBand::Hot: led=(now%1400)<500; break; // slower cycle
    case model::RoomBand::SevereHeat: led=(now%1800)<350; break; // off longer than on
    case model::RoomBand::Humid: led=(now%1000)<180; break;
    case model::RoomBand::Cool: led=(now%1600)<700; break;
    default: led=(now%250)<100; break;
  }
  digitalWrite(settings::LED_PIN,led);
  if (settings::BOARD_LED_ENABLED) {
    bool blue=false;
    if (now<800) blue=(now%400)<80; // Two short startup flashes to verify the mapping.
    else if (!fresh(now)) { uint32_t p=now%3000; blue=p<80 || (p>=200 && p<280) || (p>=400 && p<480); }
    else if (remind) blue=(now%1000)<150;
    else if (portalActive()) { uint32_t p=now%2000; blue=p<80 || (p>=200 && p<280); }
    else if (WiFi.status()!=WL_CONNECTED) blue=(now%2000)<80;
    else if (inFlight) blue=(now%400)<60;
    else blue=(now%8000)<50; // Quiet heartbeat; not proof of cloud delivery.
    digitalWrite(settings::BOARD_LED_PIN,blue==settings::BOARD_LED_ACTIVE_HIGH?HIGH:LOW);
  }
  if (bootBeeping && model::elapsed(now,bootBeepSince,settings::BUZZER_BOOT_TEST_MS)) {
    buzzerOff(); bootBeeping=false; Serial.println("BUZZER boot test: OFF");
  }
  if (buzzing && (!remind || !config.sound || model::elapsed(now,beepSince,150))) {
    buzzerOff(); buzzing=false;
  }
  if (remind && config.sound && !buzzing && !bootBeeping && model::elapsed(now,lastBeep,300000)) {
    lastBeep=beepSince=now; buzzing=true;
    if (settings::PASSIVE_BUZZER) tone(settings::BUZZER_PIN,2200);
    else digitalWrite(settings::BUZZER_PIN,HIGH);
  }
}
// Commands are bounded and processed incrementally; no blocking readString().
void serialTick(uint32_t now) {
  static char command[16];
  static uint8_t used=0;
  static bool overflow=false;
  for (unsigned count=0; count<32 && Serial.available(); ++count) {
    char ch=char(Serial.read());
    if (ch=='\r' || ch=='\n') {
      if (overflow) Serial.println("Command too long; use INFO or SETUP");
      else if (used) {
        command[used]='\0';
        if (strcmp(command,"SETUP")==0) {
          portalBegin(&config); uiPage=3; uiPageSince=now;
          if (portalActive()) {
            // Explicit local request only. Never print stored Wi-Fi password or cloud token.
            Serial.printf("SETUP AP=%s TEMP_PASSWORD=%s URL=http://192.168.4.1\n",
              portalName().c_str(),portalPassword().c_str());
          } else Serial.println("SETUP failed: could not start AP");
        } else if (strcmp(command,"INFO")==0) statusSince=now-settings::SERIAL_STATUS_MS;
        else if (strcmp(command,"BEEP")==0) { startBootBuzzer(now); Serial.println("BUZZER test requested"); }
        else Serial.println("Commands: INFO, SETUP, BEEP (New Line)");
      }
      used=0; overflow=false;
    } else if (!overflow) {
      if (used<sizeof(command)-1) command[used++]=ch;
      else overflow=true;
    }
  }
}
void setup() {
  Serial.begin(115200); Serial.printf("HP20 v%s boot\n",settings::VERSION);
  Serial.println("Commands: INFO or SETUP, select New Line. Status every 10s.");
  if (settings::BOARD_LED_ENABLED) {
    digitalWrite(settings::BOARD_LED_PIN,settings::BOARD_LED_ACTIVE_HIGH?LOW:HIGH);
    pinMode(settings::BOARD_LED_PIN,OUTPUT);
  }
  digitalWrite(settings::LED_PIN,LOW); digitalWrite(settings::BUZZER_PIN,LOW);
  pinMode(settings::LED_PIN,OUTPUT); pinMode(settings::BUZZER_PIN,OUTPUT);
  pinMode(settings::BUTTON_PIN,INPUT_PULLUP); startBootBuzzer(millis());
  dht.begin(); Wire.begin(settings::SDA_PIN,settings::SCL_PIN);
  Wire.setTimeOut(50);
  for (uint8_t addr : {uint8_t(0x3c),uint8_t(0x3d)}) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission()==0) { oled.setI2CAddress(addr*2); displayOk=true; break; }
  }
  if (displayOk) { oled.begin(); oled.setContrast(100); Serial.println("OLED I2C: detected (verify panel driver visually)"); }
  else Serial.println("OLED absent: verify I2C wiring/driver");
  loadConfig(config);
  bool storageOk=runtime.begin("room-runtime",false);
  if (storageOk) { notBefore=runtime.getULong64("notBefore",0); authBlocked=runtime.getBool("auth",false); }
  cloudReady=storageOk && cloudBegin();
  if (!cloudReady) cloudState="LOI BO NHO / CLOUD";
  sendMark=millis(); waitSeconds=config.intervalSeconds;
  WiFi.persistent(false); WiFi.setHostname("hp20-room"); WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
  connectWifi();
  if (config.ssid.isEmpty()) { portalBegin(&config); autoPortal=true; }
}
void loop() {
  uint32_t now=millis();
  if (model::elapsed(now,sampled,settings::SAMPLE_MS)) {
    sampled=now; float rh=dht.readHumidity(), t=dht.readTemperature();
    sampleOk=model::validSample(t,rh);
    temperature=sampleOk?t:NAN; humidity=sampleOk?rh:NAN;
    feel=sampleOk?model::heatIndex(t,rh):NAN;
  }
  serialTick(now); controlsTick(now); networkTick(now); cloudTick(now); displayTick(now);
  if (model::elapsed(now,statusSince,settings::SERIAL_STATUS_MS)) {
    statusSince=now;
    Serial.printf("STATUS up=%lus sensor=%s T=%.1f RH=%.1f HI=%.1f band=%s wifi=%s portal=%s page=%u button=%s cloud=%s wait>=%lus\n",
      (unsigned long)(now/1000),fresh(now)?"OK":"INVALID",temperature,humidity,feel,bandLabel(currentBand()),
      WiFi.status()==WL_CONNECTED?"OK":"OFFLINE",portalActive()?"OPEN":"CLOSED",unsigned(uiPage+1),
      digitalRead(settings::BUTTON_PIN)==LOW?"DOWN":"UP",cloudState,(unsigned long)nextSendSeconds(now));
  }
  delay(2);
}
