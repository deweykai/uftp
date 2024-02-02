/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> 

#include "my_udp.h"

int get_socket(char* hostname, char* port) {
    struct addrinfo hints, * res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int status;
    if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        return 3;
    }
    freeaddrinfo(res);

    return s;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }

    int s = get_socket(argv[1], argv[2]);

    char buf[32] = "hello world!";
    send_data(s, buf, sizeof(buf), NULL, NULL);

    int len;
    char* data = recv_data(s, &len, NULL, NULL);
    printf("%s\n", data);

    // TODO: CRC checksum

    return 0;
}
