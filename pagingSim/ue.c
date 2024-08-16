#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BROADCAST_PORT 5000
#define BUFFER_SIZE 1024
#define TMSI 123  // TMSI lưu tại UE

unsigned int sfn = 0;  // SFN (System Frame Number)

// Định nghĩa cấu trúc MIB tương tự như server
typedef struct {
    int message_id;
    unsigned int sfn;
} mib_t;

// Định nghĩa cấu trúc RRC Paging
typedef struct {
    int message_id;
    int pagingRecordList[32];
} rrc_paging_t;

// Hàm của thread để tăng SFN mỗi 1 giây
void* sfn_counter(void* arg) {
    while (1) {
        sleep(1);
        sfn = (sfn + 1) % 1024;  // SFN trong 5G có giá trị từ 0 đến 1023
        printf("Local SFN updated: %u\n", sfn);
    }
    return NULL;
}

void handle_rrc_paging(rrc_paging_t *paging, unsigned int current_sfn) {
    for (int i = 0; i < 32; i++) {
        if (paging->pagingRecordList[i] == TMSI) {
            printf("Paging thành công cho TMSI = %d tại SFN = %u\n", TMSI, current_sfn);
            return;
        }
    }
}

// Hàm của thread để nhận bản tin MIB và cập nhật SFN
void* receive_mib(void* arg) {
    int sock;
    struct sockaddr_in recv_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;
    socklen_t addr_len;

    // Tạo socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ nhận
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(BROADCAST_PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Set sockopt to handle errno 98
	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

    // Liên kết socket với địa chỉ nhận
    if (bind(sock, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0) {
        perror("bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&recv_addr, &addr_len);
        if (bytes_received > 0) {
            int message_id = *((int *)buffer);
            
            if (message_id == 101) {  // RRC Paging
                rrc_paging_t *rrc_paging = (rrc_paging_t *)buffer;
                unsigned int current_sfn = sfn;  // Lấy SFN từ biến global
                handle_rrc_paging(rrc_paging, current_sfn);
            } else if (message_id == 11) {  // MIB
                mib_t *mib_message = (mib_t *)buffer;
                sfn = mib_message->sfn;
                printf("Received MIB: message_id = %d, updated SFN = %u\n", mib_message->message_id, sfn);
            } else {
                printf("Received unknown message_id = %d\n", message_id);
            }
        }
        else {
            perror("recvfrom failed");
            close(sock);
            exit(EXIT_FAILURE);
        }
    }
    close(sock);
    return NULL;
}

int main() {
    pthread_t sfn_thread, receive_thread;

    // Tạo thread để tăng SFN
    if (pthread_create(&sfn_thread, NULL, sfn_counter, NULL) != 0) {
        perror("Failed to create SFN thread");
        exit(EXIT_FAILURE);
    }

    // Tạo thread để nhận bản tin MIB và cập nhật SFN
    if (pthread_create(&receive_thread, NULL, receive_mib, NULL) != 0) {
        perror("Failed to create receive thread");
        exit(EXIT_FAILURE);
    }

    // Chờ các thread kết thúc (thực tế sẽ không bao giờ kết thúc trong chương trình này)
    pthread_join(sfn_thread, NULL);
    pthread_join(receive_thread, NULL);

    return 0;
}

