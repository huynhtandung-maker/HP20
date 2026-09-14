# Kiểm thử HP20

## Tự động

Trên Linux/CI: `g++ -std=c++11 -Wall -Wextra tests/model_test.cpp -o /tmp/hp20-model-test && /tmp/hp20-model-test`.
Trên Windows, đặt tệp test biên dịch trong Temp, không đặt trong sketch.

Kiểm tra điểm HI đối chiếu bảng NWS, đầu vào lỗi, ngoài miền, RH tăng,
tràn bộ đếm millis, thời gian giữ ngưỡng, độ trễ thoát, reset do lỗi cảm biến,
tăng khoảng chờ, chống dội/giữ/thả BOOT và mô phỏng 31 ngày có khởi động lại. GitHub Actions biên dịch thêm hai driver OLED trên ESP32 core 3.3.11.
Xem `VALIDATION.md` để biết kiểm tra nào thực sự đã chạy.

## Trên phần cứng — chưa thực hiện

| Ca kiểm tra | Kết quả phải đạt |
|---|---|
| Lần đầu chưa cấu hình | AP có mật khẩu; OLED hiển thị SSID/PW/địa chỉ |
| Sai mật khẩu Wi-Fi | Không xóa cấu hình; có thể sửa lại bằng portal |
| Router mất điện lâu hơn ESP32 | ESP32 vẫn đo, kết nối lại khi router sẵn sàng |
| Rút điện/cắm lại | Cấu hình giữ nguyên; không phát sinh gửi ngay |
| Mất điện sau một lần gửi | Giữ mốc chờ và chờ thêm chu kỳ khởi động |
| Mạng mới | Giữ BOOT 3 giây mở portal; lưu và nối được mạng mới |
| Không có token/CA | Đo tại chỗ; không phát yêu cầu cloud |
| TLS sai CA / mất NTP | Không gửi token qua kết nối không xác thực |
| HTTP 401/403 | Dừng đến khi lưu cấu hình; không lặp xác thực |
| HTTP 429 | Dừng 24h, bao gồm qua reset |
| Cloud chậm | OLED/cảm biến vẫn cập nhật |
| DHT22 bị rút | Hiển thị lỗi, không gửi số cũ, không tính HI |
| Ngoài miền mô hình | HI = không áp dụng; khóa validity=false trên cloud |
| Nhắc bật, vượt ngưỡng <2 phút | Không báo |
| Nhắc bật, vượt ngưỡng ≥2 phút | LED nhắc; còi chỉ khi được bật |
| Dao động quanh ngưỡng | Không bật/tắt liên tục; thoát dưới ngưỡng 1°C |
| Portal qua IP LAN nhà | Bị từ chối |
| Thiếu/sai CSRF | Không lưu cấu hình |
| Chạy ít nhất 24h | Không reset bất thường; số yêu cầu đúng ngân sách |

Xác nhận OLED 128×64/driver, nguồn còi và dòng tải trước khi nghiệm thu.
