# Kiểm chứng HP20 v0.2.0

Ngày: 2026-09-14. Làm trực tiếp trong E:/IOT/ARDUINO/Tu_Hoc/HP20, không worktree.

## Đã đối chiếu

- Bản sửa MBEDTLS_PRIVATE được giữ; portal chỉ thay nhãn phiên bản và xóa khoảng trắng cuối dòng.
- config.cpp/config.h/cloud.cpp/cloud.h và mô hình heatIndex giữ nguyên cơ chế hiện có.
- Không đọc hoặc thay Wi-Fi/token trong NVS, không erase flash, không upload COM13.
- Không có OLED theo thông tin người dùng; UI/driver/chân LED chưa được nghiệm thu thực tế.

## Kiểm tra tự động

Host test: PASS, chạy bằng Zig 0.13.0 (C++11, -Wall -Wextra) trên Windows.
Đã kiểm tra miền HI, đầu vào lỗi, hysteresis, chống dội/giữ/thả BOOT,
tràn millis, thời gian chờ, mô phỏng 31 ngày reset mỗi giờ và reset mỗi 10 phút.
Phép tính thời gian chờ được dùng trực tiếp để quyết định gửi trong firmware.
Mô phỏng không thay cho thử mất điện/NVS hoặc HTTP 401/403/429 trên bo thật.
Kết quả build ESP32 cuối được bổ sung bên dưới sau khi tiến trình kết thúc.
Build, log và công cụ kiểm thử đều ở thư mục Temp ngoài sketch.
CI nhắm ESP32 core 3.3.11 và cả SH1106/SSD1306; chưa có lần chạy GitHub Actions.

## Ca nghiệm thu phần cứng cần chạy

1. Sau upload có HP20 v0.2.0 boot, sau 10 giây có STATUS; không reset lặp.
2. DHT22 đọc được T/RH; tháo cảm biến khi tắt nguồn rồi bật lại phải INVALID, không gửi số cũ.
3. BOOT bấm ngắn in đúng một sự kiện; giữ khoảng 3 giây chỉ mở cấu hình, thả không đổi trang thêm.
4. Serial SETUP + New Line cho AP tạm; điện thoại mở 192.168.4.1 và nhập Wi-Fi mới.
5. Token để trống khi lưu giữ token cũ; mật khẩu trống chỉ giữ khi SSID không đổi.
6. Chuyển địa điểm/mất Wi-Fi: sau 2 phút mở AP; quá hạn có thể mở lại bằng SETUP hoặc BOOT.
7. Mất điện trước/sau gửi: không gửi ngay khi khởi động, không gửi bù, vẫn giữ cấu hình.
8. HTTP 401/403/429 phải kiểm thử trên máy chủ giả lập, không cố tình gây lỗi lặp trên Cloud Free.
9. Lắp OLED đúng driver: bấm chuyển đủ 4 trang cả khi portal mở hoặc cảm biến lỗi; không tràn chữ.
10. Chạy 24 giờ và đối chiếu Usage Cloud với lịch gửi; chưa tuyên bố đạt độ ổn định dài hạn.
