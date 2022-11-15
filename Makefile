CC	= gcc
CFLAGS	= 
LDFLAGS	= -pthread

all: client server

client: client.o
	$(CC) -lpthread -o client client.c list.c queue.c
server: server.o
	$(CC) -o server server.c list.c queue.c