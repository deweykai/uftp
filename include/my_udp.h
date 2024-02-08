#ifndef MY_UDP_H
#define MY_UDP_H

#include "common.h"

#define PACKET_SIZE 1024
#define RETRY_COUNT 3


typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_storage sockaddr_storage;

void handle_error(const char* msg);

int send_data(int sockfd, const char* msg, int len, sockaddr* dest_addr, socklen_t* dest_addr_len);

void* recv_data(int sockfd, int* len, sockaddr* client_addr, socklen_t* client_addr_len);

#endif
