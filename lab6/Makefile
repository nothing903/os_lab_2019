CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread

all: client server

client: src/client.c
	$(CC) $(CFLAGS) src/client.c -o client -lws2_32

server: src/server.c
	$(CC) $(CFLAGS) src/server.c -o server -lws2_32

clean:
	rm -f client server

.PHONY: all clean