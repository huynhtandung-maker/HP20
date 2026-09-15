# GitHub Reconciliation · v0.9.4

GitHub `main` hiện là lịch sử cũ; không rewrite history và không force-push.

## Chiến lược

1. Tạo branch mới từ `main`.
2. Đưa toàn bộ baseline v0.9.4 vào branch.
3. GitHub Actions chạy compile/test.
4. Review Pull Request thủ công.
5. Chỉ merge khi ESP32 local đã Verify/Upload/test thật.
6. Sau merge mới tạo tag `v0.9.4` và GitHub Release.

## File legacy trên GitHub

Các file cũ có thể được thay nội dung bằng baseline mới. Không xóa lịch sử commit cũ chỉ vì code đã tiến hóa.

Nếu phát hiện secret thật từng được commit, rotate secret trước; việc xóa file ở HEAD không loại secret khỏi Git history.
