# Makefile chính
help:
	@echo "Các lệnh có sẵn:"
	@echo "  make all      - Biên dịch tất cả modules"
	@echo "  make test     - Chạy kiểm thử đầy đủ"
	@echo "  make clean    - Dọn dẹp file biên dịch"

all:
	$(MAKE) -C src all

test:
	sudo ./test_driver.sh

clean:
	$(MAKE) -C src clean
