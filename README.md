# Virtual Network Driver với Packet Capture

## Tổng quan

Dự án này triển khai một Virtual Network Driver cho Linux kernel, bao gồm hai interface ảo (`vnet0` và `vnet1`) có thể giao tiếp với nhau thông qua một kênh truyền dữ liệu nội bộ. Driver còn tích hợp tính năng bắt và phân tích gói tin thông qua Netfilter hooks.

## Tính năng chính

- **Virtual Network Interfaces**: Tạo hai interface mạng ảo `vnet0` và `vnet1`
- **Packet Forwarding**: Chuyển tiếp gói tin giữa hai interface một cách tự động
- **Packet Capture**: Bắt và lưu trữ thông tin chi tiết về các gói tin
- **Netfilter Integration**: Sử dụng Netfilter hooks để monitor traffic
- **Statistics**: Cung cấp thống kê chi tiết qua `/proc/vnet_capture`
- **Kernel Log**: Ghi log chi tiết các hoạt động vào kernel log

## Cấu trúc dự án

```
.
├── Makefile                 # Makefile chính cho toàn bộ dự án
├── test_driver.sh          # Script kiểm thử tự động
└── src/
    ├── Makefile            # Makefile cho việc biên dịch modules
    ├── vnet_driver.c       # Module driver chính
    ├── vnet_netfilter.c    # Module netfilter hooks
    └── include/
        └── vnet_driver.h   # Header file chứa định nghĩa
```

## Yêu cầu hệ thống

- **Hệ điều hành**: Linux với kernel version 4.15+
- **Quyền hạn**: Root privileges để load/unload kernel modules
- **Tools cần thiết**:
  - GCC compiler
  - Linux kernel headers
  - Make utility
  - netcat (nc) cho việc kiểm thử

## Cài đặt và sử dụng

### 1. Biên dịch

```bash
# Biên dịch tất cả modules
make all

# Hoặc biên dịch từ thư mục src
cd src
make
```

### 2. Kiểm thử tự động

```bash
# Chạy script kiểm thử đầy đủ (cần quyền root)
sudo ./test_driver.sh
```

### 3. Sử dụng thủ công

#### Load modules:
```bash
sudo insmod src/vnet_driver.ko
sudo insmod src/vnet_netfilter.ko
```

#### Cấu hình interfaces:
```bash
sudo ip addr add 192.168.10.1/24 dev vnet0
sudo ip addr add 192.168.10.2/24 dev vnet1
sudo ip link set vnet0 up
sudo ip link set vnet1 up
```

#### Kiểm tra kết nối:
```bash
# Terminal 1 - Server
nc -l -k -s 192.168.10.2 -p 12345

# Terminal 2 - Client
echo "Hello World" | nc -s 192.168.10.1 192.168.10.2 12345
```

#### Unload modules:
```bash
sudo rmmod vnet_netfilter
sudo rmmod vnet_driver
```

## Chi tiết kỹ thuật

### 1. Virtual Network Driver (vnet_driver.c)

#### Cấu trúc dữ liệu chính:
- `struct vnet_priv`: Lưu thông tin private của mỗi interface
- `struct captured_packet`: Lưu thông tin gói tin đã bắt được

#### Các hàm chính:
- `vnet_open()`: Mở interface và khởi tạo transmit queue
- `vnet_close()`: Đóng interface và dừng transmit queue
- `vnet_start_xmit()`: Xử lý việc truyền gói tin giữa hai interface
- `vnet_get_stats()`: Cung cấp thống kê network device

#### Cơ chế hoạt động:
1. Tạo hai network device ảo với quan hệ peer-to-peer
2. Khi gói tin được gửi từ một interface, nó sẽ được copy và chuyển đến interface đối tác
3. Gói tin được xử lý và forward lên network stack của interface nhận
4. Thống kê được cập nhật cho cả sender và receiver

### 2. Netfilter Module (vnet_netfilter.c)

#### Chức năng:
- Hook vào INPUT và OUTPUT chains của netfilter
- Bắt và phân tích các gói tin đi qua virtual interfaces
- Lưu trữ thông tin chi tiết về mỗi gói tin

#### Thông tin được capture:
- Timestamp
- Source và Destination IP
- Source và Destination Port (TCP/UDP)
- Protocol type
- Packet length
- Interface name
- Direction (IN/OUT)

#### Proc Interface:
- File: `/proc/vnet_capture`
- Hiển thị thống kê và danh sách gói tin đã bắt được
- Format: Table với các cột thông tin chi tiết

### 3. Header File (vnet_driver.h)

Chứa định nghĩa:
- Constants và macros
- Cấu trúc dữ liệu
- Function prototypes
- Global variables

## Script kiểm thử (test_driver.sh)

Script tự động thực hiện đầy đủ quy trình kiểm thử:

1. **Cleanup**: Gỡ bỏ modules cũ nếu có
2. **Build**: Biên dịch các modules
3. **Load**: Load modules vào kernel
4. **Verify**: Kiểm tra modules đã load thành công
5. **Configure**: Cấu hình IP addresses và enable interfaces
6. **Test**: Kiểm tra kết nối TCP bằng netcat
7. **Monitor**: Hiển thị thống kê packet capture
8. **Log**: Hiển thị kernel logs
9. **Cleanup**: Dọn dẹp và unload modules

## Thông tin Debug

### Kernel Logs
```bash
# Xem logs realtime
dmesg -w | grep -E 'vnet|netfilter'

# Xem logs gần đây
dmesg | grep -E 'vnet|netfilter' | tail -20
```

### Packet Statistics
```bash
# Xem thống kê capture
cat /proc/vnet_capture

# Xem thông tin interfaces
ip addr show vnet0
ip addr show vnet1
```

### Network Statistics
```bash
# Xem stats của interfaces
cat /proc/net/dev | grep vnet
```

## Xử lý lỗi thường gặp

### 1. Module load failed
```bash
# Kiểm tra kernel version compatibility
uname -r
modinfo src/vnet_driver.ko

# Kiểm tra dependencies
lsmod | grep vnet
```

### 2. Interface không xuất hiện
```bash
# Kiểm tra module đã load
lsmod | grep vnet_driver

# Kiểm tra kernel logs
dmesg | grep vnet | tail -10
```

### 3. Không có kết nối
```bash
# Kiểm tra interface đã up
ip link show | grep vnet

# Kiểm tra IP configuration
ip addr show vnet0
ip addr show vnet1

# Kiểm tra routing
ip route show
```

## Tính năng nâng cao

### 1. Packet Filtering
Module có thể được mở rộng để lọc packets theo criteria cụ thể.

### 2. Performance Monitoring
Thống kê chi tiết về throughput, latency, và error rates.

### 3. Security Features
Có thể tích hợp thêm các tính năng security như packet inspection và filtering.

## Phát triển và đóng góp

### Build với Debug
```bash
# Enable debug flags
EXTRA_CFLAGS += -DDEBUG
make
```

### Coding Standards
- Sử dụng Linux kernel coding style
- Comment đầy đủ cho các hàm quan trọng
- Error handling phải được thực hiện đúng cách

## Giấy phép

Module này được phát hành dưới giấy phép GPL (GNU General Public License) phù hợp với Linux kernel requirements.

## Liên hệ và hỗ trợ

Để báo cáo lỗi hoặc đóng góp, vui lòng tạo issue hoặc pull request trên repository của dự án.

---

*Phiên bản: 1.0*  
*Tác giả: Student*  
*Ngày cập nhật: 2025*