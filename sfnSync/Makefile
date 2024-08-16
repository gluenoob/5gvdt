# Tên của các tệp mã nguồn
GNB_SRC = gnb.c
UE_SRC = ue.c

# Tên của các tệp thực thi sau khi biên dịch
GNB_BIN = gnb
UE_BIN = ue

# Trình biên dịch và các tùy chọn biên dịch
CC = gcc
CFLAGS = -Wall -O2
CLIBS = -lpthread

# Mục tiêu mặc định sẽ biên dịch cả gNB, UE 
all: $(GNB_BIN) $(UE_BIN)

# Luật để biên dịch gNB
$(GNB_BIN): $(GNB_SRC)
	$(CC) $(CFLAGS) -o $(GNB_BIN) $(GNB_SRC) $(CLIBS)

# Luật để biên dịch UE
$(UE_BIN): $(UE_SRC)
	$(CC) $(CFLAGS) -o $(UE_BIN) $(UE_SRC) $(CLIBS)

# Luật để làm sạch các tệp thực thi
clean:
	rm -f $(GNB_BIN) $(UE_BIN)

# Luật để chạy gNB
run-gnb: $(GNB_BIN)
	./$(GNB_BIN)

# Luật để chạy UE
run-ue: $(UE_BIN)
	./$(UE_BIN)

# Giúp đỡ
help:
	@echo "Makefile để biên dịch và chạy gNB, UE"
	@echo "Các mục tiêu khả dụng:"
	@echo "  all          - Biên dịch cả gNB, UE"
	@echo "  clean        - Xóa các tệp thực thi"
	@echo "  run-gnb      - Chạy gNB"
	@echo "  run-ue       - Chạy UE"
	@echo "  help         - Hiển thị thông tin này"

