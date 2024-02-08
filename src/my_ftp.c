#include <dirent.h>

#include "my_ftp.h"

void print_command(ftp_command cmd) {
    switch (cmd) {
    case GET:
        printf("GET\n");
        break;
    case PUT:
        printf("PUT\n");
        break;
    case DELETE:
        printf("DELETE\n");
        break;
    case LS:
        printf("LS\n");
        break;
    case EXIT:
        printf("EXIT\n");
    }
}

static void response_ok(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    ftp_response response = OK;
    send_data(s, (char*)&response, sizeof(response), client_addr, client_addr_len);
}

static void response_error(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    ftp_response response = ERROR;
    send_data(s, (char*)&response, sizeof(response), client_addr, client_addr_len);
}

static void handle_get(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    int len;
    char* filename = (char*)recv_data(s, &len, (sockaddr*)client_addr, client_addr_len);
    if (filename == NULL) {
        fprintf(stderr, "GET: failed to read filename\n");
        response_error(s, client_addr, client_addr_len);
        return;
    }
    FILE* file = fopen(filename, "r");
    free(filename);

    if (file == NULL) {
        response_error(s, client_addr, client_addr_len);
        // no need to print error if file not found
        // perror("fopen");
        return;
    }

    // get file size
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // read file into memory
    char* data = (char*)malloc(file_size);
    if (data == NULL) {
        response_error(s, client_addr, client_addr_len);
        perror("malloc");
        return;
    }

    int bytes = fread(data, 1, file_size, file);
    if (bytes < 0) {
        response_error(s, client_addr, client_addr_len);
        free(data);
        perror("fread");
        return;
    }
    fclose(file);

    response_ok(s, client_addr, client_addr_len);
    send_data(s, data, file_size, client_addr, client_addr_len);

    free(data);
}

static void handle_put(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    // get filename
    char* filename = (char*)recv_data(s, NULL, client_addr, client_addr_len);
    if (filename == NULL) {
        fprintf(stderr, "PUT: failed to read filename\n");
        response_error(s, client_addr, client_addr_len);
        return;
    }

    // get file data
    char* filedata = (char*)recv_data(s, NULL, client_addr, client_addr_len);
    if (filedata == NULL) {
        fprintf(stderr, "PUT: failed to read filedata\n");
        free(filename);
        response_error(s, client_addr, client_addr_len);
        return;
    }

    // write to file
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("fopen");
        free(filename);
        free(filedata);
        response_error(s, client_addr, client_addr_len);
        return;
    }

    int bytes = fwrite(filedata, 1, strlen(filedata), file);
    if (bytes < 0) {
        perror("fwrite");
        free(filename);
        free(filedata);
        fclose(file);
        response_error(s, client_addr, client_addr_len);
        return;
    }
    fclose(file);

    // send response
    response_ok(s, client_addr, client_addr_len);

    // free data
    free(filename);
    free(filedata);
}

static void handle_ls(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    DIR* dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    char* files = NULL;
    size_t files_size = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Only regular files
            size_t filename_len = strlen(entry->d_name);
            size_t new_size = files_size + filename_len + 1;  // +1 for newline character

            char* new_files = realloc(files, new_size);
            if (new_files == NULL) {
                perror("realloc");
                free(files);
                closedir(dir);
                response_error(s, client_addr, client_addr_len);
                return;
            }

            files = new_files;
            files_size = new_size;

            strcat(files, entry->d_name);
            strcat(files, "\n");
        }
    }
    closedir(dir);

    if (files == NULL) {
        files = strdup("No files found\n");
    }

    response_ok(s, client_addr, client_addr_len);
    send_data(s, files, strlen(files) + 1, client_addr, client_addr_len);

    free(files);
}

static void handle_delete(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    // get filename
    char* filename = (char*)recv_data(s, NULL, client_addr, client_addr_len);
    if (filename == NULL) {
        fprintf(stderr, "failed to read filename\n");
        response_error(s, client_addr, client_addr_len);
        free(filename);
        return;
    }

    // delete file
    if (remove(filename) != 0) {
        perror("remove");
        free(filename);
        response_error(s, client_addr, client_addr_len);
        return;
    }

    response_ok(s, client_addr, client_addr_len);

    // free variables
    free(filename);
}

static void handle_exit(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    response_ok(s, client_addr, client_addr_len);
}

void handle_session(int s) {
    while (true) {
        sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int len;

        ftp_command* data = (ftp_command*)recv_data(s, &len, (sockaddr*)&client_addr, &client_addr_len);
        // no command received
        if (data == NULL) {
            continue;
        }

        printf("Received Command: ");
        print_command(*data);

        switch (*data) {
        case GET:
            handle_get(s, (sockaddr*)&client_addr, &client_addr_len);
            break;
        case PUT:
            handle_put(s, (sockaddr*)&client_addr, &client_addr_len);
            break;
        case DELETE:
            handle_delete(s, (sockaddr*)&client_addr, &client_addr_len);
            break;
        case LS:
            handle_ls(s, (sockaddr*)&client_addr, &client_addr_len);
            break;
        case EXIT:
            handle_exit(s, (sockaddr*)&client_addr, &client_addr_len);
            return;
        }
    }
}
