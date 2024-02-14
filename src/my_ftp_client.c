/**
 * @file my_ftp_client.c
 * @author Kai Dewey
 */

#include "my_ftp_client.h"

char* ftp_get(int s, char* filename, int* len) {
    // send command
    ftp_command cmd = GET;
    if (send_data(s, (char*)&cmd, sizeof(GET), NULL, NULL) == -1) {
        fprintf(stderr, "GET: failed to send command\n");
        return NULL;
    }

    // send filename
    if (send_data(s, filename, strlen(filename) + 1, NULL, NULL) == -1) {
        fprintf(stderr, "GET: failed to send filename\n");
        return NULL;
    }

    // get status
    ftp_response* status = (ftp_response*)recv_data(s, NULL, NULL, NULL);
    if (status == NULL) {
        fprintf(stderr, "GET: No response\n");
        return NULL;
    }

    if (*status == ERROR) {
        fprintf(stderr, "GET: server failed to get file: \"%s\"\n", filename);
        free(status);
        return NULL;
    }
    free(status);

    // get data
    char* data = recv_data(s, len, NULL, NULL);
    if (data == NULL) {
        fprintf(stderr, "GET: failed to receive data\n");
        return NULL;
    }
    return data;
}

void ftp_put(int s, char* filename, char* filedata, int filedata_len) {
    // send command
    ftp_command cmd = PUT;
    if (send_data(s, (char*)&cmd, sizeof(cmd), NULL, NULL) == -1) {
        fprintf(stderr, "PUT: failed to send command\n");
        return;
    }

    // send filename
    if (send_data(s, filename, strlen(filename) + 1, NULL, NULL) == -1) {
        fprintf(stderr, "PUT: failed to send filename\n");
        return;
    }

    // send filedata
    if (send_data(s, filedata, filedata_len, NULL, NULL) == -1) {
        fprintf(stderr, "PUT: failed to send filedata\n");
        return;
    }

    // status
    ftp_response* status = (ftp_response*)recv_data(s, NULL, NULL, NULL);
    if (status == NULL) {
        fprintf(stderr, "No response\n");
        return;
    }
    if (*status == ERROR) {
        fprintf(stderr, "PUT: server failed to put file: %s\n", filename);
        free(status);
        return;
    }

    printf("put file: %s\n", filename);
    free(status);
}

void ftp_delete(int s, char* filename) {
    // command
    ftp_command cmd = DELETE;
    if (send_data(s, (char*)&cmd, sizeof(cmd), NULL, NULL) == -1) {
        fprintf(stderr, "DELETE: failed to send command\n");
        return;
    }

    // filename
    if (send_data(s, filename, strlen(filename) + 1, NULL, NULL) == -1) {
        fprintf(stderr, "DELETE: failed to send filename\n");
        return;
    }

    // response
    ftp_response* status = (ftp_response*)recv_data(s, NULL, NULL, NULL);
    if (status == NULL) {
        fprintf(stderr, "DELETE: No response\n");
        return;
    }
    if (*status == ERROR) {
        fprintf(stderr, "DELETE: server failed to delete file\n");
        free(status);
        return;
    }


    printf("deleted file: %s\n", filename);
    free(status);
}

char* ftp_ls(int s) {
    // send command
    ftp_command cmd = LS;
    if (send_data(s, (char*)&cmd, sizeof(cmd), NULL, NULL) == -1) {
        fprintf(stderr, "LS: failed to send command\n");
        return NULL;
    }

    // receive status
    ftp_response* status = (ftp_response*)recv_data(s, NULL, NULL, NULL);
    if (status == NULL) {
        fprintf(stderr, "LS: No response\n");
        return NULL;
    }
    if (*status == ERROR) {
        printf("failed to ls\n");
        free(status);
        return NULL;
    }
    free(status);

    // receive data
    char* data = recv_data(s, NULL, NULL, NULL);
    if (data == NULL) {
        fprintf(stderr, "LS: failed to receive data\n");
        return NULL;
    }

    return data;
}

void ftp_exit(int s) {
    // send command
    ftp_command cmd = EXIT;
    if (send_data(s, (char*)&cmd, sizeof(cmd), NULL, NULL) == -1) {
        fprintf(stderr, "EXIT: failed to send command\n");
        return;
    }

    // receive status
    ftp_response* status = (ftp_response*)recv_data(s, NULL, NULL, NULL);
    if (status == NULL) {
        fprintf(stderr, "EXIT: No response\n");
        return;
    }
    if (*status == ERROR) {
        printf("EXIT: server failed to exit\n");
        free(status);
        return;
    }
    free(status);
}
