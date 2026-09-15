# Changelog

## v0.9.5 FIX2 — Built-in ThingsBoard Cloud CA

- Bundled ISRG Root X1 for `thingsboard.cloud`; no PEM paste required for normal onboarding.
- Custom CA in NVS still overrides built-in trust for custom/self-hosted ThingsBoard.
- Cloud and OTA use the same effective CA policy.
- Portal explains CA as advanced/custom-host only.

## v0.9.5 — Provisioning UX + secure OTA

- Thêm hybrid provisioning: NVS → local `secrets.h` → captive portal fallback.
- `secrets.h` không lên GitHub; thêm `secrets.example.h` làm mẫu.
- Portal được thiết kế lại thành 3 bước, scan SSID, show/hide dữ liệu mới đang nhập, review trước khi lưu.
- Password/token đã lưu không bị render ngược ra browser; token chỉ hiện 4 ký tự cuối.
- Thêm cấu hình OTA opt-in và chu kỳ kiểm tra 1–24 giờ.
- Thêm `hp20_ota.*`: ThingsBoard HTTPS OTA, version gate, SHA-256, size guard, OTA partition và state reporting.
- OLED/Serial bổ sung trạng thái OTA và lệnh `OTA` để kiểm tra ngay.
- CI tạo binary artifact SH1106/SSD1306 để dùng cho ThingsBoard OTA.
- NVS vẫn là nguồn cấu hình chính sau lần provisioning đầu tiên; mất điện không yêu cầu nhập lại.

## v0.9.4 — Repository-ready controlled baseline

- Tách firmware version thành `hp20_version.h` làm single source of truth.
- OLED footer, Serial boot và captive portal cùng đọc một firmware version.
- Giữ modular architecture: sensor / thermal / trend / indicator / UI / cloud / portal.
- Thermal threshold block được chú thích chi tiết để tự tuning cho người dùng Việt Nam.
- Green LED biểu diễn comfort quality: càng kém thoải mái, màu xanh xuất hiện càng ít.
- OLED 7 trang: tối đa một marquee trên mỗi trang nội dung.
- Bổ sung host regression cho green LED và firmware identity.
- Chuẩn hóa `.gitignore`, secrets policy, release checklist và GitHub Actions.
- Không thay đổi Wi-Fi/password/token flow: credential thật vẫn nhập qua portal và lưu NVS.

# HP20 Changelog

## v0.9.1 — Clean baseline / no behavior change

- Dọn source sau khi modular hóa v0.9.0.
- Xóa `model::RoomBand/roomBand()` legacy không còn được runtime sử dụng.
- Xóa `settings::UI_PAGE_MS` không còn dùng.
- Xóa API `hp20::trend::advice()` không được gọi.
- Chuyển `scaleXForFeel()` (pixel mapping) từ thermal domain sang private UI helper.
- Host tests chuyển sang kiểm tra trực tiếp thermal/trend hiện hành.
- Giữ nguyên thermal model `HP20-SG-HI-v0.9.0`.
- Giữ nguyên calibration: Temperature `-7.0%`, Humidity `+6.8%`.
- Không thay đổi Wi‑Fi, portal, ThingsBoard, reminder/buzzer, rate-limit hay OLED behavior.

# Changelog

# CHANGELOG — HP20

## v0.8.3 — Release sync / housekeeping

- Đồng bộ `settings::VERSION` lên `0.8.3`.
- Đồng bộ `HP20_SG_HI_MODEL` lên `HP20-SG-HI-v0.8.3`.
- Giữ nguyên hành vi chức năng của v0.8.2.
- Giữ calibration hiện tại của người dùng: Temperature `-9.0%`, Humidity `+4.5%`.
- Cập nhật README theo kiến trúc 7 tab OLED, chart, trend 10 phút và calibration.
- Cập nhật SCIENCE để phân biệt FEEL/apparent heat với nhiệt độ cơ thể/WBGT.
- Ghi rõ `model::RoomBand` là legacy/compatibility và không phải nguồn band UX hiện hành.
- Không sửa cloud, portal, config, token, CA, retry, 429 cooldown hay ThingsBoard send budget.

## v0.8.2

- Thêm hiệu chỉnh % độc lập cho Temperature và Humidity DHT22.
- Giữ raw values cho Serial diagnostics.
- Giới hạn calibration ±20%.

## v0.8.1

- Thêm trend FEEL khoảng 10 phút.
- Thêm sparkline và trạng thái tăng/giảm/ổn định.

## v0.8.0

- Thêm scale chart để hiển thị vị trí FEEL tương đối với vùng mục tiêu.
- Dùng thời lượng trang động cho marquee.

## v0.7.x

- Mở rộng band FEEL và tái thiết kế OLED UX.



## 0.4.0 — 2026-09-14

- Mở rộng HP20-VN Office thành 8 mức: mát, dễ chịu, hơi ấm, ấm, nóng ẩm, khó chịu, rất nóng và rất khó chịu.
- OLED thành năm tab tối giản; mỗi tab có một mục tiêu đọc, chỉ Tab nhận định/hành động dùng một dòng cuộn.
- Điều chỉnh thời lượng: chỉ số 8 giây, nhận định/hành động 16 giây, kết nối 12 giây.

## 0.3.2 — 2026-09-14

- Thiết kế lại OLED theo bốn tab: chỉ số chính, khuyến nghị thời gian thực, số đo môi trường, kết nối/cài đặt.
- Khuyến nghị dùng hai dòng cuộn độc lập, có nhận định và hành động theo HP20-VN Office; giữ 18 giây để đọc.
- Chia thời lượng tab: chỉ số/số đo 10 giây, khuyến nghị 18 giây, kết nối 14 giây; BOOT vẫn đổi ngay.

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
