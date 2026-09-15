# HP20 Validation Status · v0.9.5

## Đã kiểm tra trong package

- Host regression tests: **PASS**.
- Thermal thresholds có compile-time ordering guards.
- Green LED semantic regression test: **PASS**.
- Version source đã gom về `hp20_version.h`.
- Không có Wi‑Fi password/token hard-code trong source package.
- Hybrid provisioning và OTA source đã được thêm; cần CI + board validation trước merge.

## Cần xác nhận trên máy/ESP32 của chủ dự án trước release

- Arduino IDE Verify với ESP32 core/library thực tế.
- Upload lên board thật.
- OLED SH1106 128×64 layout và version footer.
- DHT22 calibration với cảm biến tham chiếu.
- Green LED timing bằng quan sát thực.
- Hybrid provisioning: NVS / secrets.h / portal priority.
- Captive portal 3-step lưu/đổi Wi‑Fi và ThingsBoard token.
- ThingsBoard OTA bằng binary v0.9.6 test sau khi flash v0.9.5 qua USB.
- Power-cycle recovery.
- ThingsBoard HTTPS nếu được cấu hình.

HP20 không phải thiết bị y tế. Thermal bands là lớp diễn giải vận hành đang được hiệu chỉnh thực nghiệm cho người dùng mục tiêu.
