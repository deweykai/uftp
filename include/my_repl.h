#ifndef MY_REPL_H
#define MY_REPL_H

#include "common.h"

bool handle_line(int sockfd, char* line);
void my_repl(int sockfd);

#endif // MY_REPL_H
