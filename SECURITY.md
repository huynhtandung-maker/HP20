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
User → Captive Portal → Config → ESP32 Preferences/NVS
```

Firmware source không cần Wi‑Fi password/token. Portal không hiển thị lại password/token đã lưu.

`.env.example` chỉ là reference; firmware không đọc `.env`.

## Nếu secret từng xuất hiện trên repo public

1. Xem secret là đã lộ.
2. Rotate/thay Wi‑Fi password hoặc ThingsBoard token ngay.
3. Sau đó mới xem xét làm sạch Git history nếu cần.

Chỉ xóa secret ở commit mới **không làm secret biến mất khỏi lịch sử Git**.

## Giới hạn hiện tại

Preferences/NVS baseline chưa được coi là storage chống trích xuất vật lý. Với sản phẩm thương mại, cần đánh giá Secure Boot, Flash Encryption, provisioning và device identity riêng.
