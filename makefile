CC := gcc
CFLAGS := -Wall -Wextra -Werror -std=c11 -pedantic -g -Iinclude
# List of source files
SRC_FILES := src/my_udp.c

INCLUDES := $(wildcard include/*.h)

udp_server: src/udp_server.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o udp_server $(SRC_FILES) $<

udp_client: src/udp_client.c $(SRC_FILES) $(INCLUDES)
	$(CC) $(CFLAGS) -o udp_client $(SRC_FILES) $<

# Target: all
all: udp_server udp_client

# Target: clean
clean:
	rm -f udp_server udp_client

.PHONY: all clean