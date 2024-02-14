#include "my_ftp.h"

#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

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
    if (send_data(s, (char*)&response, sizeof(response), client_addr, client_addr_len) == -1) {
        fprintf(stderr, "RESP_OK: failed to send response\n");
    }
}

static void response_error(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    ftp_response response = ERROR;
    if (send_data(s, (char*)&response, sizeof(response), client_addr, client_addr_len) == -1) {
        fprintf(stderr, "RESP_ERR: failed to send response\n");
    }
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

    if (file == NULL) {
        response_error(s, client_addr, client_addr_len);
        free(filename);
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
        free(filename);
        perror("malloc");
        return;
    }

    int bytes = fread(data, 1, file_size, file);
    if (bytes < 0) {
        response_error(s, client_addr, client_addr_len);
        free(data);
        free(filename);
        perror("fread");
        return;
    }
    fclose(file);

    printf("GET: sending file (%d bytes): \"%s\"\n", file_size, filename);

    response_ok(s, client_addr, client_addr_len);
    if (send_data(s, data, file_size, client_addr, client_addr_len) == -1) {
        fprintf(stderr, "GET: failed to send file\n");
    }

    free(data);
    free(filename);
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
    int len;
    char* filedata = (char*)recv_data(s, &len, client_addr, client_addr_len);
    if (filedata == NULL) {
        fprintf(stderr, "PUT: failed to read filedata\n");
        free(filename);
        response_error(s, client_addr, client_addr_len);
        return;
    }

    printf("PUT: received file (%d bytes): \"%s\"\n", len, filename);

    // write to file
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("fopen");
        free(filename);
        free(filedata);
        response_error(s, client_addr, client_addr_len);
        return;
    }

    int bytes = fwrite(filedata, 1, len, file);
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

static char* get_files_list() {
    struct stat st;
    if (stat(".", &st) != 0) {
        perror("stat");
        return NULL;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Not a directory\n");
        return NULL;
    }

    DIR* dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    char* files = NULL;
    size_t files_size = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", ".", entry->d_name);

        if (stat(path, &st) != 0) {
            perror("stat");
            closedir(dir);
            free(files);
            return NULL;
        }

        if (S_ISREG(st.st_mode)) {  // Only regular files
            size_t filename_len = strlen(entry->d_name);
            size_t new_size = files_size + filename_len + 1;  // +1 for newline character

            char* new_files = realloc(files, new_size * sizeof(char));
            if (new_files == NULL) {
                perror("realloc");
                free(files);
                closedir(dir);
                return NULL;
            }

            files = new_files;
            strncpy(files + files_size, entry->d_name, filename_len);

            files_size = new_size;
            files[files_size - 1] = '\n';
        }
    }
    closedir(dir);

    if (files == NULL) {
        files = strdup("No files found\n");
    }

    // add null terminator
    char* new_files = realloc(files, files_size + 1);
    if (new_files == NULL) {
        perror("realloc");
        free(files);
        return NULL;
    }
    files = new_files;
    files[files_size] = '\0';

    return files;
}

static void handle_ls(int s, sockaddr* client_addr, socklen_t* client_addr_len) {
    char* files = get_files_list();
    if (files == NULL) {
        response_error(s, client_addr, client_addr_len);
        return;
    }

    response_ok(s, client_addr, client_addr_len);
    if (send_data(s, files, strlen(files) + 1, client_addr, client_addr_len) == -1) {
        fprintf(stderr, "LS: failed to send ls data\n");
    }

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

        fflush(stdout);
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
            free(data);
            return;
        }

        free(data);
        clear_remaining_input(s);

        printf("waiting for command: ");
        fflush(stdout);
    }
}
