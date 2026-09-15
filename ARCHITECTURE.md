# HP20 Architecture · v0.9.6

## Nguyên tắc

HP20 dùng **single responsibility + single source of truth**: mỗi loại logic có một chủ sở hữu rõ ràng.

| Thành phần | Trách nhiệm chính |
|---|---|
| `hp20_sensor.*` | DHT22 raw, validation, calibration |
| `model.h` | Heat Index nền, Button, Reminder, retry helper |
| `hp20_thermal.*` | FEEL, band, meaning/action |
| `hp20_indicator.*` | pattern LED xanh theo thermal band |
| `hp20_trend.*` | lịch sử và xu hướng FEEL |
| `hp20_ui.*` | OLED presentation |
| `config.*` | cấu hình persistent trong NVS |
| `hp20_provisioning.*` | seed local profile có revision |
| `portal.*` | AP/captive portal + UX onboarding |
| `cloud.*` | HTTPS telemetry transport |
| `hp20_ota.*` | OTA check/download/SHA-256/apply |
| `HP20.ino` | orchestrator + network/control feedback |
| `hp20_version.h` | firmware identity duy nhất |
| `settings.h` | GPIO/timing/hardware constants |

## Luồng dữ liệu

```text
DHT22 RAW
  ↓
hp20_sensor → calibrated T/RH
  ↓
hp20_thermal → FEEL + Band
  ├→ hp20_indicator → green comfort LED
  ├→ hp20_trend → trend
  ├→ hp20_ui → OLED
  ├→ reminder/buzzer
  └→ cloud → ThingsBoard
```

## Provisioning / onboarding

```text
BOOT
 ↓
load NVS
 ↓
optional secrets.h revision newer? ─yes→ seed một lần vào NVS
 ↓
SSID có?
 ├─ yes → reconnect bình thường
 └─ no  → mở AP HP20-xxxxxx tối đa 10 phút

Đổi mạng có chủ đích:
physical BOOT hold ~3 s → open AP → captive portal → save NVS → reconnect
```

AP setup v0.9.6 **không có password riêng**. Quyền mở AP dựa trên thao tác vật lý BOOT và timeout. Sau mất Wi-Fi bình thường, firmware không tự phơi AP; nó chỉ reconnect.

Portal dành cho người dùng chỉ yêu cầu Wi-Fi/password. ThingsBoard/token/CA/OTA được đặt trong Advanced.

## Trạng thái sau Save

`portalSaved()` → reset cloud auth/cooldown có chủ đích → reconnect Wi-Fi → feedback LED/buzzer → gửi telemetry xác nhận sớm → trở về cadence chuẩn 5 phút.

Portal giữ mở ngắn trong giai đoạn chuyển tiếp để phone đọc `/status`; sau khi Wi-Fi ổn định khoảng 30 giây, AP tự đóng.

## OTA

`hp20_ota.*` chỉ chạy khi Wi-Fi/TLS/ThingsBoard sẵn sàng, portal đóng và cloud không có request đang xử lý. Firmware phải thỏa: `title=HP20`, version mới hơn, size hợp lệ, SHA-256 khớp.

## TLS

`custom CA in NVS` có ưu tiên cao nhất. Nếu trống và host là `thingsboard.cloud`, dùng ISRG Root X1 tích hợp. Host tùy chỉnh không có CA → chặn cloud/OTA an toàn.
