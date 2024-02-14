/**
 * @file my_udp.c
 * @author Kai Dewey
 */

#include "my_udp.h"

#include <poll.h>
#include <sys/time.h>

#define DEBUG 0
#define DEFAULT_TIMEOUT_MS 500
#define DEFAULT_SEND_TIMEOUT_MS 100
#define USE_GO_BACK_N 1
#define MAX_GO_BACK_N 1024
#define GBN_THRESHOLD_US 200

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ABS(a) ((a) < 0 ? (-a) : (a))

static int UDP_TIMEOUT_MS = DEFAULT_TIMEOUT_MS;

static long long get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

static long long get_time_ms(void) {
    return get_time_us() / 1000;
}



void handle_error(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct frame_header_t {
    int frame_id;
    enum {
        DATA,
        START,
        ACK,
        END,
    } type;
};

typedef struct frame_header_t frame_header_t;

struct frame_t {
    frame_header_t header;
    union {
        char data[PACKET_SIZE];

        struct {
            int bytes;
        } info;
    } packet;

};

typedef struct frame_t frame_t;

static void print_frame(frame_t* frame) {
    long long time = get_time_ms();
    printf("[%lld] ", time);
    printf("[FRAME: %d]", frame->header.frame_id);

    switch (frame->header.type) {
    case DATA:
        printf("  DATA\n");
        break;
    case START:
        printf("  START\n");
        printf("  bytes = %d\n", frame->packet.info.bytes);
    case ACK:
        printf("  ACK\n");
        break;
    case END:
        printf("  END\n");
        break;
    }
}

/// @brief Calculate the number number of frames needed to send len bytes
/// @param len 
/// @return number of frames needed to send len bytes
static int get_frame_count(int len) {
    int frame_count = len / PACKET_SIZE;
    if (len % PACKET_SIZE != 0) {
        frame_count += 1;
    }
    return frame_count;
}

#ifdef MSG_DONTWAIT
void clear_remaining_input(int sockfd) {

    int n;
    bool empty = true;
    char buf[1];
    while ((n = recv(sockfd, buf, 1, MSG_DONTWAIT)) > 0) {
        empty = false;
    }
    if (!empty) {
        fprintf(stderr, "clear_remaining_input: socket was not empty\n");
    }
}
#else
void clear_remaining_input(int) {
}
#endif

/// @brief Send data with timeout
/// @param data 
/// @param data_len 
/// @param sockfd 
/// @param dest_addr 
/// @param dest_addr_len 
/// @return Return true on failure
static bool send_timeout(char* data, int data_len, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    struct pollfd pfd[1];

    pfd[0].fd = sockfd;
    pfd[0].events = POLLOUT;

    long long start = get_time_ms();
    while (get_time_ms() - start < DEFAULT_SEND_TIMEOUT_MS) {
        int num_events = poll(pfd, 1, DEFAULT_SEND_TIMEOUT_MS - (get_time_ms() - start));

        if (num_events == 0) {
            return true;
        }

        int pollin_happened = pfd[0].revents & POLLOUT;
        if (pollin_happened) {
            break;
        }
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

/// @brief Receive data from socket with timeout
/// @param data 
/// @param data_len 
/// @param sockfd 
/// @param client_addr 
/// @param client_addr_len 
/// @return Return true on failure
static bool recv_timeout(char* data, int data_len, int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {
    struct pollfd pfd[1];

    pfd[0].fd = sockfd;
    pfd[0].events = POLLIN;

    long long start = get_time_ms();

    while (get_time_ms() - start < UDP_TIMEOUT_MS) {
        int num_events = poll(pfd, 1, UDP_TIMEOUT_MS - (get_time_ms() - start));

        if (num_events == 0) {
            return true;
        }

        int pollin_happened = pfd[0].revents & POLLIN;
        if (pollin_happened) {
            break;
        }
    }

    if (client_addr == NULL) {
        if (recv(sockfd, data, data_len, 0) == -1) {
            handle_error("recv");
            return true;
        }
    }
    else {
        if (recvfrom(sockfd, data, data_len, 0, client_addr, client_addr_len) == -1) {
            handle_error("recvfrom");
            return true;
        }
    }

    return false;
}

/// @brief Send frame to socket
/// @param frame 
/// @param sockfd 
/// @param dest_addr 
/// @param dest_addr_len 
/// @return Return true on failure
static bool send_frame(frame_t* frame, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
#if DEBUG > 1
    printf("SEND ");
    print_frame(frame);
#endif
    int len = sizeof(frame_header_t);
    switch (frame->header.type) {
    case DATA:
        len += sizeof(frame->packet);
        break;
    case START:
        len += sizeof(frame->packet);
        break;
    case ACK:
    case END:
        break;
    }
    bool ret = send_timeout((char*)frame, len, sockfd, dest_addr, dest_addr_len);
    return ret;
}

/// @brief Get a frame from the socket
/// @param frame 
/// @param sockfd 
/// @param client_addr 
/// @param client_addr_len 
/// @return Return true on failure
static bool recv_frame(frame_t* frame, int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {
    bool ret = recv_timeout((char*)frame, sizeof(frame_t), sockfd, client_addr, client_addr_len);
    if (ret) {
        return true;
    }

#if DEBUG > 1
    printf("RECV ");
    print_frame(frame);
#endif

    return false;
}

/// @brief Wait for an acknowledge
/// @param frame_id 
/// @param ack_frame_id 
/// @param sockfd 
/// @param client_addr 
/// @param client_addr_len 
/// @return Return true on failure. Failure is likely due to timeout
static bool try_recv_ack(int frame_id, int* ack_frame_id, int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {

    // wait for ack for timeout
    frame_t frame;
    while (recv_frame(&frame, sockfd, client_addr, client_addr_len) == false) {
        *ack_frame_id = frame.header.frame_id;

        // we allow for a header frame id to be greater than the frame id
        // since ack are only sent for frames that the client has received in order.
        // Due to this, we can garantee that the client has has received any skipped frames.
        // if we are recieving a future ack frame, we most likely has missed ack.
        if (frame.header.type == ACK && frame.header.frame_id >= frame_id) {
            return false;
        }
    }


    return true;
}

/// @brief Send a frame of data and wait for an acknowledge
/// @param frame 
/// @param wait_ack 
/// @param sockfd 
/// @param dest_addr 
/// @param dest_addr_len 
/// @return 
static bool send_frame_ack(frame_t* frame, bool wait_ack, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    // send frame
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (send_frame(frame, sockfd, dest_addr, dest_addr_len)) {
#if DEBUG > 1
            printf("Failed to send frame\n");
#endif
            continue;
        }


        // check acknowledge
        // we are using GO-BACK-N so we don't need to wait for ack for data frames.
        // We will wait for the last ending frame, and for the start frame.
        if (!wait_ack) {
            return false;
        }

        int ack_frame_id;
        if (try_recv_ack(frame->header.frame_id, &ack_frame_id, sockfd, dest_addr, dest_addr_len)) {
#if DEBUG > 1
            printf("Failed to receive ack\n");
#endif
            continue;
        }
        else {
            return false;
        }
    }

    printf("Failed to send frame:\n");

    print_frame(frame);

    printf("Transaction dropped\n");

    return true;

}

/// @brief Receive a frame of data and send an acknowledge
/// @param frame 
/// @param next_frame 
/// @param sockfd 
/// @param client_addr 
/// @param client_addr_len 
/// @return return true on failure
static bool recv_frame_ack(frame_t* frame, int next_frame, int sockfd, sockaddr* client_addr, socklen_t* client_addr_len) {
    // receive frame
    int failures = 0;
    while (failures < RETRY_COUNT) {
        if (recv_frame(frame, sockfd, client_addr, client_addr_len)) {
            failures++;
            continue;
        }

#if DEBUG > 1
        if (frame->header.frame_id != next_frame) {
            printf("Received frame %d, expected %d\n", frame->header.frame_id, next_frame);
        }
#endif

        if (frame->header.frame_id < next_frame) {
            // send again ack in case of failed ack
            // only ack if we have already passed this frame
            frame_t ack;
            ack.header.type = ACK;
            ack.header.frame_id = frame->header.frame_id;

            if (send_frame(&ack, sockfd, client_addr, client_addr_len)) {
                fprintf(stderr, "Failed to send ack\n");
            }
        }
        else if (frame->header.frame_id == next_frame) {
            frame_t ack;
            ack.header.frame_id = frame->header.frame_id;
            ack.header.type = ACK;
            if (send_frame(&ack, sockfd, client_addr, client_addr_len)) {
                fprintf(stderr, "Failed to send ack\n");
                continue;
            }
            return false;
        }
    }

    return true;
}

/// @brief Create a frame to send based in the frame id.
/// @param msg 
/// @param len 
/// @param frame_id 
/// @param wait_ack 
/// @param sockfd 
/// @param dest_addr 
/// @param dest_addr_len 
/// @return 
int send_frame_by_id(const char* msg, int len, int frame_id, bool wait_ack, int sockfd, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    // create a blank data frame
    frame_t frame;
    frame.header.frame_id = frame_id;
    frame.header.type = DATA;
    memset(&frame.packet.data, 0, PACKET_SIZE); // zero memory

    // calculate data offset and size
    int data_offset = PACKET_SIZE * (frame_id - 1);
    int data_size = MIN(len - data_offset, PACKET_SIZE);

    // copy data into frame
    memcpy(&frame.packet.data, msg + data_offset, data_size);

    return send_frame_ack(&frame, wait_ack, sockfd, dest_addr, dest_addr_len);
}

/// @brief Send some data structure
/// @param sockfd socket file descriptor
/// @param msg pointer to data
/// @param len length of data
/// @param dest_addr 
/// @param dest_addr_len 
/// @return Return bytes sent. -1 on failure
int send_data(int sockfd, const char* msg, int len, sockaddr* dest_addr, socklen_t* dest_addr_len) {
    // reset the timeout for new transaction
    UDP_TIMEOUT_MS = DEFAULT_TIMEOUT_MS;
    // count how many frames we expect
    int frame_count = get_frame_count(len);

#if DEBUG
    printf("Expecting to send %d frames\n", frame_count);
#endif

    // build starting frame
    frame_t frame;

    // we send the total size first so client can allocate space.
    memset(&frame, 0, sizeof(frame)); // zero memory
    frame.header.type = START;
    frame.header.frame_id = 0;
    frame.packet.info.bytes = len;

    if (send_frame_ack(&frame, true, sockfd, dest_addr, dest_addr_len)) {
        fprintf(stderr, "Failed to send start frame\n");
        return -1;
    }

#if USE_GO_BACK_N
    // send data frames sequentially
    int base_frame_id = 1;
    int current_frame_id = 1;
    int reset_counter = 0;
    int success_counter = 0;
    int GO_BACK_N = 32;
    int deci_position = 0;

    while (base_frame_id <= frame_count) {
        // send up to base + N frames
        while (current_frame_id <= MIN(base_frame_id + GO_BACK_N - 1, frame_count)) {
            if (send_frame_by_id(msg, len, current_frame_id, false, sockfd, dest_addr, dest_addr_len)) {
                return -1;
            }
            current_frame_id++;
        }

        long long start = get_time_us();
        int ack_frame_id;
        // wait for ack
        if (try_recv_ack(base_frame_id, &ack_frame_id, sockfd, dest_addr, dest_addr_len)) {
            // failed to receive ack from base frame id
            reset_counter++;
            if (reset_counter > RETRY_COUNT) {
                fprintf(stderr, "Failed to send frame\n");
                fprintf(stderr, "Sent %d frames out of %d\n", base_frame_id - 1, frame_count);
                return -1;
            }
#if DEBUG
            printf("Resetting base frame id to %d\n", base_frame_id);
#endif
            current_frame_id = base_frame_id;
            success_counter = 0;

            if (reset_counter > 1) {
                // decrease GO_BACK_N if we are doing poorly
                GO_BACK_N = GO_BACK_N < 2 ? 1 : GO_BACK_N / 2;
#if DEBUG
                printf("Decreasing GO_BACK_N to %d\n", GO_BACK_N);
#endif
            }

            // increase timeout for failed frame
            UDP_TIMEOUT_MS = MIN(UDP_TIMEOUT_MS * 2, 500);
        }
        else {
            reset_counter = 0;
            success_counter++;
            base_frame_id = ack_frame_id + 1;

            long long end = get_time_us();

            // set timeout to the time it took to receive a valid frame
            int elapsed_ms = ((end - start) / 1000);
            UDP_TIMEOUT_MS = elapsed_ms + 50;


            // for large files, print progress
            if (frame_count > 100) {
                if (base_frame_id >= (frame_count * deci_position / 10)) {
                    printf("Sent %d%% (%d/%d)\n", deci_position * 10, base_frame_id, frame_count);
                    fflush(stdout);
                    deci_position++;
                }
            }

            // increase GO_BACK_N if we are doing well
            if (success_counter >= GO_BACK_N && GO_BACK_N < MAX_GO_BACK_N && end - start > GBN_THRESHOLD_US) {
                GO_BACK_N = GO_BACK_N * 2;
#if DEBUG
                printf("Increasing GO_BACK_N to %d\n", GO_BACK_N);
                fflush(stdout);
#endif
            }
        }
    }
#else // USE_GO_BACK_N
    for (int i = 1; i <= frame_count; i++) {
        if (send_frame_by_id(msg, len, i, true, sockfd, dest_addr, dest_addr_len)) {
            return -1;
        }
    }
#endif // USE_GO_BACK_N

    // send end frame
    memset(&frame, 0, sizeof(frame)); // zero memory
    frame.header.type = END;
    frame.header.frame_id = frame_count + 1;

    if (send_frame_ack(&frame, true, sockfd, dest_addr, dest_addr_len)) {
        fprintf(stderr, "Failed to send start frame\n");
        return -1;
    }

#if DEBUG
    printf("Sent %d bytes\n", len);
#endif

    return len;
}

/// @brief Receive data from socket
/// @param sockfd 
/// @param len 
/// @param client_addr 
/// @param client_addr_len 
/// @return pointer to data on success, NULL on failure
void* recv_data(int sockfd, int* len, sockaddr* client_addr, socklen_t* client_addr_len) {
    UDP_TIMEOUT_MS = DEFAULT_TIMEOUT_MS;
    // receive start frame
    frame_t frame;

    // give a dummy length to avoid null error
    int dummy_len = 0;
    if (len == NULL) {
        len = &dummy_len;
    }

    // poll for start frame
    if (recv_frame_ack(&frame, 0, sockfd, client_addr, client_addr_len)) {
        *len = 0;
        return NULL;
    }

    if (frame.header.type != START) {
#if DEBUG
        fprintf(stderr, "Expected START frame\n");
#endif
        * len = 0;
        return NULL;
    }


    *len = frame.packet.info.bytes;

    // allocate memory for data
    char* data = (char*)malloc(*len);

    // receive data frames
    int frame_count = get_frame_count(frame.packet.info.bytes);
#if DEBUG
    printf("Expecting to receive %d frames\n", frame_count);
#endif
    int data_len = *len;

    int deci_position = 0;
    for (int i = 1; i <= frame_count; i++) {
        if (recv_frame_ack(&frame, i, sockfd, client_addr, client_addr_len)) {
            free(data);
            *len = 0;
            fprintf(stderr, "Failed to receive frame %d\n", i);
            return NULL;
        }

        if (frame_count > 100) {
            if (i >= (frame_count * deci_position / 10)) {
                printf("Received %d%% (%d/%d)\n", deci_position * 10, i, frame_count);
                fflush(stdout);
                deci_position++;
            }
        }

        // copy data into buffer
        memcpy(data + PACKET_SIZE * (frame.header.frame_id - 1), &frame.packet.data, MIN(data_len, PACKET_SIZE));
        data_len = data_len - PACKET_SIZE;
    }

    // receive end frame
    if (recv_frame_ack(&frame, frame_count + 1, sockfd, client_addr, client_addr_len)) {
        free(data);
        *len = 0;
        fprintf(stderr, "Failed to receive end frame\n");
        return NULL;
    }


#if DEBUG
    printf("Received %d bytes\n", *len);
#endif

    return data;
}
