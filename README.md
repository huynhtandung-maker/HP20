# HP20 · Indoor Heat-Feel Monitor · v0.9.6

HP20 là firmware ESP32 cho không gian học tập/làm việc trong khí hậu nóng ẩm. Thiết bị đọc DHT22, hiệu chỉnh cảm biến, tính **FEEL / Heat Index ước tính**, hiển thị OLED, điều khiển LED chỉ báo mức thoải mái và gửi dữ liệu lên ThingsBoard Cloud.

> HP20 là thiết bị thử nghiệm môi trường, **không phải thiết bị y tế** và không đo nhiệt độ cơ thể.

## 1. Phần cứng hiện tại

- ESP32 Dev Module
- DHT22
- OLED I2C 128×64 SH1106 hoặc SSD1306
- LED xanh ngoài GPIO25: mức độ thoải mái nhiệt
- LED xanh dương onboard GPIO2: trạng thái kỹ thuật / kết nối
- Buzzer GPIO26
- BOOT button GPIO0

## 2. Kiến trúc

```text
DHT22 → hp20_sensor → hp20_thermal → hp20_trend → hp20_ui
                         ├→ hp20_indicator → LED xanh
                         ├→ reminder/buzzer
                         └→ cloud → ThingsBoard

HP20.ino            điều phối hệ thống
config.*            lưu cấu hình trong NVS
hp20_provisioning.* seed local secrets có kiểm soát
portal.*            onboarding/captive portal
cloud.*             HTTPS telemetry
hp20_ota.*          cập nhật firmware từ xa + SHA-256
hp20_version.h      nguồn version duy nhất
```

## 3. UX onboarding v0.9.6

Luồng người dùng chuẩn:

```text
Giữ BOOT ~3 giây
→ phone chọn Wi-Fi HP20-xxxxxx (KHÔNG password)
→ captive portal
→ chọn Wi-Fi 2.4 GHz mạnh/phù hợp
→ nhập password Wi-Fi
→ Lưu & kết nối
→ HP20 tự nhớ trong NVS
```

Portal không bắt người dùng nhập thêm “mật khẩu của HP20”. Danh sách mạng được sắp theo tín hiệu và có nhãn `Rất tốt / Tốt / Trung bình / Yếu`.

Các mục kỹ thuật `ThingsBoard token / host / CA TLS / OTA / chu kỳ gửi` được đưa xuống **Cài đặt nâng cao**. Password và token đã lưu không được render ngược đầy đủ ra trình duyệt.

Nếu Wi-Fi đang dùng mất kết nối, HP20 chỉ tự reconnect; **không tự mở AP công khai sau vài phút**. AP cài đặt chỉ tự mở khi chưa có Wi-Fi hoặc khi người đứng cạnh thiết bị giữ BOOT ~3 giây.

## 4. Phản hồi khi bấm “Lưu & kết nối”

- Trang web chuyển ngay sang trạng thái đang kết nối và tự kiểm tra tiến độ.
- 1 beep + LED xanh dương nháy nhanh: đang kết nối.
- 2 beep + LED xanh dương sáng ngắn: Wi-Fi thành công.
- 3 beep + LED nháy 3 xung: chưa kết nối được; portal vẫn còn để sửa.

Mất điện không làm mất Wi-Fi/token vì cấu hình nằm trong NVS của ESP32.

## 5. ThingsBoard telemetry

Chu kỳ chuẩn v0.9.6 là **5 phút/lần**. Firmware cũ có baseline 15 phút được migration một lần sang 5 phút; sau đó người dùng vẫn có thể chủ động chọn 15/30/60 phút hoặc lâu hơn trong Advanced.

Sau khi vừa lưu cấu hình hoặc vừa boot, firmware có thể gửi một lần sớm để xác nhận kết nối; sau đó quay lại chu kỳ cấu hình. Không gửi backlog khi mất điện.

## 6. OTA

`OTA` = **Over-The-Air**, cập nhật firmware qua mạng mà không cắm USB.

- Mặc định **TẮT** cho tới khi owner bật.
- Dùng HTTPS và CA tin cậy; không dùng kết nối TLS không xác minh.
- ThingsBoard package title phải là `HP20`.
- Chỉ nhận semantic version mới hơn.
- Kiểm SHA-256 trước khi kích hoạt firmware mới.
- Chu kỳ kiểm tra OTA: 15 phút → 24 giờ; mặc định 1 giờ khi tạo cấu hình mới.

Xem `OTA.md` cho quy trình test ThingsBoard.

## 7. Secrets

ZIP/repository **không chứa `secrets.h` thật**. Nếu developer cần seed Wi-Fi/token local:

1. copy `secrets.example.h` thành `secrets.h`;
2. điền giá trị thật chỉ trên máy local;
3. không commit file này.

Nếu thiết bị đã có NVS hợp lệ, không cần `secrets.h` để chạy.

## 8. Build

- Arduino IDE 2.x
- Board: ESP32 Dev Module
- ESP32 core: 3.3.11
- DHT sensor library: 1.4.7
- Adafruit Unified Sensor: 1.1.15
- U8g2: 2.36.19
- ArduinoJson: 7.4.3

Quy tắc: **Verify → Upload → test board thật → GitHub Actions PASS → tag/release**.

## 9. Version

Chỉ sửa version tại `hp20_version.h`.

```cpp
constexpr const char* STRING = "0.9.6";
constexpr const char* TAG = "v0.9.6";
```

Không tạo tag `v0.9.6` trước khi test OTA thật hoàn tất.
