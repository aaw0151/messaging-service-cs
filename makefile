CC = gcc
CFLAGS = -Wall -g
LIBS =


all : server client

server : server.o
	${CC} ${CFLAGS} server.o -o server ${LIBS}

client : client.o
	${CC} ${CFLAGS} client.o -o client ${LIBS}

server.o : server.c
	${CC} ${CFLAGS} server.c -c ${LIBS}

client.o : client.c
	${CC} ${CFLAGS} client.c -c ${LIBS}


clean :
	rm server.o client.o server client
