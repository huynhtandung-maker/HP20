# Cơ sở chỉ số · model-v0.1

## Lựa chọn hiện tại

Rothfusz Heat Index với nhánh hiệu chỉnh độ ẩm cao của NWS. Đầu vào °C được đổi sang °F trước
khi áp công thức, kết quả đổi về °C. Chỉ nhận đầu vào hữu hạn, miền sản phẩm 26,7–50°C/RH 40–100%.
Ngoài miền không giả làm nhiệt độ cảm nhận bằng cách chép nhiệt độ không khí sang.

Nguồn công thức: https://www.weather.gov/ctp/heat

Công thức phản ánh một mô hình ước tính nhiệt khi nóng ẩm, không đo thân nhiệt, không đo chính xác
cảm nhận mỗi cá nhân, không đủ để chẩn đoán say nóng. DHT22 không đo gió, nhiệt bức xạ,
quần áo, vận động hay tình trạng sức khỏe. Không suy ra WBGT hay PMV từ hai đầu vào này.

## Vì sao chưa có bảng ngưỡng Việt Nam trong firmware

- Nguyen và cộng sự (2003), 40 người trẻ, mùa đông Hà Nội, RH 40%:
  khoảng 24–29°C nhiệt độ phòng được trên 90% đánh giá hơi mát đến hơi ấm.
  Không phải ngưỡng HI hoặc ngưỡng sức khỏe. https://pubmed.ncbi.nlm.nih.gov/16022160/
- Ha, Hoa và Binh (2021) khảo sát văn phòng Việt Nam, sử dụng ET/PMV và cả tốc độ gió.
  Không thể lấy ngưỡng ET đưa vào HI chỉ vì cùng đơn vị °C.
  https://srees.sggw.edu.pl/article/download/123/89/158
- Bộ Y tế có hướng dẫn phòng tác hại nắng nóng nhưng nguồn đã xem không cung cấp
  bảng ngưỡng HI dành cho thiết bị đo nhiệt độ/độ ẩm trong phòng:
  https://moh.gov.vn/en/tin-tong-hop/-/asset_publisher/k206Q9qkZOqn/content/bo-y-te-huong-dan-cham-soc-suc-khoe-mua-nang-nong-cho-cong-ong-va-nguoi-lao-ong

Không tăng ngưỡng nguy cơ vì mặc định người Việt quen nóng. Tiện nghi và nguy cơ sức khỏe khác nhau.
Do đó v0.1 chỉ cho phép **nhắc làm mát theo ngưỡng người dùng tự chọn**, tắt mặc định.
Không gắn màu xanh/nhãn “an toàn” cho phòng khi không đủ dữ liệu.

## Hướng dẫn hành động

Thông điệp khi vượt ngưỡng tự chọn: nghỉ nơi mát, giảm vận động, uống nước phù hợp.
Không hướng dẫn uống một lượng cố định cho tất cả mọi người (nhất là người bị hạn chế dịch).
Nếu người trong phòng có lú lẫn, ngất hoặc biểu hiện nặng khi nóng, cần gọi 115;
không chờ thiết bị vượt một ngưỡng số mới tìm trợ giúp.

## Việc còn lại để xây dựng thang phù hợp địa phương

Cần dữ liệu và đánh giá độc lập theo loại phòng, mùa/địa phương, thông gió và đối tượng sử dụng.
Phải kiểm tra mô hình với nghiên cứu tương ứng trước khi công bố bất kỳ ngưỡng “chuẩn” nào.


## Rà soát ngày 2026-09-13 cho v0.2.0

Không có thay đổi công thức HI trong phiên bản này. Nghiên cứu trên 40 người trẻ tại Hà Nội
không đủ để suy ra một thang HI y tế quốc gia. Không đổi khoảng tiện nghi nhiệt độ phòng
thành ngưỡng HI, không suy ra an toàn sức khỏe từ cảm giác đã quen nóng.

Nghiên cứu nhà ở Đà Nẵng 2025 khảo sát 60 nhà thông gió tự nhiên/kết hợp điều hòa,
ghi nhận sự thích nghi của người ở và hạn chế của mô hình PMV/adaptive.
Đây là bằng chứng cần xét bối cảnh nhà ở, không phải thang nguy cơ HI cho DHT22.
Nguồn: https://www.sciencedirect.com/science/article/pii/S0378778825008217

Các mức sản phẩm hiện có: không có số đo; ngoài miền HI; HI ước tính;
đang vượt ngưỡng nhắc do người dùng tự chọn. Không công bố mức “an toàn y tế”.
Hướng dẫn làm mát là gợi ý chung, không phải chẩn đoán theo một thang Việt Nam đã xác thực.

Để hoàn thiện mục tiêu thang Việt Nam: chọn đối tượng/loại phòng và địa phương;
thu thập vận tốc gió/nhiệt bức xạ nếu dùng mô hình yêu cầu các đại lượng đó;
đối chiếu nghiên cứu đầy đủ và đánh giá chuyên môn trước khi phát hành các mức nguy cơ.


## HP20-VN Office v1

HP20 dùng thang hành động cho công việc bàn giấy trong phòng tại Việt Nam: 24–29°C/RH ≤70% là dải dễ chịu vận hành; RH >70% là ẩm cao; T >29°C hoặc HI ≥32°C là bắt đầu nóng; HI ≥39°C là khó chịu; HI ≥45°C là rất khó chịu. Các mốc HI chỉ dùng để ưu tiên hành động trong môi trường nóng ẩm, không dùng nhãn nguy cơ y khoa của quốc gia khác.

Cơ sở: nghiên cứu về người Việt Nam ghi nhận hơn 90% người tham gia thấy 24–29°C nằm trong vùng hơi mát–trung tính–hơi ấm; nghiên cứu văn phòng Việt Nam nhấn mạnh nhiệt độ, độ ẩm và vận tốc gió cùng quyết định tiện nghi. HP20 không đo gió, bức xạ, trang phục, mức hoạt động hay chất ô nhiễm, vì vậy cần hiệu chỉnh theo phản hồi thực tế và không thay thế tư vấn y tế.
