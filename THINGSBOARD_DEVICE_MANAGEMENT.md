# HP20 · ThingsBoard Device Management v0.9.8

## Mục tiêu

Tạo một cụm điều khiển trên HP20 Dashboard để người vận hành nhìn thấy firmware hiện tại, firmware được gán và kích hoạt OTA mà không cần Serial Monitor.

## Data contract

### Client attributes từ HP20

- `current_fw_title`
- `current_fw_version`
- `ota_rpc_supported`
- `ota_rpc_method`
- `fw_state`
- `fw_progress`
- `fw_error`

### Shared attributes từ ThingsBoard OTA assignment

- `fw_title`
- `fw_version`
- `fw_checksum`
- `fw_checksum_algorithm`
- `fw_size`

## Action Widget

Tạo một RPC/action button trên dashboard của device HP20:

- Label: `CẬP NHẬT FIRMWARE`
- RPC method: `updateFirmware`
- Params: `{}`
- Request timeout: ít nhất `30000 ms`
- One-way: `false` / yêu cầu response
- Confirmation: bật
- Confirmation text: `Cập nhật HP20 lên firmware đang được ThingsBoard gán?`

Kỳ vọng response:

```json
{
  "accepted": true,
  "result": "OTA_CHECK_SCHEDULED",
  "currentVersion": "0.9.8",
  "otaEnabled": true
}
```

Nếu không có firmware mới hơn, thiết bị không flash lại cùng version; `fw_state` sẽ trở về `UPDATED`.

## Information Card đề xuất

Hiển thị tối thiểu:

```text
HP20 · DEVICE MANAGEMENT
────────────────────────────────
Device state       ACTIVE
Current firmware   0.9.8
Target firmware    0.9.9
OTA state          READY / CHECKING / DOWNLOADING / ...
Progress           0–100 %
Last error         —
────────────────────────────────
[ CẬP NHẬT FIRMWARE ]
```

Quy tắc màu/UX:

- `UPDATED` / `READY`: trạng thái bình thường.
- `CHECKING`: đang kiểm tra package.
- `DOWNLOADING`: hiển thị progress.
- `VERIFIED`: checksum đã hợp lệ.
- `UPDATING`: khóa nút, thông báo không ngắt nguồn.
- `FAILED`: hiển thị `fw_error` và cho phép thử lại sau khi xử lý nguyên nhân.

## Operational rule

Dashboard button chỉ là **execution trigger**. Quyền quyết định firmware nào được cài nằm ở `Assigned firmware` của ThingsBoard. Không nhúng URL GitHub, token hoặc binary path vào button.

Chuỗi chuẩn:

```text
GitHub Release
  → ThingsBoard OTA package
  → Assign firmware
  → Dashboard updateFirmware
  → Device validates
  → SHA-256 verify
  → Apply + reboot
  → Report active version
```

## Bootstrap note

v0.9.7 chưa hỗ trợ Dashboard RPC. Cần đưa device test lên v0.9.8 một lần bằng OTA legacy (`OTA` qua Serial) hoặc USB. Sau đó các version tiếp theo mới dùng button dashboard end-to-end.
