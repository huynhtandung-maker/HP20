# HP20 Thermal Model Notes · v0.9.4

## Chuỗi tính toán

```text
DHT22 raw
→ sensor calibration
→ calibrated temperature + RH
→ model::heatIndex()
→ FEEL raw
→ smoothing dành cho UI
→ HP20 local FEEL band
→ meaning / action / LED / chart
```

## Phân biệt các lớp

- **Calibration** sửa sai lệch cảm biến.
- **Heat Index formula** tạo FEEL từ T/RH.
- **Local FEEL bands** diễn giải FEEL cho UX và vận hành.
- **Action modifiers** dùng T/RH để gợi ý tăng gió/giảm ẩm/làm mát.

Không nên dùng một lớp để bù lỗi của lớp khác.

## Local adaptation

HP20 cho phép tự cân chỉnh band trong `hp20_thermal.cpp` dựa trên dữ liệu cảm nhận thực tế của người dùng ở khí hậu nóng ẩm. Các band hiện tại là baseline vận hành của dự án, không phải tiêu chuẩn y khoa Việt Nam.

Xem `CALIBRATION.md` để thu dữ liệu và thay threshold có kiểm soát.
