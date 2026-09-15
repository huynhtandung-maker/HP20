# HP20 Calibration & Local Tuning

HP20 có **ba lớp khác nhau**. Không trộn chúng khi cân chỉnh.

## 1. DHT22 calibration — `hp20_sensor.h`

Chỉ chỉnh khi so sánh với thiết bị tham chiếu đặt **cùng vị trí**, đủ thời gian ổn định.

```cpp
constexpr float TEMP_CORRECTION_PERCENT = -7.0f;
constexpr float HUMIDITY_CORRECTION_PERCENT = +6.8f;
```

Quy tắc:

- DHT22 cao hơn tham chiếu → correction âm.
- DHT22 thấp hơn tham chiếu → correction dương.
- Đây là **phần trăm**, không phải cộng/trừ trực tiếp °C hoặc RH points.
- Không dùng calibration để ép FEEL thành mức "dễ chịu".

## 2. Công thức FEEL nền — `model.h`

`model::heatIndex()` biến T + RH thành FEEL. Không sửa công thức trong quá trình tuning ngưỡng thông thường; nếu vừa sửa formula vừa sửa band sẽ mất khả năng truy nguyên nguyên nhân.

## 3. Local FEEL thresholds — `hp20_thermal.cpp`

Tìm block:

```text
LOCAL FEEL THRESHOLD TUNING AREA
```

Đây là nơi duy nhất để tự xây thang cảm nhận phù hợp người dùng Việt Nam/Sài Gòn. Các hằng số phải luôn tăng dần; `static_assert` sẽ chặn cấu hình bị đảo.

### Cách thu dữ liệu thực nghiệm

Ghi ít nhất:

| Thời điểm | Tcal | RHcal | FEEL | Cảm nhận | Tập trung | Quạt/AC |
|---|---:|---:|---:|---|---|---|
| sáng | | | | | | |
| trưa | | | | | | |
| chiều | | | | | | |
| tối | | | | | | |

Thang cảm nhận nên dùng cố định, ví dụ:

1 lạnh · 2 mát · 3 dễ chịu · 4 hơi ấm · 5 hơi oi · 6 oi · 7 oi nóng · 8 nóng · 9 rất nóng.

Không đổi threshold vì một lần cảm nhận. Thu nhiều ngày, nhiều thời điểm và nhiều trạng thái quạt/AC.

### Target zone trên chart

Cũng nằm trong `hp20_thermal.cpp`:

```cpp
TARGET_FEEL_LOW
TARGET_FEEL_HIGH
```

Đây là vùng vận hành mục tiêu của HP20, không phải chuẩn y khoa/pháp lý.

## 4. Action modifiers

Band do FEEL quyết định. T/RH chỉ giúp chọn hành động phù hợp hơn, ví dụ ưu tiên tăng gió hoặc giảm ẩm. Các modifier này cũng được khai báo tập trung trong `hp20_thermal.cpp` và có chú thích.
