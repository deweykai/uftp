#include <poll.h>

#include "my_udp.h"

#define DEBUG 1
#define TIMEOUT_MS 1000

void handle_error(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct frame_t {
    int frame_id;
    enum {
        DATA,
        START,
    } type;

    union {
        struct {
            char data[PACKET_SIZE];
        } data;

        struct {
            int bytes;
        } info;
    };

};

typedef struct frame_t frame_t;

struct frame_ack_t {
    int frame_id;
};

typedef struct frame_ack_t frame_ack_t;

static void print_frame(frame_t* frame) {
    printf("FRAME: %d\n", frame->frame_id);

    switch (frame->type) {
    case DATA:
        printf("  DATA\n");
        break;
    case START:
        printf("  START\n");
        printf("  bytes = %d\n", frame->info.bytes);
    }
}

static int min(int a, int b) {
    if (a < b) {
        return a;
    }
    else {
        return b;
    }
}

static int get_frame_count(int len) {
    int frame_count = len / PACKET_SIZE;
    if (len % PACKET_SIZE != 0) {
        frame_count += 1;
    }
    return frame_count;
}

static bool send_timeout(char* data, int data_len, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    struct pollfd pfd[1];

    pfd[0].fd = sockfd;
    pfd[0].events = POLLOUT;

    int num_events = poll(pfd, 1, TIMEOUT_MS);
    if (num_events == 0) {
        return true;
    }

    int pollout_happened = pfd[0].revents & POLLOUT;

    if (!pollout_happened) {
        printf("Unexpected pollout event\n");
        return true;
    }

    if (dest_addr == NULL) {
        if (send(sockfd, data, data_len, 0) == -1) {
            handle_error("send");
        }
    }
    else {
        if (sendto(sockfd, data, data_len, 0, dest_addr, *dest_addr_len) == -1) {
            handle_error("sendto");
        }
    }

    return false;
}

static bool send_frame(frame_t* frame, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    return send_timeout((char*)frame, sizeof(frame_t), sockfd, dest_addr, dest_addr_len);
}

/// @brief Send acknowledge message
/// @param frame for which to send ack
/// @param sockfd 
/// @param dest_addr 
/// @param dest_addr_len 
/// @return true on failure
static bool send_ack(frame_t* frame, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    frame_ack_t ack;
    ack.frame_id = frame->frame_id;
    return send_timeout((char*)&ack, sizeof(frame_ack_t), sockfd, dest_addr, dest_addr_len);

    // frame_t ack;

    // ack.type = ACK;
    // ack.frame_id = frame->frame_id;
    // memset(&ack.data, 0, PACKET_SIZE); // zero memory
    // memcpy(&ack.data.data, "ACK", 3); // set data to ACK for debugging

    // return send_frame(&ack, sockfd, dest_addr, dest_addr_len);
}

static bool recv_timeout(char* data, int data_len, int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {
    struct pollfd pfd[1];

    pfd[0].fd = sockfd;
    pfd[0].events = POLLIN;

    int num_events = poll(pfd, 1, TIMEOUT_MS);
    if (num_events == 0) {
        return true;
    }

    int pollin_happened = pfd[0].revents & POLLIN;

    if (!pollin_happened) {
        printf("Unexpected pollin event\n");
        return true;
    }


    if (client_addr == NULL) {
        if (recv(sockfd, data, data_len, 0) == -1) {
            handle_error("recv");
        }
    }
    else {
        if (recvfrom(sockfd, data, data_len, 0, client_addr, client_addr_len) == -1) {
            handle_error("recvfrom");
        }
    }

    return false;
}

static bool recv_frame(frame_t* frame, int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {
    return recv_timeout((char*)frame, sizeof(frame_t), sockfd, client_addr, client_addr_len);
}

static frame_ack_t recv_ack(int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {
    frame_ack_t ack;
    if (recv_timeout((char*)&ack, sizeof(ack), sockfd, client_addr, client_addr_len)) {
        printf("Failed to receive ACK frame\n");
        ack.frame_id = -1;
    }
    return ack;
}

static bool send_frame_ack(frame_t* frame, bool wait_ack, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (send_frame(frame, sockfd, dest_addr, dest_addr_len)) {
            printf("Failed to send frame\n");
            continue;
        }

        // check acknowledge
        if (!wait_ack) {
            return false;
        }
        frame_ack_t ack = recv_ack(sockfd, dest_addr, dest_addr_len);
        if (ack.frame_id == frame->frame_id) {
            return false;
        }
    }

    printf("Failed to send frame:\n");

    print_frame(frame);

    printf("Transaction dropped\n");

    return true;
}

/// receive frame and resposne with ack message
static bool recv_frame_ack(frame_t* frame, int next_frame, int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (recv_frame(frame, sockfd, client_addr, client_addr_len)) {
            return true;
        }

        if (frame->frame_id != next_frame) {
#if DEBUG
            printf("Received frame %d, expected %d\n", frame->frame_id, next_frame);
#endif
            // try to receive frame multiple times in case of out of order frames
            continue;
        }

        send_ack(frame, sockfd, client_addr, client_addr_len);

        return false;
    }

    return true;
}

int send_frame_by_id(const char* msg, int len, int frame_id, bool wait_ack, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    frame_t frame;
    frame.frame_id = frame_id;
    frame.type = DATA;
    memset(&frame.data, 0, PACKET_SIZE); // zero memory

    int data_offset = PACKET_SIZE * (frame_id - 1);
    int data_size = min(len - data_offset, PACKET_SIZE);

    memcpy(&frame.data.data, msg + data_offset, data_size);

    return send_frame_ack(&frame, wait_ack, sockfd, dest_addr, dest_addr_len);
}

int send_data(int sockfd, const char* msg, int len, sockaddr* dest_addr, socklen_t* dest_addr_len) {

    // count how many frames we expect
    int frame_count = get_frame_count(len);

#if DEBUG
    printf("Expecting %d frames\n", frame_count);
#endif

    // build starting frame
    frame_t frame;

    // we send the total size first so client can allocate space.
    frame.type = START;
    memset(&frame.data, 0, PACKET_SIZE); // zero memory
    frame.info.bytes = len;
    frame.frame_id = 0;

    if (send_frame_ack(&frame, true, sockfd, dest_addr, dest_addr_len)) {
        return -1;
    }

    // send data frames sequentially
    int base_frame_id = 1;
    int current_frame_id = 1;
    int reset_counter = 0;
    int success_counter = 0;
    static int GO_BACK_N = 2;
    while (base_frame_id <= frame_count) {
        // send up to base + N frames
        while (current_frame_id <= min(base_frame_id + GO_BACK_N - 1, frame_count)) {
            if (send_frame_by_id(msg, len, current_frame_id, false, sockfd, dest_addr, dest_addr_len)) {
                return -1;
            }
            current_frame_id++;
        }

        // wait for ack
        frame_ack_t ack = recv_ack(sockfd, dest_addr, dest_addr_len);
        if (ack.frame_id == -1) {
            reset_counter++;
            if (reset_counter > RETRY_COUNT) {
                fprintf(stderr, "Failed to send frame\n");
                return -1;
            }
#if DEBUG
            printf("Resetting base frame id\n");
#endif
            current_frame_id = base_frame_id;
            success_counter = 0;

            GO_BACK_N = GO_BACK_N < 2 ? 1 : GO_BACK_N / 2;
#if DEBUG
            printf("Decreasing GO_BACK_N to %d\n", GO_BACK_N);
#endif
        }
        else {
            reset_counter = 0;
            success_counter++;
            base_frame_id++;
            if (success_counter * 2 >= GO_BACK_N && GO_BACK_N < 1024) {
                GO_BACK_N = GO_BACK_N * 2;
#if DEBUG
                printf("Increasing GO_BACK_N to %d\n", GO_BACK_N);
#endif
            }
        }
    }
    // for (int i = 1; i <= frame_count; i++) {
    //     if (send_frame_by_id(msg, len, i, true, sockfd, dest_addr, dest_addr_len)) {
    //         return -1;
    //     }
    // }

#if DEBUG
    printf("Sent %d bytes\n", len);
#endif

    return len;
}

void* recv_data(int sockfd, int* len, sockaddr* client_addr, socklen_t* client_addr_len) {
    // receive start frame
    frame_t frame;

    // give a dummy length to avoid null error
    int dummy_len = 0;
    if (len == NULL) {
        len = &dummy_len;
    }

    if (recv_frame_ack(&frame, 0, sockfd, client_addr, client_addr_len)) {
        *len = 0;
        return NULL;
    }

    *len = frame.info.bytes;

    // allocate memory for data
    char* data = (char*)malloc(*len);

    // receive data frames
    int frame_count = get_frame_count(frame.info.bytes);
#if DEBUG
    printf("Expecting %d frames\n", frame_count);
#endif
    int data_len = *len;
    for (int i = 1; i <= frame_count; i++) {
        if (recv_frame_ack(&frame, i, sockfd, client_addr, client_addr_len)) {
            free(data);
            *len = 0;
            return NULL;
        }

        // copy data into buffer
        memcpy(data + PACKET_SIZE * (frame.frame_id - 1), &frame.data.data, min(data_len, PACKET_SIZE));
        data_len = data_len - PACKET_SIZE;
    }

#if DEBUG
    printf("Received %d bytes\n", *len);
#endif

    return data;
}
