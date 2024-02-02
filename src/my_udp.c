#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <poll.h>

#include "my_udp.h"

#define DEBUG 1

#define PACKET_SIZE 1024
#define RETRY_COUNT 3

void handle_error(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct frame_t {
    int frame_id;

    union {
        struct {
            char data[PACKET_SIZE];
        } data;

        struct {
            int bytes;
        } info;
    };

    enum {
        AWK,
        DATA,
        START,
    } type;
};

typedef struct frame_t frame_t;

static void print_frame(frame_t* frame) {
    printf("FRAME: %d\n", frame->frame_id);

    switch (frame->type) {
    case AWK:
        printf("  AWK\n");
        break;
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

static bool send_frame(frame_t* frame, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    if (dest_addr == NULL) {
        if (send(sockfd, frame, sizeof(frame_t), 0) == -1) {
            handle_error("send");
        }
    }
    else {
        if (sendto(sockfd, frame, sizeof(frame_t), 0, dest_addr, *dest_addr_len) == -1) {
            handle_error("sendto");
        }
    }

    return false;
}

static bool recv_frame(int sockfd, frame_t* frame, sockaddr* client_addr, socklen_t* client_addr_len) {
    struct pollfd pfd[1];

    pfd[0].fd = sockfd;
    pfd[0].events = POLLIN;

    int num_events = poll(pfd, 1, 1000);
    if (num_events == 0) {
        return true;
    }

    int pollin_happened = pfd[0].revents & POLLIN;

    if (pollin_happened) {
        if (client_addr == NULL) {
            if (recv(sockfd, frame, sizeof(frame_t), 0) == -1) {
                handle_error("recv");
            }
        }
        else {
            if (recvfrom(sockfd, frame, sizeof(frame_t), 0, client_addr, client_addr_len) == -1) {
                handle_error("recvfrom");
            }
        }
    }
    else {
        printf("Unexpected poll event\n");
        return true;
    }

    return false;
}

static bool send_frame_awk(frame_t* frame, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (send_frame(frame, sockfd, dest_addr, dest_addr_len)) {
            printf("Failed to send frame\n");
            continue;
        }

        frame_t awk_frame;
        if (recv_frame(sockfd, &awk_frame, dest_addr, dest_addr_len)) {
            printf("Failed to receive AWK frame\n");
            continue;
        }

        // we should only receive an AWK frame
        if (awk_frame.type != AWK) {
            fprintf(stderr, "Expected AWK frame\n");
            return true;
        }

        if (awk_frame.frame_id == frame->frame_id) {
            return false;
        }
    }

    printf("Failed to send frame:\n");

    print_frame(frame);

    printf("Transaction dropped\n");

    return true;
}

bool recv_frame_awk(int sockfd, frame_t* frame, sockaddr* client_addr, socklen_t* client_addr_len) {
    if (recv_frame(sockfd, frame, client_addr, client_addr_len)) {
        return true;
    }

    // don't except awks
    if (frame->type == AWK) {
        printf("Received AWK frame\n");
        exit(EXIT_FAILURE);
    }

    // send ack
    frame_t awk;

    awk.type = AWK;
    awk.frame_id = frame->frame_id;

    send_frame(&awk, sockfd, client_addr, client_addr_len);

    return false;
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
    frame.info.bytes = len;
    frame.frame_id = 0;

    if (send_frame_awk(&frame, sockfd, dest_addr, dest_addr_len)) {
        return -1;
    }

    // send data frames sequentially
    for (int i = 1; i <= frame_count; i++) {
        frame.frame_id = i;
        frame.type = DATA;

        // copy data into frame.
        int data_len = min(len - PACKET_SIZE * (i - 1), PACKET_SIZE);
        // zero memory incase not full frame
        memset(&frame.data.data, 0, PACKET_SIZE);
        memcpy(&frame.data.data, msg + PACKET_SIZE * (i - 1), data_len);

        if (send_frame_awk(&frame, sockfd, dest_addr, dest_addr_len)) {
            return -1;
        }
    }

#if DEBUG
    printf("Sent %d bytes\n", len);
#endif

    return len;
}

void* recv_data(int sockfd, int* len, sockaddr* client_addr, socklen_t* client_addr_len) {
    // receive start frame
    frame_t frame;

    if (recv_frame_awk(sockfd, &frame, client_addr, client_addr_len)) {
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
    for (int i = 1; i <= frame_count; i++) {
        if (recv_frame_awk(sockfd, &frame, client_addr, client_addr_len)) {
            free(data);
            *len = 0;
            return NULL;
        }

        // copy data into buffer
        int data_len = min(*len - PACKET_SIZE * (i - 1), PACKET_SIZE);
        memcpy(data + PACKET_SIZE * (frame.frame_id - 1), &frame.data.data, data_len);
    }

#if DEBUG
    printf("Received %d bytes\n", *len);
#endif

    return data;
}
