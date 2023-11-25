all: server client
CFLAGS=-Wall -Wextra -pedantic -std=c17 -D_XOPEN_SOURCE=700

server: server.c
	$(CC) server.c -o server $(CFLAGS)

client: client.c
	$(CC) client.c -o client $(CFLAGS)

clean:
	rm -f server
	rm -f client
