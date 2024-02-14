/* Author: Kai Dewey
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */
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

    printf("Staring server on port %s\n", port);
    fflush(stdout);

    int s = -1;
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

    if (s == -1) {
        fprintf(stderr, "listener: failed to bind socket\n");
        exit(1);
    }

    return s;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    int s = get_socket(argv[1]);

    while (1) {
        handle_session(s);
        printf("end of client session\n");
    }

    return 0;
}
