# ThingsBoard Cloud Free — ngân sách gửi

Ngày đối chiếu: 2026-09-13. Người dùng xác nhận Cloud Free; chưa xác định hostname/vùng.
Tài liệu Cloud EU công bố Free 0,5 triệu transport messages và 1 triệu transport data points
mỗi kỳ thuê bao. Đây là tham chiếu, không thay cho Usage/Limits của tài khoản thực tế:
https://thingsboard.io/docs/paas/eu/reference/subscriptions/

HP20 giữ chu kỳ tối thiểu 15 phút: tối đa xấp xỉ 96 lần thử/24 giờ, 2.976 lần/31 ngày.
Mỗi payload hiện có tối đa 4 trường: temperature, humidity, heat_index_c, heat_index_valid.
Ở chu kỳ 15 phút là tối đa 11.904 điểm dữ liệu/31 ngày cho một thiết bị.
Mỗi tin còn có thể sinh nhiều lượt rule engine; các thiết bị khác dùng chung quota tài khoản.
Không tăng tốc gửi chỉ vì quota tham chiếu lớn. Access token là thông tin xác thực,
không phải một bộ đếm quota riêng mà thiết bị có thể tự kiểm tra toàn bộ.

- Khởi động lại: luôn chờ đủ một chu kỳ; không gửi bù dữ liệu trong thời gian mất điện.
- Ghi mốc lần kế tiếp vào NVS trước khi gửi; phải thỏa cả thời gian chờ từ lúc khởi động và mốc đã lưu.
- Chỉ một yêu cầu đang gửi; tác vụ mạng riêng để không chặn vòng lặp giao diện.
- Không có cảm biến hợp lệ, Wi-Fi, đồng hồ hoặc CA TLS: không gửi.
- HTTP 401/403: khóa gửi, lưu qua reset, chỉ thử lại sau lưu cấu hình chủ động.
- HTTP 429: nghỉ ít nhất 24 giờ theo mốc lưu; đổi cấu hình không xóa mốc này.
- Lỗi khác: tăng thời gian chờ, tối đa 6 giờ hoặc chu kỳ cấu hình nếu lớn hơn.
- Lỗi lưu mốc/khóa: dừng cloud trong phiên hiện tại.

Chưa kết nối tài khoản thật, chưa xác nhận Usage, Rule Chains hoặc phản hồi HTTP thực tế.
Không cam kết tài khoản không bị hạn chế bởi hoạt động từ thiết bị/ứng dụng khác.
