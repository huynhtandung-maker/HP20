# HP20 · Indoor Heat-Feel Monitor · v0.9.4

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
portal.*            captive portal Wi‑Fi / ThingsBoard
cloud.*             HTTPS telemetry
model.h             HI formula + utility dùng chung
hp20_version.h      firmware version duy nhất
settings.h          chân GPIO / timing / hardware settings
```

## Version

Firmware version chỉ sửa tại:

```cpp
// hp20_version.h
constexpr const char* STRING = "0.9.4";
```

OLED, Serial boot và portal đều đọc cùng nguồn này. Git tag phát hành phải tương ứng `v0.9.4`.

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

HP20 **không lưu Wi‑Fi password hoặc ThingsBoard token trong source code**.

1. Giữ BOOT khoảng 3 giây để mở captive portal.
2. Kết nối AP của HP20.
3. Mở `192.168.4.1`.
4. Nhập SSID/password và ThingsBoard token nếu cần.
5. Dữ liệu được lưu bằng ESP32 Preferences/NVS và tồn tại qua mất điện.

`.env.example` chỉ là tài liệu tham chiếu; firmware Arduino không tự đọc `.env`.
Xem `SECURITY.md`.

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
