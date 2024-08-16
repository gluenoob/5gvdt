#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define GNB_IP "127.0.0.1"
#define GNB_PORT 6000
#define BUFFER_SIZE 1024

// Định nghĩa cấu trúc NGAP Paging
typedef struct {
    int message_id;
    int ue_id;
    int tai;
} ngap_paging_t;

void send_paging_message(int sock) {
    ngap_paging_t paging;
    paging.message_id = 100;
    paging.ue_id = 123;
    paging.tai = 45204;

    // Gửi struct NGAP Paging tới gNB
    if (send(sock, &paging, sizeof(paging), 0) < 0) {
        perror("send failed");
    } else {
        printf("Sent NGAP Paging to gNB: message_id = %d, ue_id = %d, tai = %d\n", 
               paging.message_id, paging.ue_id, paging.tai);
    }
}

int main() {
    int sock;
    struct sockaddr_in gnb_addr;
    char command[BUFFER_SIZE];

    // Tạo socket TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ gNB
    memset(&gnb_addr, 0, sizeof(gnb_addr));
    gnb_addr.sin_family = AF_INET;
    gnb_addr.sin_port = htons(GNB_PORT);
    if (inet_pton(AF_INET, GNB_IP, &gnb_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Kết nối tới gNB
    if (connect(sock, (struct sockaddr*)&gnb_addr, sizeof(gnb_addr)) < 0) {
        perror("connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to gNB at %s:%d\n", GNB_IP, GNB_PORT);

    // Nhận lệnh từ người dùng
    while (1) {
        printf("Enter command: ");
        if (fgets(command, sizeof(command), stdin) != NULL) {  // Kiểm tra kết quả của fgets
            command[strcspn(command, "\n")] = 0;  // Loại bỏ ký tự newline

            if (strcmp(command, "send") == 0) {
                send_paging_message(sock);
            } else {
                printf("Unknown command\n");
            }
        } else {
            perror("Error reading command");
        }
    }

    close(sock);
    return 0;
}

