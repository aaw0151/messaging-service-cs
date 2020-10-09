#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define CHATLOG_SIZE 10
#define USERS 5
#define USERNAME_SIZE 17

char** ReadWordBlacklist(FILE* fp, char** arr, int* count, int* cap)
{
	char buffer[BUFFER_SIZE + 1];
	int i;

	*count = 0;
	*cap = 2;
	for(i = 0; i < *cap; i++)
		if((arr[i] = malloc(BUFFER_SIZE * sizeof(char))) == NULL) //allocating initial memory
		{
			perror("word blacklist malloc #1");
			exit(1);
		}

	if((fp = fopen("messaging_word_blacklist.txt", "r")) == NULL)
	{
		perror("word blacklist read");
		exit(1);
	}
	else
	{
		fgets(buffer, BUFFER_SIZE, fp);
		while(strcmp(buffer, "\0") != 0)
		{
			if(*count == *cap - 1)
			{
				*cap *= 2; //increasing the capacity by 2 times
				if((arr = realloc(arr, *cap * sizeof(char*))) == NULL)
				{
					perror("word blacklist realloc");
					exit(1);
				}
				else
				{
					for(i = *cap - 1; i >= *count; i--) //adding space for newly added spaces
					{
						if((arr[i] = malloc(BUFFER_SIZE * sizeof(char))) == NULL)
						{
							perror("word blacklist malloc #2");
							exit(1);
						}
					}
				}
			}
			*strchr(buffer, '\n') = '\0'; //removing newline character from buffer
			strncpy(arr[*count], buffer, BUFFER_SIZE - 1);
			*count = *count + 1;
			memset(buffer, '\0', BUFFER_SIZE);
			fgets(buffer, BUFFER_SIZE, fp);
		}
	}

	fclose(fp);
	printf("Blacklisted words loaded.\n");

	return arr;
}

int main(int argc, char* argv[])
{
	fd_set master; //set of master file descs
	fd_set read_fds; //set of readable file descs
	int fdmax, i, j, k, m, n, on = 1, portno, newsockfd; //int for max file desc, loop vars (i, j, k), for reuse addr, port number, and incoming socket file desc
	int sockfd= 0; //initial socket file desc
	struct sockaddr_in my_addr, client_addr; //server address and client address
	socklen_t addrlen; //length of address
	int  nread; //int for byte read amount
	char buffer[BUFFER_SIZE + 1]; //char array for input buffer from clients
	char chatlog[CHATLOG_SIZE][USERNAME_SIZE + BUFFER_SIZE + 2]; //char 2d array to save chat log of clients
	int  chatlogcount = 0; //int for counter for chat log
	int  ischatsaved = -1; //boolean for saving chat log
	char usernames[USERS][USERNAME_SIZE]; //char 2d array for usernames of clients
	char temp[USERNAME_SIZE + BUFFER_SIZE + 2]; //char array for output for clients (for formatting)
	char* chrptr; //char pointer used for word blacklist
	int  filedescs[USERS]; //int array for current file descriptors alive
	char** wordblacklist = malloc(2 * sizeof(char*)); //double pointer to list of blacklisted words
	int wordblacklistcount; //counter for the list of blacklisted words
	int wordblacklistcap; //capacity for the list of blacklisted words
	FILE* fp; //file pointer for blockable words and chatlog


	for(i = 0; i < USERS; i++) //intializing file descriptor array
		filedescs[i] = -1;
	for(i = 0; i < USERS; i++) //initializing username array
		memset(usernames[i], '\0', USERNAME_SIZE);
	for(i = 0; i < CHATLOG_SIZE; i++) //initializing chatlog array
		memset(chatlog[i], '\0', USERNAME_SIZE + BUFFER_SIZE + 2);

	if(argc != 2) //checking for port number
	{
		fprintf(stderr, "usage: %s port\n", argv[0]);
		exit(1);
	}

	/*
	 *  READING BLACKLISTED WORDS
	 */

	wordblacklist = ReadWordBlacklist(fp, wordblacklist, &wordblacklistcount, &wordblacklistcap);

	/*
	 *  STARTING SOCKET
	 */

	portno = atoi(argv[1]); //assigning port number from command line args

	FD_ZERO(&master);   //initializing FD sets
	FD_ZERO(&read_fds); //

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { //opening socket
		perror("Socket");
		exit(1);
	}
	printf("Socket opened.\n");

	my_addr.sin_family = AF_INET; //setting up IP, port, and connection type information
	my_addr.sin_port = htons(portno);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) { //setting reuse address opt
		perror("setsockopt");
		exit(1);
	}

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) { //binding socket
		perror("Unable to bind");
		exit(1);
	}
	if (listen(sockfd, 10) == -1) { //listening for connections on socket
		perror("listen");
		exit(1);
	}
	printf("Waiting for clients...\n");
	fflush(stdout);

	FD_SET(sockfd, &master); //creating initial FD set
	FD_SET(0, &master); //adding stdin

	fdmax = sockfd; //setting max file descriptor
	while(1){
		read_fds = master;
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(4);
		}

		/*
		 *  SERVER COMMANDS
		 */

		if(FD_ISSET(0, &read_fds)) //reading stdin
		{
			memset(buffer, '\0', BUFFER_SIZE);
			if((nread = read(0, buffer, BUFFER_SIZE)) > 0)
			{
				/*
				 *  KICK COMMAND
				 */

				if(strncmp(buffer, "/kick ", 6) == 0)
				{
					if((!isspace(buffer[6])) && (!isspace(buffer[strlen(buffer) - 2])))
					{
						for(i = 0; i < USERS; i++)
						{
							//printf("test=%.*s\n", strlen(usernames[i]), &buffer[6]);
							if((strcmp(usernames[i], "\0") != 0) && (strncmp(usernames[i], &buffer[6], strlen(usernames[i])) == 0) && (strlen(&buffer[6]) - 1 == strlen(usernames[i]))) //found user
							{
								printf("Kicking user \"%s\".\n", usernames[i]);
								fflush(stdout);
								getpeername(filedescs[i], (struct sockaddr*) &client_addr, (socklen_t*) &addrlen);
								printf("Client disconnected, %s:%d, (kicked)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
								fflush(stdout);
								close(filedescs[i]);
								FD_CLR(filedescs[i], &master);
								memset(temp, '\0', USERNAME_SIZE + BUFFER_SIZE + 2);
								sprintf(temp, "%s has connected.\n", usernames[i]);
								filedescs[i] = -1;
								for(k = 0; k < USERS; k++) //looping through all possible connections
								{
									if(filedescs[k] != -1) //sending to alive connections
									{
										send(filedescs[k], temp, strlen(temp), 0);
									}
								}
								memset(usernames[i], '\0', USERNAME_SIZE);
								break;
							}
							if(i == USERS - 1) //user never found
							{
								printf("User \"%.*s\" is not online.\n" , strlen(buffer) - 7, &buffer[6]);
								fflush(stdout);
								break;
							}
						}
					}
					else
					{
						printf("\tusage: /kick <username>\n");
					}
				}

				/*
				 *  WORD BLACKLIST COMMANDS
				 */

				else if(strncmp(buffer, "/wordblacklist", 14) == 0)
				{
					if(strncmp(buffer, "/wordblacklist -r", 17) == 0) //reload flag
					{
						for(i = 0; i < wordblacklistcount; i++) //deallocating memory
						{
							free(wordblacklist[i]);
						}
						free(wordblacklist); //
						wordblacklist = ReadWordBlacklist(fp, wordblacklist, &wordblacklistcount, &wordblacklistcap); //resetting wordblacklist
					}
					else if(strncmp(buffer, "/wordblacklist -p", 17) == 0) //print flag
					{
						printf("\tWord blacklist:\n");
						for(i = 0; i < wordblacklistcount; i++)
						{
							printf("\t%-3d: %s\n", i + 1, wordblacklist[i]);
						}
					}
					else
					{
						printf("\tusage: /wordblacklist <flag>\n\t       -r Reload blacklist\n\t       -p Print blacklist\n");
					}
				}
				memset(buffer, '\0', BUFFER_SIZE);
			}
			else
			{
				perror("stdin read");
			}
		}

		/*
		 *  CLIENT INPUT/OUTPUT
		 */

		for (i = 1; i <= fdmax; i++){ //looping through all file descriptors
			if (FD_ISSET(i, &read_fds)){ //if a message has been sent by a client
				if (i == sockfd)
				{
					addrlen = sizeof(struct sockaddr_in);
					if((newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &addrlen)) == -1) { //accepting new client
						perror("accept");
						exit(1);
					}else {
						FD_SET(newsockfd, &master); //adding file descriptor to master set
						if(newsockfd > fdmax){ //finding new file descriptor max
							fdmax = newsockfd;
						}
						printf("Client connected, %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); //outputting client information to server
						if(!(nread = recv(newsockfd, buffer, BUFFER_SIZE, 0)) <= 0) //if the current message can be read (username mesage) (initial)
						{
							buffer[nread] = '\0';
							for(j = 0; j < USERS; j++)
							{
								/*
								 *  INITIALIZING USERNAMES TO CLIENTS AND HANDLING INITIAL DISCONNECTS
								 */

								if(filedescs[j] == -1) //assigning new username and file descriptior
								{
									strcpy(usernames[j], buffer);
									filedescs[j] = newsockfd;
									printf(", named \"%s\"\n", usernames[j]);
									fflush(stdout);
									break;
								}
								else if(strcmp(usernames[j], buffer) == 0) //check if the username is taken
								{
									printf("\n");
									if(send(newsockfd, "Sorry, that username is taken. Try again later...\n", 51, 0) == -1) //sending disconnect message
									{
										perror("send username taken");
									}
									getpeername(newsockfd, (struct sockaddr*) &client_addr, (socklen_t*) &addrlen);
									printf("Client disconnected, %s:%d, (username taken)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
									close(newsockfd); //removing current client
									FD_CLR(newsockfd, &master);
									break;
								}
								if(j == USERS - 1) //check if the server is currently full
								{
									printf("\n");
									if(send(newsockfd, "Sorry, chat room is currently full. Try again later...\n", 56, 0) == -1) //sending disconnect message
									{
										perror("send full disconnect");
									}
									getpeername(newsockfd, (struct sockaddr*) &client_addr, (socklen_t*) &addrlen);
									printf("Client disconnected, %s:%d, (server full)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
									close(newsockfd); //removing current client
									FD_CLR(newsockfd, &master);
									break;
								}
							}
						}
					}
				}
				else
				{
					memset(buffer, '\0', BUFFER_SIZE); //clearing buffer
					if ((nread = recv(i, buffer, BUFFER_SIZE, 0)) <= 0) //checking if client disconneted
					{
						/*
						 *  CLIENT DISCONNECTS
						 */

						if(nread == 0) 
						{
							getpeername(i, (struct sockaddr*) &client_addr, (socklen_t*) &addrlen);
							printf("Client disconnected, %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
							fflush(stdout);
							for(j = 0; j < USERS; j++) //removing client from usernames and filedescs, notifying other clients of the disconnection
							{
								if(filedescs[j] == i) //if file desc is the disconnected one
								{
									filedescs[j] = -1; //resetting file descriptor in array
									memset(temp, '\0', USERNAME_SIZE + BUFFER_SIZE + 2); //clearing buffer
									sprintf(temp, "%s has disconnected.\n", usernames[j]); //formating disconnect message to temp
									for(k = 0; k < USERS; k++) //looping through all possible connections
									{
										if(filedescs[k] != -1) //sending to alive connections
										{
											send(filedescs[k], temp, strlen(temp), 0);
										}
									}
									memset(usernames[j], '\0', USERNAME_SIZE); //clearing username of disconnected client
								}
							}
						}
						else 
						{
							perror("recv");
						}
						close(i); //removing current client
						FD_CLR(i, &master);
					}
					else 
					{
						/*
						 *  ECHOING MESSAGE TO OTHER CLIENTS
						 */

						for(k = 0; k < USERS; k++) //appending message to the end of the user's username
						{
							if(filedescs[k] == i)
							{
								memset(temp, '\0', USERNAME_SIZE + BUFFER_SIZE + 2); //clearing buffer
								sprintf(temp, "%s> %s", usernames[k], buffer);
								break;
							}
						}
						printf("%s", temp); //printing current message to server
						fflush(stdout);
						ischatsaved = 0; //resetting boolean for if the current chat has been saved
						for(j = 1; j <= fdmax; j++)
						{
							if (FD_ISSET(j, &master))
							{
								if (j != sockfd && j != i)
								{
									for(m = 0; m < wordblacklistcount; m++)
									{
										while((chrptr = strstr(temp, wordblacklist[m])) != NULL)
										{
											for(n = 0; n < strlen(wordblacklist[m]); n++)
											{
												temp[chrptr - &temp[0] + n] = '*';
											}
										}
									}
									if(send(j, temp, strlen(temp), 0) == -1) //sending message to all clients except the sender
									{
										perror("sendtoclients");
									}
									else //message sent correctly
									{
										/*
										 *  CHATLOG SAVING
										 */
										if(ischatsaved == 0) //checking if the chat has been saved already
										{
											strncpy(chatlog[chatlogcount], temp, BUFFER_SIZE + USERNAME_SIZE + 2); //adding current message to chatlog
											chatlogcount++; //increasing chatlog counter
											if(chatlogcount == CHATLOG_SIZE)
											{
												if((fp = fopen("messaging_log.txt", "a+")) == NULL) //opening file
												{
													perror("chatlog open");
												}
												for(k = 0; k < CHATLOG_SIZE; k++) //printing chat log of the last CHATLOG_SIZE messages
												{
													fprintf(fp, "%s", chatlog[k]);
												}
												chatlogcount = 0; //resetting counter
												if(fclose(fp) != 0) //closing file pointer
												{
													perror("chatlog close");
												}
												printf("Chat saved.\n");
												fflush(stdout);
											}
											ischatsaved = 1; //updating if the current chat was saved or not
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	for(i = 0; i < wordblacklistcap; i++)
	{
		free(wordblacklist[i]);
	}
	free(wordblacklist);

	return 0;
}
