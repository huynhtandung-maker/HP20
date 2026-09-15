# HP20 Release Process

HP20 dùng quy trình **manual-control-first**. AI/automation hỗ trợ kiểm tra nhưng không tự merge/tag release khi chưa được người quản trị dự án duyệt.

## 1. Chọn version

Chỉ sửa `hp20_version.h`.

- PATCH: bug fix, UX refinement, docs/repo cleanup.
- MINOR: capability mới nhưng tương thích.
- MAJOR: thay đổi không tương thích về cấu hình/kiến trúc sản phẩm.

## 2. Local validation

1. Arduino Verify.
2. Upload ESP32.
3. Kiểm tra OLED, DHT22, green LED, blue LED, buzzer, BOOT portal.
4. Power cycle và xác nhận config giữ nguyên.
5. Kiểm tra ThingsBoard rate limit/cooldown nếu cloud được cấu hình.

## 3. Git review

```powershell
git status
git diff --stat
git diff
```

Không commit secret hoặc artifact build.

## 4. Commit

Ví dụ:

```powershell
git add .
git commit -m "release: HP20 v0.9.4 clean modular baseline"
git push origin <branch>
```

Khuyến nghị dùng branch + Pull Request thay vì push thẳng `main`.

## 5. CI

GitHub Actions phải pass:

- host regression tests
- ESP32 SH1106 compile
- ESP32 SSD1306 compile

## 6. Merge → tag → release

Chỉ sau khi phần cứng thật đã pass:

```powershell
git checkout main
git pull
git tag -a v0.9.4 -m "HP20 v0.9.4"
git push origin v0.9.4
```

Git tag phải khớp `hp20::version::STRING`.
