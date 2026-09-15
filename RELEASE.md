# HP20 Release Gate · v0.9.6

## Không tag/release chỉ vì code đã nằm trong ZIP

Một bản phát hành chính thức chỉ được tạo khi đủ các cổng sau:

1. Arduino IDE **Verify PASS** với ESP32 Dev Module.
2. Upload lên board thật PASS.
3. DHT22/OLED/LED/buzzer/BOOT hoạt động.
4. BOOT ~3 s mở AP `HP20-xxxxxx` không password.
5. Phone vào portal, chọn Wi-Fi, save, thấy progress + LED/buzzer feedback.
6. Reboot/mất điện tự reconnect, không hỏi lại credential.
7. ThingsBoard nhận telemetry và cadence 5 phút sau giai đoạn xác nhận ban đầu.
8. GitHub Actions SH1106 + SSD1306 PASS.
9. Không có `secrets.h`, token/password thật trong Git diff.
10. Sau đó mới tạo tag `v0.9.6` và GitHub Release.

## Git flow sau khi test board PASS

```powershell
git checkout main
git pull origin main
git status
```

Đưa source v0.9.6 vào branch mới, commit, push, tạo Pull Request, chờ CI PASS rồi merge.

Sau khi merge:

```powershell
git checkout main
git pull origin main
git tag -a v0.9.6 -m "HP20 v0.9.6 onboarding UX and OTA readiness"
git push origin v0.9.6
```

Tag phải khớp `hp20_version.h`.

## OTA release tiếp theo

Để test OTA thật, target phải **mới hơn 0.9.6**, ví dụ v0.9.7. Không gán binary v0.9.6 cho thiết bị đang chạy v0.9.6 rồi kỳ vọng OTA chạy.
