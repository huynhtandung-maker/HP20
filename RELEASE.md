# HP20 Release Process

HP20 dùng quy trình **manual-control-first**. AI/automation hỗ trợ kiểm tra nhưng không tự merge/tag/publish release khi chưa được người quản trị dự án duyệt.

## 1. Version authority — Single Source of Truth

Chỉ sửa version tại `hp20_version.h`.

`hp20_version.h` là nguồn duy nhất cho:

- `MAJOR`, `MINOR`, `PATCH`
- `STRING`
- `TAG`
- firmware identity hiển thị trên thiết bị
- current firmware attributes gửi ThingsBoard
- tên binary do CI sinh

Không được hard-code "current version" trong test, dashboard, OTA logic, tài liệu vận hành hoặc workflow build. Test chỉ được kiểm tra tính nhất quán giữa `MAJOR.MINOR.PATCH`, `STRING` và `TAG`.

Quy tắc SemVer:

- PATCH: bug fix, UX refinement, docs/repo cleanup.
- MINOR: capability mới nhưng tương thích.
- MAJOR: thay đổi không tương thích về cấu hình/kiến trúc sản phẩm.

## 2. Local validation

1. Arduino Verify.
2. Upload firmware vào ESP32 test khi thay đổi ảnh hưởng runtime/hardware.
3. Kiểm tra OLED, DHT22, LED, buzzer, BOOT portal và Wi-Fi provisioning theo phạm vi thay đổi.
4. Power cycle và xác nhận config/NVS giữ nguyên.
5. Nếu thay đổi cloud/OTA: kiểm tra telemetry, TLS, ThingsBoard attributes/RPC và OTA gate.

## 3. Git review

```powershell
git status
git diff --stat
git diff
```

Không commit secret hoặc build artifact local. `build/`, `*.bin`, `secrets.h` phải tiếp tục nằm ngoài Git.

## 4. Branch + commit

Không sửa release trực tiếp trên `main`.

```powershell
git checkout -b fix/<topic>
git add .
git commit -m "fix: <mô tả>"
git push origin fix/<topic>
```

Tạo Pull Request vào `main`.

## 5. CI release gate

GitHub Actions phải xanh trước khi merge:

- host regression tests
- ESP32 SH1106 compile
- ESP32 SSD1306 compile
- OTA-ready firmware artifacts
- SHA-256 manifest

Nếu một check đỏ: **không merge, không tag, không publish release, không đưa binary local lên ThingsBoard production**.

Binary canonical của release phải là binary sinh từ CI của đúng source commit đã được chấp nhận. Binary build thủ công chỉ dùng cho local validation/debug.

## 6. Merge → verify main → tag

Sau khi PR pass và phần cứng thật đã đạt acceptance:

1. Merge PR vào `main`.
2. Chờ CI của chính merge commit trên `main` xanh.
3. Xác nhận version trong `hp20_version.h` là version dự kiến phát hành.
4. Tạo Git tag đúng version.

Ví dụ nếu `hp20_version.h` là `0.9.9`:

```powershell
git checkout main
git pull
git tag -a v0.9.9 -m "HP20 v0.9.9"
git push origin v0.9.9
```

Git tag phải bằng `v` + `hp20::version::STRING`.

## 7. GitHub Release

Release phải trỏ đúng tag vừa tạo và đính kèm binary CI tương ứng, ví dụ:

```text
HP20-v0.9.9-SH1106.bin
HP20-v0.9.9-SSD1306.bin
SHA256SUMS.txt
```

Không publish draft release nếu CI của source commit đang đỏ hoặc asset chưa được xác nhận provenance/hash.

## 8. ThingsBoard OTA rollout

Chuỗi chuẩn:

```text
hp20_version.h
  → source commit
  → green CI
  → canonical binary + SHA-256
  → Git tag
  → GitHub Release
  → ThingsBoard OTA package
  → Assign firmware
  → Dashboard RPC updateFirmware
  → Device verifies title/version/size/SHA-256
  → Apply + reboot
  → report current_fw_version
```

ThingsBoard chỉ được assign package có `fw_title = HP20` và version mới hơn firmware đang chạy, trừ khi có quy trình rollback riêng được thiết kế và kiểm chứng.

## 9. Release acceptance / kết thúc vòng

Một release chỉ được coi là hoàn tất khi:

- CI `main` xanh.
- Git tag == `hp20_version.h`.
- GitHub Release dùng canonical CI binary.
- ThingsBoard target đúng version release.
- OTA end-to-end thành công trên device thật.
- Sau reboot `current_fw_version == fw_version`.
- `fw_state = UPDATED`, `fw_error` rỗng, telemetry hoạt động bình thường.

Sau đó mới dọn branch đã merge và chuyển version tiếp theo sang một branch công việc mới.
