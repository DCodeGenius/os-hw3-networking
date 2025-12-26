CC=gcc
CFLAGS=-Wall -Wextra -g
LDFLAGS=

SERVER_OBJS=hw3server.o server_clients.o
CLIENT_OBJS=hw3client.o

.PHONY: all clean

all: hw3server hw3client

hw3server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(LDFLAGS)

hw3client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o hw3server hw3client
