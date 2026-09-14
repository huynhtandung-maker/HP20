# Changelog

## 0.3.1 — 2026-09-14

- Chốt thang HP20-VN Office v1 cho phòng làm việc Việt Nam; công khai điều kiện, câu báo và giới hạn cảm biến.
- Hạ ngưỡng ẩm cao từ 75% xuống 70%; OLED ghi rõ `VN Office` để tránh hiểu nhầm là thang HI y khoa nước ngoài.

## 0.3.0 — 2026-09-14

- GPIO25 trở thành đèn trạng thái nhiệt cảm nhận: dễ chịu sáng liên tục; bắt đầu nóng chớp đều; khó chịu chớp chậm; rất khó chịu sáng ngắn, tắt dài.
- Thêm bíp khởi động 120 ms cho còi GPIO26 và lệnh Serial `BEEP` để kiểm tra; không phụ thuộc cảnh báo cloud.
- OLED chuyển gợi ý theo trạng thái phòng làm việc: làm mát, thông gió khi không khí ngoài sạch, hút ẩm và các bước nghỉ ngơi phù hợp.
- Nhãn là hướng dẫn vận hành HP20, không phải chẩn đoán y khoa; giữ nguyên giới hạn ThingsBoard và portal.

## 0.2.0 — 2026-09-14

- Bốn trang OLED; BOOT có chống dội, đổi trang cả khi portal mở hoặc DHT22 lỗi.
- Trang cấu hình ưu tiên khi mở; bấm BOOT xem số đo mà không đóng AP.
- Serial trạng thái mỗi 10 giây, hỗ trợ cấu hình khi chưa lắp OLED.
- Giữ lịch gửi qua mất điện; lỗi lưu khóa xác thực làm dừng cloud.
- Portal hiển thị cùng phiên bản firmware; giữ sửa MBEDTLS_PRIVATE.
- CI nhắm ESP32 core 3.3.11; bổ sung kiểm thử nút và thời gian chờ.
- Ghi rõ hạn chế mô hình HI và kế hoạch kiểm chứng ngưỡng phù hợp Việt Nam.

## 0.1.1 — 2026-09-13

- OLED ưu tiên chỉ số cảm nhận và hiển thị T/RH cùng trang; hai cột ở trang cảm biến.
- Đổi trang 12 giây, bấm ngắn BOOT đổi trang, giữ BOOT mở cấu hình.
- Portal giữ nguyên trang hướng dẫn; cuộn trạng thái dài trong vùng riêng.
- LED xanh trên bo có cấu hình chân/mức bật, nhịp báo trạng thái ngắn.
- Giữ nguyên bảo vệ nguồn, thuật toán và lịch gửi ThingsBoard.

## 0.1.0 — 2026-09-13

- Bản Arduino IDE đầu tiên trong HP20.
- DHT22, OLED tự chuyển trang, driver SH1106 hoặc SSD1306.
- Heat Index có miền áp dụng và cờ hợp lệ; chưa có thang y học Việt Nam.
- Nhắc tự chọn, còi tắt mặc định, giữ ngưỡng và khoảng nghỉ.
- Wi-Fi cấu hình có mật khẩu, lưu Wi-Fi/token/CA trong NVS.
- HTTPS có xác minh chứng chỉ; giới hạn nhịp gửi, không backlog, nghỉ khi lỗi/quota.
- Kiểm tra mô hình, GitHub Actions và tài liệu sử dụng.
