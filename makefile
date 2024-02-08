CC := clang
CFLAGS := -Wall -Wextra -Werror -std=c17 -pedantic -g -Iinclude
# List of source files
SRC_FILES := src/my_udp.c src/my_ftp.c src/my_ftp_client.c src/my_repl.c

INCLUDES := $(wildcard include/*.h)

all: ftp_server ftp_client

ftp_server: src/ftp_server.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o ftp_server $(SRC_FILES) $<

ftp_client: src/ftp_client.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o ftp_client $(SRC_FILES) $<

clean:
	rm -f ftp_server ftp_client

.PHONY: all clean