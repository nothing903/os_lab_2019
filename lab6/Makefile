CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
LDFLAGS = -lws2_32

all: client server

client: src/client.c common.c
	$(CC) $(CFLAGS) src/client.c common.c -o client $(LDFLAGS)

server: src/server.c common.c
	$(CC) $(CFLAGS) src/server.c common.c -o server $(LDFLAGS)

clean:
	rm -f client server

.PHONY: all clean