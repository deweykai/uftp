CC := gcc
CFLAGS := -Wall -Wextra -std=c2x -g -Iinclude -O2
# List of source files
SRC_FILES := src/my_udp.c src/my_ftp.c src/my_ftp_client.c src/my_repl.c

INCLUDES := $(wildcard include/*.h)

all: ftp_server ftp_client

ftp_server: src/uftp_server.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o ftp_server $(SRC_FILES) $<

ftp_client: src/uftp_client.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o ftp_client $(SRC_FILES) $<

clean:
	rm -f ftp_server ftp_client

.PHONY: all clean
