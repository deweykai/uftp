CC := clang
CFLAGS := -Wall -Wextra -Werror -std=c17 -pedantic -g -Iinclude
# List of source files
SRC_FILES := src/my_udp.c

INCLUDES := $(wildcard include/*.h)

all: udp_server udp_client

udp_server: src/udp_server.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o udp_server $(SRC_FILES) $<

udp_client: src/udp_client.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o udp_client $(SRC_FILES) $<

clean:
	rm -f udp_server udp_client

.PHONY: all clean