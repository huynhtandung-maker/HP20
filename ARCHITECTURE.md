# HP20 Architecture · v0.9.5

## Nguyên tắc

HP20 dùng **single responsibility + single source of truth**. Một loại logic chỉ được sở hữu bởi một module.

| Thành phần | Trách nhiệm | Không được chứa |
|---|---|---|
| `hp20_sensor.*` | DHT22 raw, validation, calibration | FEEL bands, UI, cloud |
| `model.h` | công thức Heat Index nền, Button, Reminder, retry helpers | UI text, local bands |
| `hp20_thermal.*` | FEEL state, local bands, meaning/action, target range | OLED drawing, Wi‑Fi |
| `hp20_indicator.*` | pattern LED xanh theo thermal band | tự tính band/FEEL |
| `hp20_trend.*` | lịch sử và xu hướng FEEL | lời khuyên thermal |
| `hp20_ui.*` | OLED layout/chart/marquee | threshold thermal |
| `config.*` | persistent config/NVS | portal rendering |
| `hp20_provisioning.*` | NVS → local secrets → portal fallback | thermal/UI logic |
| `portal.*` | onboarding 3 bước qua AP | hard-code secret vào repo |
| `cloud.*` | HTTPS/ThingsBoard telemetry transport | thermal classification |
| `hp20_ota.*` | OTA check/download/verify/apply | FEEL/UI threshold |
| `HP20.ino` | orchestrator | threshold, calibration, OLED drawing |
| `hp20_version.h` | firmware identity | GPIO/runtime settings |
| `settings.h` | GPIO, timing, hardware config | firmware version, secrets |

## Luồng dữ liệu

```text
DHT22 RAW
  ↓
hp20_sensor
  ↓ calibrated T/RH
hp20_thermal
  ↓ FEEL raw + FEEL UI + Band
  ├→ hp20_indicator → green LED
  ├→ hp20_trend     → trend
  ├→ hp20_ui        → OLED
  ├→ reminder/buzzer
  └→ cloud telemetry
```

## Version độc lập

- Firmware: `hp20_version.h`
- Thermal interpretation model: `hp20_thermal.h::MODEL_VERSION`

Hai version có thể khác nhau. Ví dụ sửa Wi‑Fi/UI có thể tăng firmware version mà không đổi thermal model.

## Provisioning và phục hồi

```text
BOOT
  ↓
loadConfig(NVS)
  ↓
hp20_provisioning: PROFILE_REVISION local > revision đã lưu?
  ├─ có → apply local profile một lần → save NVS
  └─ không → giữ nguyên NVS
  ↓
SSID có? ── có → Wi-Fi connect
  └─ không → captive portal

Mất điện → NVS giữ config → tự reconnect.
Đổi địa điểm → giữ BOOT 3 s → portal → save NVS → dùng mạng mới từ lần boot sau.
```

## OTA

`hp20_ota.*` chỉ chạy khi Wi-Fi/TLS/ThingsBoard sẵn sàng, cloud không có request đang bay và portal đóng. Telemetry tạm nhường đường trong lúc OTA. Firmware mới phải có title `HP20`, semantic version mới hơn và checksum SHA-256 hợp lệ trước khi `Update.end()` kích hoạt partition mới.


## TLS trust resolution

`custom CA in NVS` → ưu tiên cao nhất. Nếu trống và host là `thingsboard.cloud` → dùng ISRG Root X1 tích hợp trong firmware. Host tùy chỉnh mà không có CA → chặn cloud/OTA an toàn.
