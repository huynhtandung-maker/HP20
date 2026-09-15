# ThingsBoard send budget · HP20 v0.9.6

HP20 chuẩn hóa telemetry **5 phút/lần** sau giai đoạn xác nhận kết nối ban đầu.

Ước lượng tối đa lý thuyết cho một thiết bị khi chạy liên tục:

- 12 lần/giờ
- 288 lần/24 giờ
- 8.928 lần/31 ngày
- payload hiện có tối đa 4 key: `temperature`, `humidity`, `heat_index_c`, `heat_index_valid`
- tương đương tối đa 35.712 key-value samples/31 ngày nếu mỗi lần có đủ 4 key

Đây chỉ là ước lượng firmware, **không phải cam kết cách ThingsBoard tính quota/billing**. Các rule chain, OTA, attribute traffic và thiết bị khác có thể dùng chung quota tài khoản.

Bảo vệ anti-spam hiện có:

- Không backlog dữ liệu sau mất điện.
- Chỉ một cloud request đang xử lý.
- 401/403 → khóa auth cho tới khi user chủ động lưu config mới.
- 429 → cooldown 24 giờ.
- Lỗi khác → exponential backoff tới giới hạn hiện hành.
- Không Wi-Fi / thời gian / sensor / CA hợp lệ → không gửi.
