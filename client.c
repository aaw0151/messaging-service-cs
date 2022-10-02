#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define USERNAME_SIZE 17

int main(int argc, char* argv[])
{
	int sockfd, fdmax, i, portno, nread;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	fd_set master;
	fd_set read_fds;
	char send_buffer[BUFFER_SIZE];
	char recv_buffer[BUFFER_SIZE];
	char username[USERNAME_SIZE];

	if(argc != 3) //invalid parameters
	{
		fprintf(stderr, "usage: %s hostname port\n", argv[0]);
		exit(1);
	}

	printf("Enter your username (16 characters): ");
	memset(username, '\0', USERNAME_SIZE); //clearing buffer
	fgets(username, USERNAME_SIZE - 1, stdin);
	username[USERNAME_SIZE-1] = '\0'; //adding null terminator
	username[strchr(username, '\n') - &username[0]] = '\0'; //removing newline

	portno = atoi(argv[2]); //setting up port
	server = gethostbyname(argv[1]); //setting up hostname

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
	{
		perror("Socket");
		exit(1);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr)); //clearing server address
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);
	
	memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero));

	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) == -1) 
	{
		perror("connect");
		exit(1);
	}

	send(sockfd, username, strlen(username), 0); //sending username

	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(0, &master);
	FD_SET(sockfd, &master);
	fdmax = sockfd;

	while(1){
		read_fds = master;
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(4);
		}

		for(i=0; i <= fdmax; i++ )
		{
			if(FD_ISSET(i, &read_fds))
			{
				if (i == 0){
					fgets(send_buffer, BUFFER_SIZE, stdin);
					if (strcmp(send_buffer , "quit\n") == 0) {
						exit(0);
					} else {
						send(sockfd, send_buffer, strlen(send_buffer), 0);
					}
				} else {
					nread = recv(sockfd, recv_buffer, BUFFER_SIZE, 0);
					recv_buffer[nread] = '\0';
					printf("%s" , recv_buffer);
					fflush(stdout);
				}
			}
		}
	}
	printf("Disconnected\n");
	close(sockfd);
	return 0;
}
