# HP20 · Indoor Heat-Feel Monitor · v0.9.5

HP20 là firmware ESP32 cho không gian học tập/làm việc trong khí hậu nóng ẩm. Thiết bị đọc DHT22, hiệu chỉnh cảm biến, tính **FEEL / Heat Index ước tính**, diễn giải theo thang HP20 có thể tự cân chỉnh, hiển thị OLED 1.3", điều khiển LED xanh theo mức thoải mái và có thể gửi telemetry lên ThingsBoard.

> HP20 là thiết bị thử nghiệm môi trường, **không phải thiết bị y tế** và không đo nhiệt độ cơ thể.

## Phần cứng hiện tại

- ESP32 Dev Module
- DHT22
- OLED I2C 128×64 SH1106 hoặc SSD1306
- LED xanh ngoài GPIO25: chỉ báo **mức độ thoải mái nhiệt**
- LED xanh dương onboard GPIO2: trạng thái kỹ thuật
- Buzzer GPIO26
- BOOT button GPIO0

## Kiến trúc

```text
DHT22
  ↓
hp20_sensor.*      raw → calibration → T/RH hợp lệ
  ↓
hp20_thermal.*     T/RH → FEEL → band → nhận định/hành động
  ├──────────────→ hp20_indicator.* → LED xanh theo comfort
  ↓
hp20_trend.*       xu hướng FEEL
  ↓
hp20_ui.*          OLED 7 trang

HP20.ino            orchestrator
config.*            cấu hình lưu NVS
hp20_provisioning.* NVS → local secrets → portal fallback
portal.*            captive portal 3 bước Wi‑Fi / Cloud / xác nhận
cloud.*             HTTPS telemetry
hp20_ota.*          ThingsBoard HTTPS OTA + SHA-256 verification
model.h             HI formula + utility dùng chung
hp20_version.h      firmware version duy nhất
settings.h          chân GPIO / timing / hardware settings
```

## Version

Firmware version chỉ sửa tại:

```cpp
// hp20_version.h
constexpr const char* STRING = "0.9.5";
```

OLED, Serial boot và portal đều đọc cùng nguồn này. Git tag phát hành phải tương ứng `v0.9.5`.

## Thang FEEL tự cân chỉnh

Không chỉnh ngưỡng trong OLED hay `HP20.ino`. Mở:

```text
hp20_thermal.cpp
```

và tìm vùng:

```text
LOCAL FEEL THRESHOLD TUNING AREA
```

Mỗi ngưỡng có chú thích về khoảng FEEL và ý nghĩa. Xem `CALIBRATION.md` trước khi đổi.

## LED xanh

LED xanh ngoài không phải đèn cảnh báo khẩn. Ý nghĩa:

- `Comfort` → sáng liên tục.
- Mức thoải mái giảm → thời gian xuất hiện màu xanh giảm dần.
- Band nóng nhất → chỉ nháy xanh rất ngắn, chu kỳ dài.

Logic nằm duy nhất trong `hp20_indicator.cpp`.

## Wi‑Fi / ThingsBoard / secrets

HP20 v0.9.5 dùng **hybrid provisioning** theo thứ tự:

```text
NVS đã lưu → dùng ngay
NVS trống + có secrets.h local → seed một lần vào NVS
không có cả hai → mở captive portal
```

`secrets.h` thật chỉ nằm trên máy developer và đã bị `.gitignore` chặn. Copy `secrets.example.h` thành `secrets.h`, điền Wi‑Fi/password/token/CA rồi Verify/Upload. `PROFILE_REVISION` tạo một lần áp dụng có chủ đích: revision mới hơn NVS sẽ seed/override local profile đúng **một lần**; sau đó NVS trở thành nguồn chính. Đổi Wi‑Fi bằng portal sẽ không bị `secrets.h` kéo ngược lại. Khi thật sự muốn áp local profile mới, tăng `PROFILE_REVISION`.

Portal vẫn luôn mở thủ công bằng cách giữ BOOT khoảng 3 giây. Portal v0.9.5 chia 3 bước rõ ràng, cho phép xem/ẩn **giá trị mới đang nhập**, nhưng không hiển thị lại đầy đủ password/token đã lưu.

## OTA từ xa

OTA là **opt-in** và mặc định tắt. Khi bật, HP20 dùng chính HTTPS ThingsBoard đang cấu hình để:

1. báo `current_fw_title/current_fw_version`;
2. đọc firmware package được gán qua shared attributes;
3. chỉ nhận package title `HP20`, version mới hơn;
4. tải firmware qua HTTPS;
5. kiểm SHA-256 trước khi kích hoạt OTA partition;
6. reboot sau khi xác minh thành công.

Lệnh Serial `OTA` yêu cầu kiểm tra ngay. OLED trang KẾT NỐI hiển thị trạng thái OTA. Xem `OTA.md`.

## Build

Arduino IDE:

- Board: ESP32 Dev Module
- ESP32 core: 3.3.11
- DHT sensor library: 1.4.7
- Adafruit Unified Sensor: 1.1.15
- U8g2: 2.36.19
- ArduinoJson: 7.4.3

Luôn **Verify → Upload → test phần cứng** trước khi tạo Git tag/release.

GitHub Actions kiểm tra host regression và compile cả SH1106/SSD1306.

## Tài liệu

- `ARCHITECTURE.md` — ranh giới module
- `CALIBRATION.md` — hiệu chỉnh DHT22 và thang FEEL
- `SCIENCE.md` — giả định/mô hình thermal
- `SECURITY.md` — Wi‑Fi/token/secrets
- `TESTING.md` — quy trình test
- `VALIDATION.md` — trạng thái kiểm chứng
- `RELEASE.md` — quy trình Git/GitHub release
- `CHANGELOG.md` — lịch sử thay đổi


## TLS UX

Với host mặc định `thingsboard.cloud`, HP20 v0.9.5 FIX2 có sẵn ISRG Root X1 trong firmware. Người dùng chỉ cần Wi-Fi, mật khẩu và device token. Trường CA trong portal chỉ dành cho ThingsBoard self-hosted/custom host.
