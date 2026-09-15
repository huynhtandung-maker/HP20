# HP20 OTA · ThingsBoard HTTPS

## Mục tiêu

HP20 v0.9.5 hỗ trợ OTA từ xa qua ThingsBoard nhưng **mặc định tắt**. OTA dùng cùng host/token/CA TLS với telemetry và không yêu cầu GitHub token trên thiết bị.

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

## ThingsBoard setup

1. Flash v0.9.5 bằng USB trước.
2. Bật OTA trong portal hoặc local `secrets.h`.
3. Khi muốn thử OTA, build **v0.9.6**. Không thể kiểm OTA bằng package cùng version v0.9.5.
4. ThingsBoard → Advanced features → OTA updates → Add package.
5. Title: `HP20`; Version: `0.9.6`; Type: Firmware; checksum SHA-256; upload binary đúng panel SH1106/SSD1306.
6. Assign package cho riêng device HP20 trước khi cân nhắc profile/fleet.
7. Theo dõi `fw_state`: DOWNLOADING → DOWNLOADED → VERIFIED → UPDATING → UPDATED.

## Safety gates

- HTTPS + CA required; không insecure TLS.
- OTA chỉ chạy khi portal đóng và cloud không có request đang xử lý.
- Giới hạn binary 4 MiB ở firmware HP20.
- Sai title/version/SHA-256/size → từ chối.
- Telemetry nhường đường khi OTA đang chạy.

## Serial

`OTA` + New Line: yêu cầu kiểm tra ngay.
