/* Author: Kai Dewey
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include "my_repl.h"

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

    int s = -1;
    // loop through all the results and bind to the first we can
    for (struct addrinfo* p = res; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
            perror("connect");
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
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }

    int s = get_socket(argv[1], argv[2]);
    my_repl(s);

    // char* msg = "what is up!";
    // ftp_put(s, "foo.txt", msg, strlen(msg));
    // char* files = ftp_ls(s);
    // if (files != NULL) {
    //     printf("%s\n", files);
    //     free(files);
    // }


    // char* data = ftp_get(s, "foo.txt");
    // if (data != NULL) {
    //     printf("%s\n", data);
    //     free(data);
    // }

    // ftp_delete(s, "foo.txt");
    // ftp_exit(s);

    return 0;
}
