# Virtual Network Driver với Packet Capture

## Mô tả tổng quan

Dự án này triển khai một **Virtual Network Driver** cho Linux kernel, tạo ra hai interface mạng ảo (`vnet0` và `vnet1`) có khả năng giao tiếp với nhau thông qua một kênh truyền dữ liệu nội bộ. Driver được tích hợp với tính năng bắt và phân tích gói tin thông qua Netfilter hooks.

## ✨ Tính năng chính

- **🌐 Virtual Network Interfaces**: Tạo hai interface mạng ảo `vnet0` và `vnet1`
- **🔄 Packet Forwarding**: Chuyển tiếp gói tin tự động giữa hai interface
- **📊 Packet Capture**: Bắt và lưu trữ thông tin chi tiết về các gói tin
- **🔍 Netfilter Integration**: Tích hợp với Netfilter hooks để monitor traffic
- **📈 Statistics**: Cung cấp thống kê chi tiết qua `/proc/vnet_capture`
- **📝 Kernel Logging**: Ghi log các hoạt động vào kernel log

## 🗂️ Cấu trúc dự án

```
driver/
├── Makefile                    # Makefile chính cho toàn bộ dự án
├── test_driver.sh             # Script kiểm thử tự động
├── include/
│   └── vnet_driver.h          # Header file chứa các định nghĩa và cấu trúc
└── src/
    ├── Makefile               # Makefile cho việc biên dịch modules
    ├── vnet_driver.c          # Module driver chính - quản lý virtual interfaces
    └── vnet_netfilter.c       # Module netfilter hooks - bắt và phân tích packets
```

## 🛠️ Yêu cầu hệ thống

- **Hệ điều hành**: Linux với kernel version 4.15+
- **Quyền hạn**: Root privileges để load/unload kernel modules
- **Tools cần thiết**:
  - GCC compiler
  - Linux kernel headers
  - Make utility
  - netcat (nc) cho việc kiểm thử

### Cài đặt dependencies trên Ubuntu 20.04:
```bash
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r) netcat-openbsd
```

## 🚀 Cài đặt và sử dụng

### 1. Biên dịch

```bash
# Xem các lệnh có sẵn
make help

# Biên dịch tất cả modules
make all

# Hoặc biên dịch từ thư mục src
cd src
make
cd ..
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

#### Cấu hình network interfaces:
```bash
# Gán địa chỉ IP
sudo ip addr add 192.168.10.1/24 dev vnet0
sudo ip addr add 192.168.10.2/24 dev vnet1

# Kích hoạt interfaces
sudo ip link set vnet0 up
sudo ip link set vnet1 up

# Kiểm tra cấu hình
ip addr show vnet0
ip addr show vnet1
```

#### Kiểm tra kết nối:

**Terminal 1 - Server:**
```bash
nc -l -k -s 192.168.10.2 -p 12345
```

**Terminal 2 - Client:**
```bash
echo "Hello Virtual Network!" | nc -s 192.168.10.1 192.168.10.2 12345
```

#### Xem thống kê packet capture:
```bash
cat /proc/vnet_capture
```

#### Xem kernel logs:
```bash
dmesg | grep vnet
```

#### Unload modules:
```bash
sudo rmmod vnet_netfilter
sudo rmmod vnet_driver
```

## 🔧 Chi tiết kỹ thuật

### 1. Virtual Network Driver (src/vnet_driver.c)

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

### 2. Netfilter Module (src/vnet_netfilter.c)

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

### 3. Header File (include/vnet_driver.h)

Chứa định nghĩa:
- Constants và macros
- Cấu trúc dữ liệu
- Function prototypes
- Global variables

## 📋 Script kiểm thử (test_driver.sh)

Script tự động hóa quá trình testing:

1. Gỡ bỏ modules cũ nếu tồn tại
2. Biên dịch modules mới
3. Load modules vào kernel
4. Cấu hình network interfaces
5. Kiểm tra kết nối TCP
6. Hiển thị thống kê packet capture
7. Cleanup resources

## 🐛 Troubleshooting

### Các lỗi thường gặp:

#### 1. "Operation not permitted"
```bash
# Nguyên nhân: Thiếu quyền root
# Giải pháp:
sudo your_command
```

#### 2. "No such file or directory" khi load module
```bash
# Nguyên nhân: Chưa biên dịch hoặc path sai
# Giải pháp:
make clean && make all
sudo insmod src/vnet_driver.ko
```

#### 3. "Device or resource busy"
```bash
# Nguyên nhân: Module đã được load
# Giải pháp:
sudo rmmod vnet_netfilter vnet_driver
sudo insmod src/vnet_driver.ko src/vnet_netfilter.ko
```

### Debug Commands:
```bash
# Kiểm tra modules đã load
lsmod | grep vnet

# Xem kernel logs
dmesg | tail -20

# Kiểm tra network interfaces
ip link show | grep vnet
```

## 👨‍💻 Tác giả

- **Ndtgithub3333** - [https://github.com/Ndtgithub3333](https://github.com/Ndtgithub3333)

## 📞 Liên hệ

Nếu bạn có bất kỳ câu hỏi nào, vui lòng tạo issue trên GitHub repository.

**Repository**: [https://github.com/Ndtgithub3333/driver](https://github.com/Ndtgithub3333/driver)