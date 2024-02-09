#ifndef MY_FTP_CLIENT_H
#define MY_FTP_CLIENT_H

#include "common.h"
#include "my_ftp.h"

char* ftp_get(int s, char* filename, int* len);
void ftp_put(int s, char* filename, char* filedata, int filedata_len);
void ftp_delete(int s, char* filename);
char* ftp_ls(int s);
void ftp_exit(int s);

void repl(int sockfd);


#endif // MY_FTP_CLIENT_H
