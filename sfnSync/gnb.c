#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BROADCAST_PORT 5000
#define BROADCAST_IP "255.255.255.255"
#define MESSAGE_ID 11

unsigned int sfn = 0;  // System Frame Number (SFN)

// Định nghĩa cấu trúc MIB
typedef struct {
    int message_id;
    unsigned int sfn;
} mib_t;

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
		// Đợi 8 giây
		if (sfn % 8 == 0)
		{
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

int main() {
    pthread_t sfn_thread, broadcast_thread;

    // Tạo thread để tăng SFN
    if (pthread_create(&sfn_thread, NULL, sfn_counter, NULL) != 0) {
        perror("Failed to create SFN thread");
        exit(EXIT_FAILURE);
    }

    // Tạo thread để broadcast MIB
    if (pthread_create(&broadcast_thread, NULL, broadcast_mib, NULL) != 0) {
        perror("Failed to create broadcast thread");
        exit(EXIT_FAILURE);
    }

    // Chờ các thread kết thúc (thực tế sẽ không bao giờ kết thúc trong chương trình này)
    pthread_join(sfn_thread, NULL);
    pthread_join(broadcast_thread, NULL);

    return 0;
}

