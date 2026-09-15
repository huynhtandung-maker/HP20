# HP20 Testing · v0.9.5

## Host regression

```bash
g++ -std=c++11 -Wall -Wextra \
  tests/model_test.cpp \
  hp20_thermal.cpp hp20_trend.cpp hp20_indicator.cpp \
  -o /tmp/hp20-tests
/tmp/hp20-tests
```

Test bảo vệ:

- Heat Index invariants
- thermal band boundaries
- target range
- UI FEEL smoothing state
- green LED semantic ordering
- trend direction
- reminder timing
- retry/cooldown
- button debounce/hold
- millis wrap-around
- firmware version identity

## Arduino hardware validation

Sau mỗi thay đổi firmware:

1. Verify compile.
2. Upload.
3. Serial boot phải hiện firmware version + thermal model version.
4. OLED footer phải hiện firmware version.
5. Kiểm tra đủ 7 trang UI bằng BOOT click.
6. Comfort band → LED xanh sáng liên tục.
7. Band nóng tăng dần → green presence giảm dần.
8. Giữ BOOT ~3 giây → portal mở.
9. Mất điện/cấp lại → Wi‑Fi/config được khôi phục.
10. NVS trống + `secrets.h` local → seed NVS một lần, không mở portal.
11. Đổi Wi‑Fi bằng portal → reboot vẫn dùng Wi‑Fi mới, local secrets không ghi đè.
12. Portal: chỉ một luồng 3 bước; show/hide chỉ hiện dữ liệu mới đang nhập.
13. Nếu cloud bật, không gửi dồn sau reboot.
14. OTA OFF mặc định → không tự tải firmware.
15. OTA ON → lệnh `OTA` kiểm tra shared attrs; cùng version không update.
16. Package sai title/checksum/version → từ chối.
17. Package đúng `HP20` + version mới + SHA-256 đúng → update/reboot và báo version mới.
