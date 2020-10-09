all : server client

server : server.o
	gcc server.o -o server

client : client.o
	gcc client.o -o client

server.o : server.c
	gcc server.c -c

client.o : client.c
	gcc client.c -c


clean :
	rm *.o server client
