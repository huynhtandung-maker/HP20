# HP20 Architecture · v0.9.4

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
| `config.*` | persistent config | portal rendering |
| `portal.*` | cấu hình runtime qua AP | hard-code password/token |
| `cloud.*` | HTTPS/ThingsBoard transport | thermal classification |
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
