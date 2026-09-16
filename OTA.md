# HP20 OTA · ThingsBoard HTTPS + Dashboard Command

## v0.9.15 acceptance — GitHub External URL, no duplicate binary

v0.9.14 đã có manual HTTPS cross-host redirect handling cho GitHub Release.
v0.9.15 dùng chính cơ chế này làm acceptance path:

```text
GitHub Actions
→ GitHub Release
→ ThingsBoard firmware package (Use external URL)
→ Dashboard RPC updateFirmware
→ HP20 v0.9.14 tải SHA256SUMS + binary trực tiếp từ GitHub
→ SHA-256 verify
→ inactive OTA partition
→ reboot v0.9.15
```

URL SH1106 chuẩn:

```text
https://github.com/huynhtandung-maker/HP20/releases/download/v0.9.15/HP20-v0.9.15-SH1106.bin
```

Không upload `.bin` lần hai vào ThingsBoard trong bài test này.


## Mục tiêu

HP20 v0.9.7 là baseline đã kiểm chứng OTA end-to-end. Nhánh `feature/v0.9.8-device-management` bổ sung lớp **device management** để người vận hành có thể kích hoạt kiểm tra/cập nhật firmware trực tiếp từ ThingsBoard Dashboard thay vì phải mở Serial Monitor và gõ `OTA`.

Nguyên tắc kiểm soát vẫn giữ nguyên:

- ThingsBoard **Assigned firmware** quyết định firmware mục tiêu.
- Nút Dashboard **không tự chọn file** và không bỏ qua version gate.
- Thiết bị chỉ cập nhật khi OTA đã được bật trong captive portal.
- HTTPS + CA + SHA-256 + size guard + inactive OTA partition vẫn bắt buộc.

## Luồng chuẩn từ v0.9.8

```text
GitHub Release (.bin)
        ↓
ThingsBoard OTA package
        ↓
Assign firmware cho HP20
        ↓
Dashboard → RPC updateFirmware
        ↓
HP20 nhận lệnh qua ThingsBoard HTTP RPC
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
        ↓
HP20 báo current_fw_version + fw_state trở lại ThingsBoard
```

## RPC chuẩn của HP20

### `updateFirmware`

Dùng cho nút **CẬP NHẬT FIRMWARE** trên Dashboard.

Thiết bị trả lời ngay khi đã nhận lệnh:

```json
{
  "accepted": true,
  "result": "OTA_CHECK_SCHEDULED",
  "currentVersion": "0.9.8",
  "otaEnabled": true
}
```

Sau đó HP20 kiểm tra firmware đang được ThingsBoard gán và chỉ tải nếu version mới hơn firmware hiện tại.

### `checkFirmware`

Alias kỹ thuật của `updateFirmware`; dùng khi muốn tên nút là **KIỂM TRA BẢN MỚI**.

### `getDeviceInfo`

Trả về firmware hiện tại, trạng thái OTA, progress và lỗi gần nhất; hữu ích khi debug widget hoặc kiểm thử RPC.

## Client attributes HP20 gửi lên ThingsBoard

| Key | Ý nghĩa |
|---|---|
| `current_fw_title` | Firmware family hiện tại, chuẩn là `HP20` |
| `current_fw_version` | Version thực tế đang chạy |
| `ota_rpc_supported` | `true` nếu firmware hỗ trợ Dashboard RPC |
| `ota_rpc_method` | RPC chính, chuẩn là `updateFirmware` |
| `fw_state` | CHECKING / DOWNLOADING / DOWNLOADED / VERIFIED / UPDATING / UPDATED / FAILED |
| `fw_progress` | 0–100 |
| `fw_error` | Chuỗi lỗi gần nhất; rỗng khi không có lỗi |

ThingsBoard tự tạo các shared attributes `fw_title`, `fw_version`, `fw_checksum`, `fw_checksum_algorithm`, `fw_size` khi firmware package được assign cho device.

## Chu kỳ nhận lệnh Dashboard

HP20 v0.9.8 giữ transport HTTPS hiện có và dùng ThingsBoard HTTP server-side RPC polling. Thiết bị kiểm tra command khoảng mỗi **10 giây**; do đó nút Dashboard không phải instant theo mili-giây nhưng thường được nhận trong một chu kỳ polling.

Đây là lựa chọn có chủ đích để không thay toàn bộ transport sang MQTT trong một patch release. Nếu về sau triển khai fleet lớn hoặc cần command realtime, bước kiến trúc tiếp theo nên chuyển control channel sang MQTT persistent connection.

## Bootstrap v0.9.7 → v0.9.8

v0.9.7 chưa có receiver cho Dashboard RPC. Vì vậy **lần chuyển từ 0.9.7 lên 0.9.8 là lần bootstrap cuối cùng** và dùng một trong hai cách:

1. OTA hiện tại bằng Serial command `OTA`, hoặc
2. USB upload trực tiếp binary v0.9.8 trong giai đoạn acceptance.

Sau khi thiết bị đã chạy v0.9.8, các bản tiếp theo có thể dùng:

```text
Assign firmware mới → Dashboard button → updateFirmware → OTA
```

## Acceptance test v0.9.8

1. Pull branch `feature/v0.9.8-device-management`.
2. Verify/compile trong Arduino IDE.
3. Upload v0.9.8 một lần để bootstrap device test.
4. Serial phải báo `HP20 v0.9.8`.
5. ThingsBoard → Attributes phải thấy `current_fw_version=0.9.8`, `ota_rpc_supported=true`.
6. Giữ Assigned firmware là 0.9.8 và bấm Dashboard RPC `updateFirmware`.
7. RPC phải trả `accepted=true`; HP20 phải đi qua `CHECKING` rồi về `UPDATED`, không reboot vì target không mới hơn.
8. Acceptance nâng version thực sự thực hiện với target kế tiếp, ví dụ 0.9.9.
9. Khi 0.9.8 ổn định mới merge vào `main`, tag `v0.9.8`, tạo GitHub Release và đính kèm đúng `.bin`.

## Safety gates

- HTTPS + CA required; không dùng insecure TLS.
- OTA phải được bật rõ ràng trong Portal.
- Nút Dashboard chỉ kích hoạt **check**, không vượt qua assignment/version/SHA gate.
- Sai title/version/SHA-256/size → từ chối.
- Firmware binary giới hạn 4 MiB.
- Telemetry nhường đường khi OTA đang download/apply.
- Serial `OTA` vẫn được giữ làm kênh service/fallback.
