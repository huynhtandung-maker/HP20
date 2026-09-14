# HP20 v0.3.0 — giao diện và chẩn đoán

## OLED 128×64

1. Cảm nhận nóng HI: số lớn, T/RH bên dưới; ngoài miền hiện -- và lý do.
2. Phòng / gợi ý: T/RH hoặc hành động khi vượt ngưỡng nhắc tự chọn.
3. Wi-Fi / ThingsBoard: tên mạng, trạng thái gửi, chu kỳ và thời gian chờ tối thiểu.
4. Thiết bị / cài đặt: phiên bản, IP; khi mở AP hiện tên AP, mật khẩu tạm và địa chỉ cấu hình.

Header 0–11 px, nội dung 16–51 px, footer từ 54 px; font 5×7 cho thông tin dài,
font số lớn cho chỉ số chính. Các dòng dài cuộn trong vùng riêng, footer có số trang 1/4–4/4.
Không dùng màu/nhãn an toàn để diễn giải một ngưỡng y tế chưa được kiểm chứng.

Bấm ngắn BOOT đổi trang kể cả khi AP mở hoặc cảm biến lỗi. Giữ khoảng 3 giây mở AP.
Trang AP giữ để nhập cấu hình; người dùng bấm ngắn có thể rời trang mà AP vẫn hoạt động.
Các trang khác tự chuyển mỗi 12 giây. AP hết hạn sau 10 phút;
nếu vẫn mất Wi-Fi, hệ thống chờ thêm 2 phút rồi mở phiên mới với mật khẩu mới.

## Khi chưa lắp OLED

Serial Monitor: 115200 baud, New Line. Mỗi 10 giây có STATUS gồm số đo,
trạng thái cảm biến, Wi-Fi, portal, trang, nút, cloud và thời gian chờ tối thiểu.
Không tự in mật khẩu Wi-Fi đã lưu, token, CA hoặc SSID mạng nhà.

Lệnh SETUP mở AP và in tên/mật khẩu AP tạm trên Serial để cấu hình bằng điện thoại.
Chỉ lệnh do người dùng nhập này mới in mật khẩu AP; không gửi ảnh dòng đó công khai.
Lệnh INFO yêu cầu in trạng thái ở vòng lặp tiếp theo. Không có lệnh gửi cloud ngay.
Nếu DHT22 chưa lắp, số đo INVALID/nan là dự kiến; không gửi số giả lên ThingsBoard.

## LED / còi

GPIO25 là LED chỉ báo điều kiện phòng: **dễ chịu** sáng liên tục; **bắt đầu nóng** chớp đều nhanh; **khó chịu** chớp chậm; **rất khó chịu** sáng 350 ms, tắt 1450 ms. Ẩm cao, mát hoặc lỗi cảm biến có nhịp riêng để tránh hiểu nhầm là dễ chịu.

Còi GPIO26 bíp 120 ms khi khởi động và in `BUZZER boot test: ON/OFF` trên Serial. Có thể nhập `BEEP` với New Line để thử lại. Còi nhắc định kỳ vẫn chỉ hoạt động khi người dùng đã bật nhắc và âm thanh trong portal. LED bo GPIO2 là chỉ báo kỹ thuật riêng.

## Nghiệm thu

Xem VALIDATION.md để biết kiểm tra phần mềm đã chạy. OLED chưa lắp theo người dùng,
chưa thể nghiệm thu driver SH1106, bố cục trên tấm nền, GPIO2 hay nút BOOT vật lý.
Không tự upload COM13. Giữ Erase All Flash Before Sketch Upload = Disabled.
