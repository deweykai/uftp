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
#include "my_ftp.h"

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

void test_get(int s) {
    // hardcode GET

    // command
    ftp_command data = GET;
    send_data(s, (char*)&data, sizeof(GET), NULL, 0);

    // filename
    char buf[32] = "foo.txt";
    send_data(s, buf, sizeof(buf), NULL, 0);

    // status
    int len;
    ftp_response* status = (ftp_response*)recv_data(s, &len, NULL, NULL);
    if (*status == ERROR) {
        printf("failed to get file: %s\n", buf);
        free(status);
        return;
    }

    // data
    free(status);
    char* rdata = recv_data(s, &len, NULL, 0);

    printf("%s\n", rdata);
    free(rdata);
}

void test_put(int s) {
    // hardcode PUT

    // command
    ftp_command data = PUT;
    send_data(s, (char*)&data, sizeof(PUT), NULL, 0);

    // filename
    char* filename = "foo.txt";
    send_data(s, filename, strlen(filename), NULL, 0);

    char* filedata = "hello world!";
    send_data(s, filedata, strlen(filedata), NULL, 0);

    // status
    int len;
    ftp_response* status = (ftp_response*)recv_data(s, &len, NULL, NULL);
    if (status == NULL) {
        printf("No response\n");
        return;
    }
    if (*status == ERROR) {
        printf("failed to put file: %s\n", filename);
        free(status);
        return;
    }
    else {
        printf("put file: %s\n", filename);
        free(status);
    }
}

void test_delete(int s) {
    // hardcode DELETE

    // command
    ftp_command data = DELETE;
    send_data(s, (char*)&data, sizeof(DELETE), NULL, 0);

    // filename
    char* filename = "foo.txt";
    send_data(s, filename, strlen(filename), NULL, 0);

    int len;
    ftp_response* status = (ftp_response*)recv_data(s, &len, NULL, NULL);
    if (status == NULL) {
        printf("No response\n");
        return;
    }
    if (*status == ERROR) {
        printf("failed to delete file\n");
        free(status);
        return;
    }
    else {
        printf("deleted file\n");
        free(status);
    }
}

void test_ls(int s) {
    // hardcode LS

    // command
    ftp_command data = LS;
    send_data(s, (char*)&data, sizeof(LS), NULL, 0);

    // data
    int len;

    ftp_response* status = (ftp_response*)recv_data(s, &len, NULL, NULL);
    if (*status == ERROR) {
        printf("failed to ls\n");
        free(status);
        return;
    }

    free(status);
    char* rdata = recv_data(s, &len, NULL, 0);

    printf("%s\n", rdata);

    free(rdata);

}


int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }

    int s = get_socket(argv[1], argv[2]);

    test_put(s);
    test_ls(s);
    test_get(s);
    test_delete(s);

    // char buf[32] = "hello world!";
    // send_data(s, buf, sizeof(buf), NULL, NULL);

    // int len;
    // char* data = recv_data(s, &len, NULL, NULL);
    // printf("%s\n", data);

    // TODO: CRC checksum

    return 0;
}
