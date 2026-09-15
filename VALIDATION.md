# HP20 Validation Status · v0.9.6 RC1

## Đã kiểm tra trong môi trường tạo package

- Host regression tests: **PASS**.
- Thermal/trend/green LED/version regression: **PASS**.
- `hp20_version.h` đồng bộ 0.9.6.
- Telemetry default/minimum source: 300 s = 5 phút.
- `secrets.h` thật: **không nằm trong package**.
- Portal source: open setup AP, Wi-Fi scan/ranking, Advanced cloud/OTA, save progress endpoint.
- Auto-open AP sau Wi-Fi loss thường: đã loại bỏ; chỉ first boot/physical BOOT/Serial SETUP.

## Cần xác nhận trên ESP32 thật trước Git tag

- Arduino IDE Verify với ESP32 core 3.3.11 và libraries pinned.
- Upload board thật.
- Captive portal tự bật trên phone; fallback http://192.168.4.1.
- Wi-Fi list và RSSI label đúng thực tế.
- 1/2/3 beep + board LED cho connecting/success/failure.
- NVS giữ credential sau power cycle.
- ThingsBoard Active + Latest telemetry.
- Cadence 5 phút sau lần gửi xác nhận ban đầu.
- OTA toggle lưu được và firmware báo current version.
- OTA end-to-end bằng **target version mới hơn 0.9.6**.
- SH1106 và SSD1306 GitHub Actions compile PASS.

Không tuyên bố release production cho tới khi các mục hardware/OTA trên PASS.
