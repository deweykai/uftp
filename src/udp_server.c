/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 

#include "my_udp.h"
#include "my_ftp.h"

#define BACKLOG 5

int get_socket(char* port) {
    struct addrinfo hints, * res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    int s;
    // loop through all the results and bind to the first we can
    for (struct addrinfo* p = res; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(s, p->ai_addr, p->ai_addrlen) == -1) {
            close(s);
            perror("listener: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    return s;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    int s = get_socket(argv[1]);

    while (1) {
        handle_server(s);
        // if (data == NULL) {
        //     continue;
        // }
        // printf("%s\n", data);

        // send_data(s, data, len, (sockaddr*)&client_addr, &client_addr_len);
        // free(data);
    }

    return 0;
}
