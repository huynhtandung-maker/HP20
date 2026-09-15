# HP20 OTA · ThingsBoard HTTPS · v0.9.6

`OTA` = **Over-The-Air**: cập nhật firmware từ xa qua Wi-Fi, không cần cắm USB.

## Trạng thái mặc định

- OTA: **TẮT** cho tới khi owner bật trong `Portal → Cài đặt nâng cao`.
- Chu kỳ kiểm tra: 15 phút → 24 giờ; cấu hình mới mặc định 1 giờ.
- Lệnh Serial `OTA` yêu cầu kiểm tra ngay, nhưng OTA vẫn phải được bật và đủ Wi-Fi/token/TLS.

## Luồng thiết bị

```text
Wi-Fi + TLS + ThingsBoard ready
→ báo current_fw_title/current_fw_version
→ đọc fw_title/fw_version/fw_checksum/fw_checksum_algorithm/fw_size
→ title=HP20 + version mới hơn + SHA256 + size hợp lệ?
→ tải firmware HTTPS
→ ghi OTA partition + tính SHA-256 song song
→ checksum khớp
→ Update.end()
→ reboot
```

## Test OTA sau khi v0.9.6 đã ổn định

Không thể chứng minh OTA bằng package **cùng version**. Cách test đúng:

1. Flash/upload v0.9.6 bằng USB và xác nhận ThingsBoard telemetry ổn định.
2. Trong Portal → Advanced, bật OTA và lưu.
3. Tạo một bản test tiếp theo, ví dụ `0.9.7` (chỉ sau khi chủ dự án đồng ý).
4. GitHub Actions build binary SH1106/SSD1306 đúng phần cứng.
5. ThingsBoard → Advanced features → OTA updates → Add package.
6. Title: `HP20`; Version: `0.9.7`; Type: Firmware; chọn đúng binary.
7. Gán package cho riêng device HP20 trước, chưa rollout cả fleet.
8. Theo dõi `fw_state`: DOWNLOADING → DOWNLOADED → VERIFIED → UPDATING → UPDATED.

## Safety gates

- HTTPS + CA bắt buộc.
- Portal phải đóng trước khi OTA chạy.
- Telemetry nhường đường khi OTA busy.
- Max firmware 4 MiB.
- Sai title/version/checksum/size → từ chối.
