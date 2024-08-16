#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BROADCAST_PORT 5000
#define BROADCAST_IP "255.255.255.255"
#define SERVER_PORT 6000
#define MESSAGE_ID 11
#define BUFFER_SIZE 1024

#define DRX_CYCLE 32
#define NPF 16
#define MAX_QUEUE_SIZE 1024
#define MAX_UE_PER_QUEUE 32

unsigned int sfn = 0;  // SFN (System Frame Number (SFN))

// Định nghĩa cấu trúc MIB
typedef struct {
    int message_id;
    unsigned int sfn;
} mib_t;

// Định nghĩa cấu trúc NGAP Paging
typedef struct {
    int message_id;
    int ue_id;
    int tai;
} ngap_paging_t;

// Định nghĩa cấu trúc RRC Paging
typedef struct {
    int message_id;
    int pagingRecordList[MAX_UE_PER_QUEUE];
} rrc_paging_t;

// Định nghĩa hàng đợi lưu trữ các UE ID
int msgQueue[MAX_QUEUE_SIZE][MAX_UE_PER_QUEUE];
int queueSize[MAX_QUEUE_SIZE] = {0}; // Số lượng phần tử hiện tại trong mỗi hàng đợi

int dequeueTMSI(int qid, int *pagingRecordList) {
    int count = queueSize[qid];
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            pagingRecordList[i] = msgQueue[qid][i];
        }
        queueSize[qid] = 0; // Đặt lại số lượng phần tử sau khi dequeue
        return count;
    } else {
        return 0; // Không có UE nào trong hàng đợi
    }
}

int enqueueTMSI(int qid, int ueid_temp) {
    if (queueSize[qid] < MAX_UE_PER_QUEUE) {
        msgQueue[qid][queueSize[qid]] = ueid_temp;
        queueSize[qid]++;
        return 0; // Thành công
    } else {
        return -1; // Thất bại vì hàng đợi đầy
    }
}

void message_handler(ngap_paging_t *paging) {
    int ueid_temp = paging->ue_id;
    int drxCycle = DRX_CYCLE;
    int nPF = NPF;
    int SFN_temp = sfn;

    int remainder = (drxCycle / nPF) * ((ueid_temp % 1024) % nPF);

    int qid = 0, offset = 0;
    while (1) {
        SFN_temp = sfn;
        // Calculate SFN offset to SFN target
        if (remainder > (SFN_temp % drxCycle)) {
            qid = (SFN_temp + (remainder - (SFN_temp % drxCycle)) + offset * drxCycle) % 1024;
        } else if (remainder == (SFN_temp % drxCycle)) {
            qid = (SFN_temp + (remainder - (SFN_temp % drxCycle)) + (offset + 1) * drxCycle) % 1024;
        } else {
            qid = (SFN_temp + (drxCycle + (remainder - (SFN_temp % drxCycle))) + offset * drxCycle) % 1024;
        }

        if (enqueueTMSI(qid, ueid_temp) == 0) {
            printf("[SFN = %u] queue ID  = %d ---- TMSI = %d\n", SFN_temp, qid, ueid_temp);
            break;
        } else {
            offset++;
        }
    }
}

// Hàm của thread để tăng SFN mỗi 1 giây
void* sfn_counter(void* arg) {
    while (1) {
        sleep(1);
        sfn = (sfn + 1) % 1024;  // SFN trong 5G có giá trị từ 0 đến 1023
        printf("SFN updated: %u\n", sfn);
    }
    return NULL;
}

// Hàm của thread để broadcast MIB mỗi 8 giây
void* broadcast_mib(void* arg) {
    int sock;
    struct sockaddr_in broadcast_addr;
    int broadcast_permission = 1;
    mib_t mib_message;
    rrc_paging_t rrc_paging;

    // Tạo socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Cấu hình socket để cho phép gửi broadcast
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_permission, sizeof(broadcast_permission)) < 0) {
        perror("setsockopt failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ broadcast
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    while (1) {
        // Kiểm tra nếu có UE trong hàng đợi tại SFN hiện tại
        int SFN_temp = sfn;
        if (queueSize[SFN_temp] > 0) {
            memset(&rrc_paging,0,sizeof(rrc_paging));
            int count = dequeueTMSI(SFN_temp, rrc_paging.pagingRecordList);
            if (count > 0) {
                rrc_paging.message_id = 101;

                // Gửi bản tin RRC Paging qua broadcast
                if (sendto(sock, &rrc_paging, sizeof(rrc_paging), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
                    perror("sendto failed");
                    close(sock);
                    exit(EXIT_FAILURE);
                }

                printf("[SFN = %u] Broadcasted RRC Paging: message_id = %d, UEs count = %d\n", SFN_temp, rrc_paging.message_id, count);
                for (int i = 0; i < count; i++) {
                    printf("\t\tUE ID: %d\n", rrc_paging.pagingRecordList[i]);
                }
            }
        }

        // Chỉ broadcast khi SFN chia hết cho 8
        if (sfn % 8 == 0) {
            // Tạo bản tin MIB với message ID và SFN hiện tại
            mib_message.message_id = MESSAGE_ID;
            mib_message.sfn = sfn;

            // Gửi bản tin broadcast (gửi toàn bộ struct)
            if (sendto(sock, &mib_message, sizeof(mib_message), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
                perror("sendto failed");
                close(sock);
                exit(EXIT_FAILURE);
            }

            printf("Broadcasted: message_id = %d, sfn = %u\n", mib_message.message_id, mib_message.sfn);
        }
        sleep(1);
    }

    close(sock);
    return NULL;
}

// Hàm xử lý kết nối TCP với AMF
void* tcp_server(void* arg) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Tạo socket TCP
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ và port cho gNB
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Set sockopt to handle errno 98
	int opt = 1;
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
	  perror("setsockopt");
	  exit(EXIT_FAILURE);
	}

    // Liên kết socket với địa chỉ và port
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Lắng nghe kết nối từ AMF
    if (listen(server_sock, 5) < 0) {
        perror("listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("gNB TCP server is listening on port %d...\n", SERVER_PORT);

    // Chấp nhận kết nối từ AMF
    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len)) > 0) {
        printf("Connected to AMF\n");

        // Nhận dữ liệu từ AMF
        while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            buffer[bytes_received] = '\0';
            ngap_paging_t paging;
            memcpy(&paging, buffer, sizeof(ngap_paging_t));
            message_handler(&paging);
        }

        close(client_sock);
    }

    close(server_sock);
    return NULL;
}

int main() {
    pthread_t sfn_thread, broadcast_thread, tcp_thread;

    // Tạo thread để tăng SFN
    if (pthread_create(&sfn_thread, NULL, sfn_counter, NULL) != 0) {
        perror("Failed to create SFN thread");
        exit(EXIT_FAILURE);
    }

    // Tạo thread để broadcast MIB khi SFN chia hết cho 8
    if (pthread_create(&broadcast_thread, NULL, broadcast_mib, NULL) != 0) {
        perror("Failed to create broadcast thread");
        exit(EXIT_FAILURE);
    }

    // Tạo thread để làm TCP server
    if (pthread_create(&tcp_thread, NULL, tcp_server, NULL) != 0) {
        perror("Failed to create TCP server thread");
        exit(EXIT_FAILURE);
    }

    // Chờ các thread kết thúc (thực tế sẽ không bao giờ kết thúc trong chương trình này)
    pthread_join(sfn_thread, NULL);
    pthread_join(broadcast_thread, NULL);
    pthread_join(tcp_thread, NULL);

    return 0;
}

