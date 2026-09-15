# HP20 Security & Secrets · v0.9.6

Repository/ZIP không được chứa secret thật:

- Wi-Fi password
- ThingsBoard device token
- private key
- `.env` thật
- `secrets.h` thật
- flash/NVS dump

## Setup AP v0.9.6

HP20 setup Wi-Fi là **open AP** để giảm ma sát onboarding. Đây là quyết định UX có giới hạn bảo vệ:

1. AP chỉ tự mở khi thiết bị chưa có Wi-Fi hoặc người dùng giữ BOOT ~3 giây.
2. Wi-Fi mất kết nối bình thường **không tự mở AP**.
3. Portal timeout sau khoảng 10 phút.
4. Request cấu hình chỉ được chấp nhận từ interface AP.
5. Form save có CSRF token; password/token đã lưu không được render đầy đủ.

Với sản phẩm thương mại triển khai rộng, có thể nâng lên QR/PIN proof-of-possession thay vì thêm password Wi-Fi tạm thời khó dùng.

## Credential flow

```text
NVS hợp lệ → dùng NVS
secrets.h local + PROFILE_REVISION mới → seed một lần → NVS
không có SSID → captive portal → NVS
```

`secrets.example.h` chỉ chứa placeholder. `secrets.h` bị `.gitignore` chặn và cố ý không có trong ZIP phát hành.

## OTA security

- OTA mặc định OFF.
- HTTPS có xác minh CA; không `setInsecure()`.
- Chỉ title `HP20` và version mới hơn.
- Checksum bắt buộc SHA-256 64 hex.
- Binary chỉ được kích hoạt sau khi checksum khớp.
- OTA không cần GitHub token trên ESP32.

## Giới hạn

Preferences/NVS hiện chưa được coi là storage chống trích xuất vật lý. Sản phẩm thương mại cần đánh giá Secure Boot, Flash Encryption, provisioning identity và signing policy riêng.
