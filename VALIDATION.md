# HP20 Validation Status · v0.9.4

## Đã kiểm tra trong package

- Host regression tests: **PASS**.
- Thermal thresholds có compile-time ordering guards.
- Green LED semantic regression test: **PASS**.
- Version source đã gom về `hp20_version.h`.
- Không có Wi‑Fi password/token hard-code trong source package.

## Cần xác nhận trên máy/ESP32 của chủ dự án trước release

- Arduino IDE Verify với ESP32 core/library thực tế.
- Upload lên board thật.
- OLED SH1106 128×64 layout và version footer.
- DHT22 calibration với cảm biến tham chiếu.
- Green LED timing bằng quan sát thực.
- Captive portal lưu/đổi Wi‑Fi và ThingsBoard token.
- Power-cycle recovery.
- ThingsBoard HTTPS nếu được cấu hình.

HP20 không phải thiết bị y tế. Thermal bands là lớp diễn giải vận hành đang được hiệu chỉnh thực nghiệm cho người dùng mục tiêu.
