> **Phiên bản hiện tại: 0.2.0.** Phần cứng hiện chưa lắp OLED: dùng Serial 115200,
> chọn New Line, nhập `SETUP` để mở cấu hình bằng điện thoại; `INFO` xem trạng thái.
> Không cần gửi token/mật khẩu cho người hỗ trợ. Firmware chưa tự upload.
>
> [Giao diện](UI_CHANGES.md) · [Kiểm chứng](VALIDATION.md) · [Cơ sở khoa học](SCIENCE.md)
> · [Ngân sách ThingsBoard Free](THINGSBOARD_BUDGET.md)
>
> Wi-Fi/token thực được nhập qua portal và lưu NVS trên ESP32, không nằm trong mã.
> `.env.example` chỉ là mẫu ghi chú; Arduino không tự đọc `.env`. Không commit bản `.env`
> chứa dữ liệu thật. NVS mặc định chưa mã hóa; sản phẩm triển khai thực cần đánh giá bảo vệ vật lý.

# HP20 · Room Monitor · v0.2.0

Dự án Arduino IDE cho ESP32 Dev Module, DHT22, OLED I2C 128×64, LED đơn và còi.
Đo trong phòng, hiển thị nhiệt độ cảm nhận **ước tính**, cấu hình Wi-Fi/ThingsBoard bằng điện thoại.
Đây là bản thử nghiệm kỹ thuật, chưa được xác nhận trên phần cứng của bạn.

## Mở và biên dịch

Mở `HP20.ino` trong Arduino IDE. Các file `.h/.cpp` phải nằm cùng thư mục HP20.
Nếu IDE vẫn hiện tab trống sau khi file được sửa từ ngoài, đóng rồi mở lại sketch;
không lưu đè tab trống lên code mới.

- Board: **ESP32 Dev Module** (`esp32:esp32:esp32`).
- Cổng hiện tại **COM13**; kiểm tra lại khi rút/cắm USB.
- Môi trường biên dịch: Arduino ESP32 **3.3.11**. Xem kết quả cụ thể trong `VALIDATION.md`.
- Library Manager: DHT sensor library **1.4.7**, Adafruit Unified Sensor **1.1.15**,
  U8g2 **2.36.19**, ArduinoJson **7.4.3**.
- Nhấn Verify trước, Upload sau khi đấu dây đúng. Chưa tự động nạp firmware vào bo mạch.

## Đấu dây

Ngắt nguồn khi đấu dây. Tất cả linh kiện phải chung GND.

| Linh kiện | ESP32 / cách nối |
|---|---|
| DHT22 module chân + | 3V3 |
| DHT22 module chân − | GND |
| DHT22 chân tín hiệu | GPIO27; xác định theo nhãn module, không suy từ thứ tự ảnh |
| OLED VDD | 3V3, sau khi kiểm tra module hỗ trợ |
| OLED GND | GND |
| OLED SCK/SCL | GPIO22 |
| OLED SDA | GPIO21 |
| LED anode | GPIO25 qua điện trở nối tiếp 330 Ω |
| LED cathode | GND |
| Còi | Điều khiển qua transistor/MOSFET từ GPIO26; **không đấu còi chưa rõ dòng trực tiếp vào GPIO** |

Còi 2 chân chưa xác định điện áp/dòng. Chọn nguồn đúng định mức của còi và tầng công suất phù hợp.
Với NPN: GPIO26 → điện trở base → base, emitter → GND, collector → cực − còi,
cực + còi → nguồn định mức. Chọn điện trở theo transistor và dòng còi thực tế;
thêm diode dập xung nếu là tải điện từ. Chưa cấp nguồn cho còi khi chưa xác định định mức.
Mặc định `PASSIVE_BUZZER=false` trong `settings.h`; đổi thành `true` nếu còi cần PWM.
Giữ còi chưa đấu vẫn chạy được phần cảm biến, OLED, mạng.

Mặc định driver **SH1106**. Nếu màn hình dùng SSD1306, đổi `OLED_SH1106` thành `0`
trong HP20.ino. Code tìm địa chỉ 0x3C/0x3D nhưng không tự xác định loại chip hay độ phân giải.
Đặt DHT22 tách khỏi ESP32, ổn áp, ánh nắng và luồng khí nóng để giảm sai lệch.

## Sử dụng

1. Chưa có OLED: mở Serial Monitor **115200 baud**, chọn **New Line**, nhập `SETUP` và Enter.
   Serial hiện tên AP, mật khẩu tạm và địa chỉ `192.168.4.1`. Nếu đã lắp OLED, các thông tin này có trên trang cài đặt.
2. Điện thoại kết nối mạng đó; chấp nhận ở lại mạng không có Internet. Mở `http://192.168.4.1`.
3. Nhập SSID/mật khẩu Wi-Fi 2.4 GHz. Có thể để ThingsBoard trống và dùng đo tại chỗ.
4. Nếu có ThingsBoard, nhập hostname HTTPS, token và CA gốc PEM tin cậy cho **hostname HTTPS đó**.
   Không dùng hostname MQTT như `mqtt.thingsboard.cloud` cho API HTTPS.
5. Lưu. Nếu kết nối thành công, AP đóng sau khoảng 30 giây; nếu thất bại, sửa trong portal.
6. Bất cứ lúc nào giữ **BOOT 3 giây khi thiết bị đang chạy** để mở cấu hình và đổi token.
   Không giữ BOOT trong lúc reset/cấp điện vì có thể vào chế độ nạp chương trình.

Portal tự đóng sau 10 phút; giữ BOOT hoặc nhập `SETUP` để mở lại. Nếu vẫn offline,
thiết bị chờ thêm 2 phút rồi tự mở phiên mới. Mật khẩu AP thay đổi mỗi phiên mở mới.
Sau mất điện, Wi-Fi/token giữ nguyên; ESP32 tự chạy lại và chờ router. Sau 2 phút không kết nối,
AP cấu hình mở, đồng thời thiết bị tiếp tục thử Wi-Fi. Không tự xóa cấu hình.
Trong chế độ AP, mật khẩu được hiển thị là **mật khẩu mạng cấu hình ESP32**, không phải mật khẩu Wi-Fi nhà.

Bấm ngắn BOOT chuyển đủ bốn trang, kể cả khi portal mở hoặc cảm biến lỗi.
Serial in STATUS mỗi 10 giây; `INFO` in trạng thái ngay ở vòng lặp kế tiếp.
`nan` kèm `sensor=INVALID` nghĩa là chưa có số đo hợp lệ, không phải số đo bằng 0.
Không có lệnh gửi ThingsBoard ngay để vượt qua lịch chờ.

## ThingsBoard và ngân sách gửi

HTTPS `/api/v1/{token}/telemetry`, có kiểm tra CA và hostname, không tắt xác minh TLS.
Cần đồng bộ đồng hồ qua NTP trước khi gửi. Không có thời gian chính xác thì chỉ đo tại chỗ.
CA là chứng chỉ công khai, không phải token. Lấy CA phù hợp từ quản trị/tài liệu máy chủ HTTPS;
không mặc định CA của cổng MQTT cũng đúng với cổng HTTPS.

- DHT22 đọc mỗi 2,5 giây; OLED cập nhật tối đa 5 khung/giây, tự chuyển trang mỗi 12 giây; trang AP chờ người dùng bấm BOOT.
- Gửi **15 phút/lần mặc định**, chỉ cho phép tăng tới 1440 phút.
- Mỗi lần khởi động chờ đủ một chu kỳ; không gửi ngay và không gửi dữ liệu tồn đọng.
- Giữ lịch chờ trong NVS trước khi gửi để reset không vượt qua thời gian chờ đã lưu.
- Một tác vụ HTTPS độc lập, tối đa một lần gửi đang chạy; UI tiếp tục hoạt động khi TLS chậm.
- HTTP 2xx: đã nhận phản hồi thành công. HTTP 401/403: dừng đến khi người dùng lưu cấu hình.
- HTTP 429: dừng ít nhất 24 giờ, giữ mốc qua khởi động lại.
- Lỗi khác: tăng khoảng chờ lên tối đa 6 giờ (hoặc chu kỳ người dùng nếu dài hơn).
- Tối đa khoảng 96 yêu cầu/ngày ở nhịp mặc định, 2.880 yêu cầu/30 ngày.
  Payload có 3–4 khóa: tối đa 11.520 điểm dữ liệu/30 ngày nếu mỗi khóa tính một điểm.
  **Đây là tính toán tải của firmware, không phải cam kết phù hợp mọi gói Free.**
  Cần đối chiếu usage, các thiết bị khác và rule engine trong tài khoản của bạn.

Telemetry: `temperature`, `humidity`, `heat_index_c` (chỉ khi hợp lệ), `heat_index_valid`.
Dashboard phải dùng `heat_index_valid` để ẩn giá trị HI cũ khi giá trị mới không áp dụng.
Không gửi Wi-Fi password/token trong telemetry. Không gửi lại ngay khi có cảnh báo.

## Chỉ số cảm nhận và khuyến cáo

Xem `SCIENCE.md`. HI là mô hình NWS được ghi tên rõ ràng, **không phải thang nguy cơ Mỹ/Âu**.
Không áp dụng các dải phân loại nguy cơ của NWS và không gọi là chuẩn y học Việt Nam.
Miền tính sản phẩm: 26,7–50°C, RH 40–100%; ngoài miền hiển thị `--` kèm lý do.
Miền này chỉ là giới hạn triển khai thận trọng, không chứng minh mô hình chính xác tại mọi điểm.
Ngưỡng nhắc do người dùng chọn, **mặc định tắt**. Giá trị 35°C trong ô cấu hình là giá trị khởi tạo,
không phải khuyến cáo sức khỏe. Bật nhắc: vượt liên tục 2 phút; thoát khi thấp hơn ngưỡng 1°C.
Còi cần bật riêng: tiếng ngắn 150 ms, ít nhất 5 phút giữa các lần.
Mất mẫu hoặc lỗi cảm biến hủy tính HI và trạng thái nhắc, không kết luận an toàn.

OLED dùng tiếng Việt không dấu để phù hợp font nhỏ. Mô tả đầy đủ có dấu trong trang cấu hình/tài liệu.
Hiện chưa triển khai thang tiện nghi/khuyến cáo nhiều mức Việt Nam vì chưa có căn cứ đủ mạnh.

## Cấu trúc và bảo mật

- `HP20.ino`: chu kỳ hoạt động, hiển thị, điều khiển, điều phối gửi.
- `settings.h`: chân kết nối và cấu hình phần cứng.
- `model.h`: thuật toán HI, chống dội BOOT và các phép tính thời gian nhắc/gửi.
- `config.*`: lưu cấu hình NVS bằng một bản ghi JSON.
- `portal.*`: Wi-Fi cấu hình, trang web tiếng Việt, kiểm tra dữ liệu và mã chống CSRF.
- `cloud.*`: HTTPS trong tác vụ riêng, truyền dữ liệu qua hàng đợi.
- `tests/`, `.github/workflows/`: kiểm tra mô hình và biên dịch trên GitHub Actions.

Thông tin thật nhập trên điện thoại, lưu vào NVS. `.env.example` chỉ minh họa tên cấu hình,
firmware không đọc file `.env`. `.gitignore` bỏ qua `.env`, `secrets.h`, khóa và file build.
NVS của bản phát triển **chưa mã hóa**; chống trích xuất vật lý cần flash/NVS encryption và secure boot,
ngoài phạm vi bản đầu. Không chia sẻ bản dump flash. Portal chỉ phục vụ giao diện AP, không qua LAN nhà.

## Git/GitHub

Dự án được chuẩn bị để dùng Git tại chính HP20. Kiểm tra `git status` trước mỗi commit.
Kho đích: https://github.com/huynhtandung-maker/HP20 (Public do chủ dự án tạo).
Trạng thái đồng bộ được ghi trong `GITHUB_SETUP.md`; không đưa bí mật lên kho.
Workflow chỉ chạy khi repository được push và GitHub Actions được bật.

## Kiểm thử trước khi dùng hằng ngày

Xem `TESTING.md`: phải kiểm tra trên bo mạch thật, gồm mất điện, router chậm,
sai token, mất cảm biến và OLED đúng driver. Không coi biên dịch thành công là đã kiểm thử phần cứng.
