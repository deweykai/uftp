#ifndef MY_FTP_H
#define MY_FTP_H

#include "my_udp.h"

enum ftp_command { // all commands send a ftp_response. The response comes before any sent data.
    GET, // CMD(s) -> FILENAME(s) -> RESPONSE(r)
    PUT, // CMD(s) -> FILENAME(s) -> DATA(s)
    DELETE, // CMD(s) -> FILENAME(s)
    LS, // CMD(s) -> RESPONSE(r)
    EXIT, // CMD(s)
};

enum ftp_response {
    OK, // RESPONSE(r)
    ERROR, // RESPONSE(r)
};

typedef enum ftp_command ftp_command;
typedef enum ftp_response ftp_response;

void handle_session(int s);

// void handler();

// void recv_command();

#endif // MY_FTP_H
