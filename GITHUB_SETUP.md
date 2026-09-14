# Git / GitHub

Kho đích do chủ dự án tạo: https://github.com/huynhtandung-maker/HP20 (Public).
Remote origin: https://github.com/huynhtandung-maker/HP20.git.
Dự án làm việc Local tại thư mục gốc; không worktree.

Version 0.2.0 ở settings.h được dùng chung cho Serial, trang thiết bị và portal.
CHANGELOG.md giữ lịch sử. Workflow .github/workflows/build.yml đã chuẩn bị cho core 3.3.11,
kiểm thử host và biên dịch SH1106/SSD1306. Chưa chạy CI trước khi push thành công.

## Trạng thái đồng bộ ngày 2026-09-14

Git Credential Manager đã được chủ dự án xác thực trên máy. Nhánh `main` và tag
`v0.2.0` đã được push thành công lên `origin`; làm việc Local và GitHub đang cùng
commit `2fea9f4`.

Sau một thay đổi mới, tại thư mục HP20:

```powershell
git -c safe.directory=E:/IOT/ARDUINO/Tu_Hoc/HP20 push -u origin main
git -c safe.directory=E:/IOT/ARDUINO/Tu_Hoc/HP20 push origin v0.2.0
```

Tham số safe.directory chỉ áp dụng từng lệnh do thư mục .git được tạo bởi tài khoản sandbox.
Cấu hình TLS của riêng kho dùng Schannel/kho chứng chỉ Windows; không tắt xác minh TLS.

Trước push: kiểm tra git status và nội dung commit; không có .env/secrets.h, khóa riêng,
ảnh chứa mật khẩu AP, log Serial hoặc bản sao NVS. .env.example chỉ có tên biến rỗng.
Firmware lấy cấu hình thật từ portal/NVS. Bản nguồn không kèm dump flash hay trình biên dịch.

Version phần mềm không đồng nghĩa nghiệm thu phần cứng hoặc chứng nhận y tế.
Xem VALIDATION.md trước khi dùng hằng ngày.
