# HP20 OTA · ThingsBoard HTTPS

## Mục tiêu

HP20 v0.9.5 là USB baseline hiện đang chạy ổn định. Nhánh release candidate v0.9.6 được dùng làm **OTA target đầu tiên** để kiểm chứng đường cập nhật từ xa end-to-end. OTA mặc định tắt và dùng cùng host/token/CA TLS với telemetry; thiết bị không cần GitHub token.

## Device flow

```text
Wi-Fi + TLS ready
  ↓
POST current_fw_title/current_fw_version
  ↓
GET shared attrs fw_title/fw_version/fw_checksum/fw_checksum_algorithm/fw_size
  ↓
validate title=HP20 + newer version + SHA256 + size
  ↓
GET /api/v1/<token>/firmware?title=HP20&version=<target>
  ↓
write inactive OTA partition + stream SHA-256
  ↓
checksum match → Update.end() → reboot
```

## Acceptance test v0.9.5 → v0.9.6

1. Giữ thiết bị đang chạy **v0.9.5**; không flash v0.9.6 bằng USB trước khi thử OTA.
2. Mở portal, bật OTA và lưu cấu hình. Có thể chọn chu kỳ kiểm OTA 1 giờ để thử; lệnh Serial `OTA` vẫn cho phép kiểm ngay.
3. GitHub Actions trên nhánh/release v0.9.6 phải xanh và sinh đúng binary của panel đang dùng.
4. ThingsBoard → Advanced features → OTA updates → Add package.
5. Title: `HP20`; Version: `0.9.6`; Type: Firmware; checksum SHA-256; upload binary đúng panel SH1106/SSD1306.
6. Assign package **chỉ cho device HP20** ở lần thử đầu tiên, không rollout qua device profile.
7. Gửi lệnh Serial `OTA` để yêu cầu kiểm tra ngay hoặc chờ chu kỳ kiểm OTA.
8. Theo dõi `fw_state`: DOWNLOADING → DOWNLOADED → VERIFIED → UPDATING → UPDATED.
9. Sau reboot, Serial phải báo `HP20 v0.9.6`; Wi-Fi/token/NVS vẫn còn; telemetry tiếp tục hoạt động.
10. Chỉ sau khi bước 1–9 đạt mới tạo Git tag `v0.9.6` và GitHub Release chính thức.

## Sau khi v0.9.6 được phát hành

- v0.9.6 trở thành baseline phát hành.
- Lần kiểm OTA tiếp theo phải dùng version lớn hơn, ví dụ `0.9.7`; package cùng version sẽ bị firmware từ chối.
- Rollout rộng chỉ thực hiện sau khi single-device OTA đã qua acceptance test.

## Safety gates

- HTTPS + CA required; không insecure TLS.
- OTA chỉ chạy khi portal đóng và cloud không có request đang xử lý.
- Giới hạn binary 4 MiB ở firmware HP20.
- Sai title/version/SHA-256/size → từ chối.
- Telemetry nhường đường khi OTA đang chạy.

## Serial

`OTA` + New Line: yêu cầu kiểm tra ngay.
