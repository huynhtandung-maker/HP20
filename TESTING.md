# HP20 Testing · v0.9.6

## A. Regression phần lõi

```bash
g++ -std=c++11 -Wall -Wextra tests/model_test.cpp \
  hp20_thermal.cpp hp20_trend.cpp hp20_indicator.cpp -o hp20-tests
./hp20-tests
```

Kỳ vọng: `HP20 v0.9.6 ... tests passed`.

## B. Arduino build

- ESP32 Dev Module
- core 3.3.11
- DHT 1.4.7
- Adafruit Unified Sensor 1.1.15
- U8g2 2.36.19
- ArduinoJson 7.4.3

Verify cả SH1106 và SSD1306 qua GitHub Actions.

## C. Onboarding UX

1. Giữ BOOT ~3 s.
2. Phone thấy `HP20-xxxxxx`; kết nối **không password**.
3. Captive portal tự mở; nếu không, vào 192.168.4.1.
4. Wi-Fi list sắp mạnh → yếu, có nhãn dễ hiểu.
5. Chọn mạng, nhập password, bấm Save.
6. Web không đứng im: phải hiện progress.
7. Connecting: 1 beep + LED xanh dương nháy nhanh.
8. Success: 2 beep + LED sáng ngắn; ThingsBoard nhận dữ liệu.
9. Sai password: khoảng 20 s phải có failure feedback + portal còn để sửa.
10. Mất điện/reboot: không nhập lại.

## D. Security regression

- Wi-Fi mất kết nối bình thường không tự mở AP.
- `secrets.h` không nằm trong Git/ZIP.
- Token cũ không render đầy đủ lên browser.
- Host custom không có CA thì cloud/OTA bị chặn an toàn.

## E. OTA

Bật OTA trong Advanced, lưu, reboot, xác nhận config vẫn bật. Test binary target phải có version > 0.9.6 và đúng panel OLED.
