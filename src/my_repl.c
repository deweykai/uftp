#include "my_udp.h"
#include "my_ftp.h"
#include "my_ftp_client.h"
#include "my_repl.h"

#include <regex.h>

static bool handle_get(int sockfd, char* filename) {
    int len;
    char* data = ftp_get(sockfd, filename, &len);
    if (data != NULL) {
        // write data to local file

        FILE* file = fopen(filename, "w");
        if (file == NULL) {
            fprintf(stderr, "GET: failed to open file: \"%s\"\n", filename);
            free(data);
            return false;
        }

        if (fwrite(data, 1, len, file) != (unsigned long)len) {
            fprintf(stderr, "GET: failed to write to file: \"%s\"\n", filename);
            free(data);
            fclose(file);
            return false;
        }

        printf("wrote local file: \"%s\"\n", filename);
        fclose(file);
        free(data);
    }
    return false;
}

static bool handle_put(int sockfd, char* filename) {
    // read data from local file

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "PUT: failed to open file: \"%s\"\n", filename);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long len = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = malloc(len);
    if (fread(data, 1, len, file) != (unsigned long)len) {
        fprintf(stderr, "PUT: failed to read from file: \"%s\"\n", filename);
        free(data);
        fclose(file);
        return false;
    }

    fclose(file);

    printf("PUT: sending file (%lu): \"%s\"\n", len, filename);

    ftp_put(sockfd, filename, data, len);
    return false;
}

static bool handle_delete(int sockfd, char* filename) {
    ftp_delete(sockfd, filename);
    return false;
}

static bool handle_ls(int sockfd) {
    char* files = ftp_ls(sockfd);
    if (files != NULL) {
        printf("%s", files);
        free(files);
    }
    return false;
}

/// @brief Handle a line of input from the user
/// @param sockfd 
/// @param line 
/// @return should program exit
bool handle_line(int sockfd, char* line) {
    // Match against ftp commands
    // get <filename>
    // put <filename>
    // delete <filename>
    // ls
    // exit

    regex_t re;
    char* pattern = "(get|put|delete|ls|exit|cat)([[:space:]]+([^[:space:]]+))?";
    int reti = regcomp(&re, pattern, REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        return true;
    }

    regmatch_t matches[4];
    reti = regexec(&re, line, 4, matches, 0);
    if (!reti) {
        // read the command and arg
        char* cmd = NULL;
        char* arg = NULL;
        if (matches[1].rm_so != -1) {
            int len = matches[1].rm_eo - matches[1].rm_so;
            cmd = malloc(len + 1);
            strncpy(cmd, line + matches[1].rm_so, len);
            cmd[len] = '\0';
        }

        if (matches[3].rm_so != -1) {
            int len = matches[3].rm_eo - matches[3].rm_so;
            arg = malloc(len + 1);
            strncpy(arg, line + matches[3].rm_so, len);
            arg[len] = '\0';
        }

        // CAT
        if (strcmp(cmd, "cat") == 0) {
            if (arg == NULL) {
                printf("Usage: cat <filename>\n");
                return false;
            }

            int len;
            char* data = ftp_get(sockfd, arg, &len);
            if (data != NULL) {
                if (len < 1000) {
                    // don't print huge files
                    printf("%s\n", data);
                }
                else {
                    printf("file too large to print\n");
                }
                free(data);
            }

            return false;
        }

        // GET
        else if (strcmp(cmd, "get") == 0) {
            if (arg == NULL) {
                printf("Usage: get <filename>\n");
                return false;
            }

            return handle_get(sockfd, arg);
        }

        // PUT
        else if (strcmp(cmd, "put") == 0) {
            if (arg == NULL) {
                printf("Usage: put <filename>\n");
                return false;
            }

            return handle_put(sockfd, arg);
        }

        // DELETE
        else if (strcmp(cmd, "delete") == 0) {
            if (arg == NULL) {
                printf("Usage: delete <filename>\n");
                return false;
            }

            return handle_delete(sockfd, arg);
        }

        // LS
        else if (strcmp(cmd, "ls") == 0 && arg == NULL) {
            if (arg != NULL) {
                printf("Usage: ls\n");
                return false;
            }

            return handle_ls(sockfd);
        }

        // EXIT
        else if (strcmp(cmd, "exit") == 0 && arg == NULL) {
            if (arg != NULL) {
                printf("Usage: exit\n");
                return false;
            }
            ftp_exit(sockfd);
            return true;
        }

        if (cmd != NULL) {
            free(cmd);
        }
        if (arg != NULL) {
            free(arg);
        }
    }
    else if (reti == REG_NOMATCH) {
        printf("Command not found\n");
    }
    else {
        char msgbuf[100];
        regerror(reti, &re, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
    }

    return false;
}

void my_repl(int sockfd) {
    char* line = NULL;
    size_t len = 0;
    ssize_t nread;
    printf("myftp> ");
    fflush(stdout);
    while ((nread = getline(&line, &len, stdin)) != -1) {
        if (nread > 1) {
            line[nread - 1] = '\0';
            if (handle_line(sockfd, line)) {
                clear_remaining_input(sockfd);
                return;
            }
            clear_remaining_input(sockfd);
        }

        printf("myftp> ");
        fflush(stdout);
    }
    free(line);
}
