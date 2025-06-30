#!/bin/bash

# Tên script: test_driver.sh
# Mô tả: Kiểm thử Virtual Network Driver

# Dừng nếu có lỗi
set -e

# Kiểm tra quyền root
if [ "$EUID" -ne 0 ]
then
    echo "Vui lòng chạy script bằng quyền root: sudo ./test_driver.sh"
    exit 1
fi

# Hàm in tiêu đề
print_header() {
    echo -e "\n\e[1;34m$1\e[0m"
    echo "========================================"
}

# 0. Gỡ bỏ module cũ nếu tồn tại
print_header "0. Gỡ bỏ module cũ"
sudo rmmod vnet_netfilter 2>/dev/null || true
sudo rmmod vnet_driver 2>/dev/null || true
echo "Đã gỡ bỏ module cũ (nếu có)"

# 1. Biên dịch và tải module
print_header "1. Biên dịch và tải module"
cd src
make clean
make
cd ..
sudo insmod src/vnet_driver.ko
sudo insmod src/vnet_netfilter.ko

# 2. Kiểm tra module đã tải
print_header "2. Module đã tải"
lsmod | grep -E 'vnet_driver|vnet_netfilter'

# 3. Kiểm tra interface mạng
print_header "3. Interface mạng"
ip link show vnet0
ip link show vnet1

# 4. Cấu hình địa chỉ IP
print_header "4. Cấu hình địa chỉ IP"
sudo ip addr add 192.168.10.1/24 dev vnet0
sudo ip addr add 192.168.10.2/24 dev vnet1
sudo ip link set vnet0 up
sudo ip link set vnet1 up

# 5. Hiển thị cấu hình IP
print_header "5. Cấu hình IP hiện tại"
ip addr show vnet0
ip addr show vnet1

# 6. Kiểm tra kết nối bằng netcat thay vì ping
print_header "6. Kiểm tra kết nối TCP"

# Chạy netcat ở chế độ nghe trên vnet1
nc -l -k -s 192.168.10.2 -p 12345 > /dev/null &
SERVER_PID=$!

# Đợi server khởi động
sleep 1

# Gửi dữ liệu từ vnet0
echo "Test connection" | nc -w 1 -s 192.168.10.1 192.168.10.2 12345

# Kiểm tra kết quả
if [ $? -eq 0 ]; then
    echo -e "\e[1;32mKết nối TCP thành công!\e[0m"
else
    echo -e "\e[1;31mKết nối TCP thất bại!\e[0m"
fi

# Dừng server
kill $SERVER_PID


# 7. Kiểm tra thống kê qua netfilter
print_header "7. Thống kê netfilter"
echo "Xem thống kê tại: /proc/vnet_capture"
cat /proc/vnet_capture | head -20

# 8. Kiểm tra log kernel
print_header "8. Kernel log"
dmesg | grep -E 'vnet|netfilter' | tail -20

# 9. Dọn dẹp
print_header "9. Dọn dẹp"
sudo rmmod vnet_netfilter
sudo rmmod vnet_driver
make -C src clean

echo -e "\n\e[1;32mKiểm thử hoàn tất!\e[0m"