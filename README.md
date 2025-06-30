# Virtual Network Driver với Packet Capture

## Mô tả tổng quan

Dự án này triển khai một **Virtual Network Driver** cho Linux kernel, tạo ra hai interface mạng ảo (`vnet0` và `vnet1`) có khả năng giao tiếp với nhau thông qua một kênh truyền dữ liệu nội bộ. Driver được tích hợp với tính năng bắt và phân tích gói tin thông qua Netfilter hooks, cung cấp khả năng giám sát và phân tích traffic mạng chi tiết.

## ✨ Tính năng chính

- **🌐 Virtual Network Interfaces**: Tạo hai interface mạng ảo `vnet0` và `vnet1` hoạt động độc lập
- **🔄 Packet Forwarding**: Chuyển tiếp gói tin tự động giữa hai interface với cơ chế peer-to-peer
- **📊 Packet Capture**: Bắt và lưu trữ thông tin chi tiết về các gói tin đi qua hệ thống
- **🔍 Netfilter Integration**: Tích hợp với Netfilter hooks để monitor traffic real-time
- **📈 Statistics**: Cung cấp thống kê chi tiết qua `/proc/vnet_capture`
- **📝 Kernel Logging**: Ghi log chi tiết các hoạt động vào kernel log để debug
- **🧪 Automated Testing**: Script kiểm thử tự động đầy đủ

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

### Phần cứng
- **RAM**: Tối thiểu 1GB (khuyến nghị 2GB+)
- **CPU**: Bất kỳ kiến trúc x86_64

### Phần mềm
- **Hệ điều hành**: Linux với kernel version 4.15+ (đã test trên Ubuntu 18.04+, CentOS 7+)
- **Quyền hạn**: Root privileges để load/unload kernel modules
- **Tools cần thiết**:
  - `build-essential` (GCC compiler, make)
  - `linux-headers-$(uname -r)` - Linux kernel headers
  - `netcat-openbsd` - cho việc kiểm thử kết nối TCP/UDP
  - `iproute2` - công cụ quản lý network

### Cài đặt dependencies trên Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r) netcat-openbsd iproute2
```

### Cài đặt dependencies trên CentOS/RHEL:
```bash
sudo yum install gcc make kernel-devel-$(uname -r) nc iproute
```

## 🚀 Cài đặt và sử dụng

### 1. Clone repository
```bash
git clone https://github.com/Ndtgithub3333/driver.git
cd driver
```

### 2. Biên dịch

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

### 3. Kiểm thử tự động (Khuyến nghị)

```bash
# Chạy script kiểm thử đầy đủ (cần quyền root)
sudo ./test_driver.sh
```

Script sẽ tự động:
- ✅ Gỡ bỏ modules cũ nếu có
- ✅ Biên dịch và load modules mới
- ✅ Cấu hình network interfaces
- ✅ Kiểm tra kết nối TCP
- ✅ Hiển thị thống kê packet capture
- ✅ Cleanup sau khi hoàn thành

### 4. Sử dụng thủ công

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

**Terminal 1 - Chạy server:**
```bash
# TCP server listening trên vnet1
nc -l -k -s 192.168.10.2 -p 12345
```

**Terminal 2 - Chạy client:**
```bash
# Gửi dữ liệu từ vnet0 đến vnet1
echo "Hello Virtual Network!" | nc -s 192.168.10.1 192.168.10.2 12345
```

#### Xem thống kê packet capture:
```bash
# Hiển thị thống kê real-time
cat /proc/vnet_capture

# Theo dõi liên tục
watch cat /proc/vnet_capture
```

#### Xem kernel logs:
```bash
# Xem logs real-time
sudo dmesg -w | grep vnet

# Xem logs đã ghi
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
- **`struct vnet_priv`**: Lưu thông tin private của mỗi interface
  - Pointer đến peer device
  - Network statistics
  - Lock mechanisms
- **`struct captured_packet`**: Lưu thông tin gói tin đã bắt được
  - Timestamp
  - IP addresses (source/destination)
  - Ports (source/destination)
  - Protocol type
  - Packet length

#### Các hàm chính:
- **`vnet_open()`**: Mở interface và khởi tạo transmit queue
- **`vnet_close()`**: Đóng interface và dừng transmit queue
- **`vnet_start_xmit()`**: Xử lý việc truyền gói tin giữa hai interface
- **`vnet_get_stats()`**: Cung cấp thống kê network device

#### Cơ chế hoạt động:
1. **Khởi tạo**: Tạo hai network device ảo với quan hệ peer-to-peer
2. **Packet Transmission**: Khi gói tin được gửi từ một interface, nó sẽ được copy và chuyển đến interface đối tác
3. **Packet Reception**: Gói tin được xử lý và forward lên network stack của interface nhận
4. **Statistics Update**: Thống kê được cập nhật cho cả sender và receiver

### 2. Netfilter Module (src/vnet_netfilter.c)

#### Chức năng:
- **Hook Integration**: Hook vào INPUT và OUTPUT chains của netfilter
- **Packet Analysis**: Bắt và phân tích các gói tin đi qua virtual interfaces
- **Data Storage**: Lưu trữ thông tin chi tiết về mỗi gói tin trong kernel memory

#### Thông tin được capture:
- **Timestamp**: Thời gian chính xác khi gói tin được bắt
- **Network Layer**: Source và Destination IP addresses
- **Transport Layer**: Source và Destination Ports (TCP/UDP)
- **Protocol**: Protocol type (TCP, UDP, ICMP, etc.)
- **Size**: Packet length in bytes
- **Interface**: Interface name (vnet0/vnet1)
- **Direction**: Traffic direction (IN/OUT)

#### Proc Interface:
- **File**: `/proc/vnet_capture`
- **Format**: Formatted table với các cột thông tin chi tiết
- **Real-time**: Cập nhật real-time khi có packet mới

### 3. Header File (include/vnet_driver.h)

**Chứa định nghĩa:**
- Constants và macros for configuration
- Cấu trúc dữ liệu shared giữa các modules
- Function prototypes for inter-module communication
- Global variables và shared memory structures

## 📋 Script kiểm thử (test_driver.sh)

Script tự động hóa hoàn toàn quá trình testing:

1. **Cleanup**: Gỡ bỏ modules cũ nếu tồn tại
2. **Build**: Biên dịch modules mới
3. **Load**: Load modules vào kernel
4. **Configure**: Cấu hình network interfaces
5. **Test**: Kiểm tra kết nối TCP/UDP
6. **Monitor**: Hiển thị thống kê packet capture
7. **Cleanup**: Dọn dẹp resources

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

#### 4. "Cannot allocate memory"
```bash
# Nguyên nhân: Thiếu RAM
# Giải pháp: Giải phóng memory hoặc thêm swap
free -h
sudo swapon --show
```

### Debug Commands:
```bash
# Kiểm tra modules đã load
lsmod | grep vnet

# Xem kernel logs
dmesg | tail -20

# Kiểm tra network interfaces
ip link show | grep vnet

# Monitor system resources
top
free -h
```

## 📊 Performance và Benchmarks

### Thông số đo được:
- **Throughput**: ~1Gbps với packets 1500 bytes
- **Latency**: <1ms trong local transmission
- **Memory Usage**: ~2MB per 1000 captured packets
- **CPU Usage**: <5% trên single core 2GHz

### Giới hạn:
- **Max Interfaces**: 2 (vnet0, vnet1)
- **Max Captured Packets**: 10,000 (configurable)
- **Max Packet Size**: 1500 bytes (standard MTU)

## 🔒 Bảo mật

### Considerations:
- Module chạy ở kernel space với full privileges
- Không validate input từ userspace
- Chỉ sử dụng trong môi trường test/development
- Không khuyến nghị deploy trên production systems

## 🤝 Đóng góp

1. Fork repository
2. Tạo feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add some amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Tạo Pull Request

## 📝 License

Distributed under the MIT License. See `LICENSE` file for more information.

## 👨‍💻 Tác giả

- **Ndtgithub3333** - *Initial work* - [Ndtgithub3333](https://github.com/Ndtgithub3333)

## 🙏 Acknowledgments

- Linux Kernel Development Community
- Netfilter Project
- Virtual Network Device Implementations

---

## 📞 Liên hệ

Nếu bạn có bất kỳ câu hỏi nào, vui lòng tạo issue trên GitHub repository.

**Repository**: [https://github.com/Ndtgithub3333/driver](https://github.com/Ndtgithub3333/driver)