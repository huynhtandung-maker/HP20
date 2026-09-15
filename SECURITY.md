# HP20 Security & Secrets

Repository hiện là **public**. Quy tắc mặc định: coi mọi thứ commit lên GitHub là công khai vĩnh viễn.

## Secrets không được commit

- Wi‑Fi password
- ThingsBoard device token
- private key
- `.env` thật
- `secrets.h` thật
- NVS/flash dump
- log/screenshot có credential nhạy cảm

`.gitignore` chặn các loại phổ biến, nhưng `.gitignore` không thay thế việc kiểm tra `git diff` trước commit.

## Runtime credential flow

```text
NVS đã có → dùng NVS
secrets.h local có PROFILE_REVISION mới hơn → apply một lần vào NVS
NVS/portal giữ quyền ưu tiên sau đó
không có SSID → Captive Portal → NVS
```

`secrets.h` là file local bị `.gitignore` chặn. `secrets.example.h` chỉ chứa placeholder công khai. Portal không render lại password/token đã lưu; nút show/hide chỉ áp dụng cho giá trị mới đang nhập.

`.env.example` chỉ là reference; firmware không đọc `.env`. Root CA là trust material công khai, nhưng vẫn phải lấy từ nguồn ThingsBoard tin cậy và kiểm soát thay đổi.

## Nếu secret từng xuất hiện trên repo public

1. Xem secret là đã lộ.
2. Rotate/thay Wi‑Fi password hoặc ThingsBoard token ngay.
3. Sau đó mới xem xét làm sạch Git history nếu cần.

Chỉ xóa secret ở commit mới **không làm secret biến mất khỏi lịch sử Git**.

## Giới hạn hiện tại

Preferences/NVS baseline chưa được coi là storage chống trích xuất vật lý. Với sản phẩm thương mại, cần đánh giá Secure Boot, Flash Encryption, provisioning và device identity riêng.


## OTA security

- OTA mặc định **OFF** cho tới khi owner bật.
- Transport dùng HTTPS với CA đã cấu hình; không dùng `setInsecure()`.
- Chỉ package title `HP20` và version mới hơn mới được xem xét.
- Chỉ chấp nhận checksum `SHA256` 64 hex và kích thước hợp lý.
- Binary được ghi vào OTA partition nhưng chỉ `Update.end()` sau khi checksum khớp.
- Không commit access token vào GitHub để phục vụ OTA.
