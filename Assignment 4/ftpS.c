/*
Name: Sai Saketh Aluru
Roll No. 16CS30030
*/
// Standard Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
// Macro for Server's Sc port
#define PORT_X 50000
// Buffer sizes with and without header
#define BUFFLEN 1024
#define READLEN 1000
// Status codes for the commands
#define FIRST_NOT_PORT 503
#define PORT_RANGE_INVALID 550
#define PORT_OK 200
#define CD_ERROR 501
#define CD_OK 200
#define FILE_NOT_FOUND 550
#define GET_ERROR 550
#define GET_OK 250
#define PUT_ERROR 550
#define PUT_OK 250
#define QUIT_STATUS 421
#define INVALID_ARGUMENTS 501
#define UNKNOWN_COMMAND 502

// Function forward declarations
int parsePort(char* cmd_line,int* port_Y);
char* trimwhitespace(char *str);

// Function to find minimum of two numbers
int min(int a,int b)
{
	return a<b?a:b;
}

int main()
{
	int sockfd,clilen,i,newsfd,status,stat_net;
	int nbytes_recv,nbytes_send;
	struct sockaddr_in serv_addr,client_addr;
	char read_buff[READLEN];
	char* buff = (char*)calloc(sizeof(char),BUFFLEN);
	char* cmd_line = (char*)calloc(sizeof(char),BUFFLEN);
	// Create socket for Sc process
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<0){
		perror("Cannot create socket");
		exit(1);
	}
	// Enable the socket to reuse port - Needed for avoiding Address in use error
	int enable_port_reuse = 1;
	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&enable_port_reuse,sizeof(int))<0){
		perror("setsockopt(SO_REUSEADDR) failed");
	}
	// Fill in details of server socket address 
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT_X);
	// Bind server address to the socket
	if(bind(sockfd,(const struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
		perror("Bind failure");
		exit(1);
	}
	printf("Created FTP server's control server..\n");
	// Listen upto 5 client connections
	listen(sockfd,5);
	while(1){
		// Accept new clients connection
		clilen = sizeof(client_addr);
		newsfd = accept(sockfd,(struct sockaddr *)&client_addr,&clilen);
		if(newsfd < 0){
			perror("Accept error");
			exit(1);
		}
		printf("Created Control channel\n");
		printf("> ");
		fflush(stdout);
		// Receive the command from the client
		nbytes_recv =  recv(newsfd,cmd_line,BUFFLEN,0);
		if(nbytes_recv < 0){
			perror("Receive error");
			exit(1);
		}
		// If client closes its end of the socket unexpectedly
		else if(nbytes_recv==0){
			printf("Connection closed. Waiting for next client\n");
			continue;
		} 
		printf("Received command %s\n",cmd_line);
		int port_Y;
		// Check and parse the first command for the port command and the port value. 
		// The status variable indicates whether the port command is valid or not and also
		// sets the value of port_Y	with the given value if it is valid.
		status = parsePort(cmd_line,&port_Y);
		// Convert the status to network byte order and send it to the client
		stat_net = htonl(status);
		send(newsfd,&stat_net,sizeof(stat_net),0);
		printf("sending status %d\n",status);
		// If the status is an error code, close the client's connection and wait for next client
		if(status!=PORT_OK){
			printf("Port error, closing client connection\n");
			close(newsfd);
			continue;
		}
		printf("Set port to %d\n",port_Y);
		// Iteratively receive client's commands
		while(1){
			bzero(cmd_line,BUFFLEN);
			printf("> ");
			fflush(stdout);
			// Receive the command
			nbytes_recv=  recv(newsfd,cmd_line,BUFFLEN,0);
			// Error in receive
			if(nbytes_recv < 0){
				perror("Receive error");
				close(newsfd);
				break;
			}
			// If client terminates unexpectedly, drop the connection and go back to accept
			else if(nbytes_recv == 0){
				printf("Client connection closed. Waiting for next client\n");
				close(newsfd);
				break;
			}
			// Remove any leading and trailing spaces
			cmd_line = trimwhitespace(cmd_line);
			printf("Received command %s\n",cmd_line);
			// If the command entered is quit
			if(strcmp(cmd_line,"quit")==0){
				// convert and send the appropriate status to client 
				// in network byte order
				stat_net = htonl(QUIT_STATUS);
				printf("Sending quit status %d\n",QUIT_STATUS);
				send(newsfd,&stat_net,sizeof(stat_net),0);
				// close the connection after sending the status.
				close(newsfd);
				break;
			}
			// All other commands are expected to have arguments.
			// Hence take the first word as the command and the rest as arguments.
			char* cmd = strtok(cmd_line," ");
			// For cd argument
			if(strcmp(cmd,"cd")==0){
				// Get the directory
				char* dir = strtok(NULL," ");
				// If no argument is given after cd
				if(dir == NULL){
					stat_net = htonl(INVALID_ARGUMENTS);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue;
				}
				// If more than one directory is mentioned.
				if(strtok(NULL," ")!=NULL){
					stat_net = htonl(INVALID_ARGUMENTS);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue;
				}
				// If change directory doesn't work, send the error code
				if(chdir(dir)<0){
					perror("Unable to change directory");
					stat_net = htonl(CD_ERROR);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue;
				}
				// If cd is executed properly.
				else{
					stat_net = htonl(CD_OK);
					send(newsfd,&stat_net,sizeof(stat_net),0);
				}
			}
			// If the command is a get command
			else if(strcmp(cmd,"get")==0){
				// get the name of the file given as argument
				char* filename = strtok(NULL," ");
				// If no file is mentioned
				if(filename == NULL){
					status = INVALID_ARGUMENTS;
					stat_net = htonl(status);
					printf("Sending status %d\n",status);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue;	// wait for next command
				}
				// If more than one file 
				if(strtok(NULL," ")!=NULL){
					status = INVALID_ARGUMENTS;
					stat_net = htonl(status);
					printf("Sending status %d\n",status);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue;	// wait for next command
				}
				int filefd;
				// Check to see if file exists and is accessible
				// If error occurs while opening the file, send error code to client
				if((filefd=open(filename,O_RDONLY))<0){
					perror("Unable to open file");
					status = FILE_NOT_FOUND;
					stat_net = htonl(status);
					printf("Sending status %d\n",status);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue; 	// wait for next command
				}
				else{
					// If command is valid, fork and create the Sd data process
					pid_t pid = fork();
					// If fork fails
					if(pid<0){
						status = GET_ERROR;
						stat_net = htonl(status);
						send(newsfd,&stat_net,sizeof(stat_net),0);
						perror("Unable to create child process");
						exit(1);
					}
					// Child process - Data channel
					if(pid==0){
						int data_sfd,nbytes_read;
						struct sockaddr_in data_serv_addr;
						// Create data channel's socket
						data_sfd = socket(AF_INET,SOCK_STREAM,0);
						if(data_sfd<0){
							perror("Unable to create data socket");
							exit(1);
						}
						// Fill in information of client's Cd server.
						data_serv_addr.sin_family = AF_INET;
						inet_aton("127.0.0.1",&data_serv_addr.sin_addr);
						data_serv_addr.sin_port = htons(port_Y);
						// Sleeping for 0.1 millisecond to give time for client's Cd process to start listening
						usleep(100);
						// Connect to Cd
						if(connect(data_sfd,(struct sockaddr *)&data_serv_addr,sizeof(data_serv_addr))<0){
							perror("Unable to connect to data server");
							// close(data_sfd);
							exit(1);
						}
						// Read the data from the file
						bzero(buff,BUFFLEN);
						bzero(read_buff,READLEN);
						nbytes_read = read(filefd,read_buff,READLEN-1);
						// If read fails
						if(nbytes_read<0){
							perror("Read Error");
							close(data_sfd);
							close(filefd);
							exit(1);
						}
						// While there is data read
						while(nbytes_read>0){
							// Default header character is taken as a Z.
							buff[0] = 'Z';
							// Length of the data read from the file.
							unsigned short len = strlen(read_buff);
							// Add the length (as a 16 bit short int) to the header of the buffer
							memcpy(&buff[1],&len,sizeof(len));
							// Copy the data read from the file after the header
							memcpy(&buff[3],read_buff,sizeof(read_buff));
							// Read from the file again to see if anything else is left to be read
							bzero(read_buff,READLEN);
							nbytes_read = read(filefd,read_buff,READLEN-1);
							// If some error occurs during read.
							if(nbytes_read<0){
								perror("Error in data read from file");
								close(data_sfd);
								close(filefd);
								exit(1);
							}
							// If nothing is left in the file, it means that the current buffer is the last chunk
							// So we change the header character to 'L'
							else if(nbytes_read==0){
								buff[0] = 'L';
							}
							printf("Sending %s\n",buff);
							// Send the chunk to client. If any error during sending, exit the child process
							// after closing the sockets and file descriptors
							if((nbytes_send=send(data_sfd,buff,len+3,MSG_NOSIGNAL))<=0){
								perror("Error in sending data");
								close(data_sfd);
								close(filefd);
								exit(1);
							}
							// Clear the buffer for next iteration
							bzero(buff,BUFFLEN);
						}
						// After everything is sent, close the socket and file descriptor
						close(data_sfd);
						close(filefd);
						// Exit with status 0 if success. For any error we exit with status 1
						exit(0);
					}
					else{
						// Close the file opened as this is not needed in parent process
						close(filefd);
						// Make the parent wait till child terminates
						// child_stat is set by the exit call in child process
						int child_stat=0;
						waitpid(pid,&child_stat,0);
						// If child exits with some error, send the error code to client
						if(child_stat!=0)
							status = GET_ERROR;
						// Else send a success code
						else status = GET_OK;
						stat_net = htonl(status);
						printf("Sending status %d\n",status);
						send(newsfd,&stat_net,sizeof(stat_net),0);
					}
				}
			}
			// If the command is put
			else if(strcmp(cmd,"put")==0){
				char* filename = strtok(NULL," ");
				// If no file is mentioned
				if(filename == NULL){
					status = INVALID_ARGUMENTS;
					stat_net = htonl(status);
					printf("Sending status %d\n",status);
					send(newsfd,&stat_net,sizeof(stat_net),0);

				}
				// If more than one file is mentioned
				if(strtok(NULL," ")!=NULL){
					status = INVALID_ARGUMENTS;
					stat_net = htonl(status);
					printf("Sending status %d\n",status);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue;
				}
				int filefd;
				// Open the file. Create a new one if there is no such file
				filefd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
				// If file opening fails due to some reason, send error code to client and wait for next command
				if(filefd < 0){
					perror("Unable to open file");
					status = PUT_ERROR;
					stat_net = htonl(status);
					printf("Sending status %d\n",status);
					send(newsfd,&stat_net,sizeof(stat_net),0);
					continue;
				}
				else{
					// Fork to create child process
					pid_t pid = fork();
					// If fork fails
					if(pid<0){
						perror("Error in creating child process");
						status = PUT_ERROR;
						stat_net = htonl(status);
						send(newsfd,&stat_net,sizeof(stat_net),0);
						exit(1);
					}
					// Child process - data channel
					else if(pid==0){
						int data_sfd;
						struct sockaddr_in data_serv_addr;
						data_sfd = socket(AF_INET,SOCK_STREAM,0);
						if(data_sfd<0){
							perror("Unable to create data socket");
							exit(1);
						}
						// Fill in details of Cd process's server
						data_serv_addr.sin_family = AF_INET;
						inet_aton("127.0.0.1",&data_serv_addr.sin_addr);
						data_serv_addr.sin_port = htons(port_Y);
						// Put the process to sleep to give time for Cd server to start listening
						usleep(1);
						// Connect to the server
						if(connect(data_sfd,(struct sockaddr *)&data_serv_addr,sizeof(data_serv_addr))<0){
							perror("Unable to connect to data server");
							close(data_sfd);
							close(filefd);
							exit(1);
						}
						printf("Connected to client's data channel\n");
						// Receive data from the client
						bzero(buff,BUFFLEN);
						bzero(read_buff,READLEN);
						nbytes_recv = recv(data_sfd,buff,BUFFLEN,0);
						printf("Received %s\n",buff);
						// If receive fails
						if(nbytes_recv<0){
							close(data_sfd);
							close(filefd);
							perror("Receive error");
							exit(1);
						}
						// If it is a blank message or socket got closed
						else if(nbytes_recv==0){
							perror("Connection closed");
							close(filefd);
							close(data_sfd);
							exit(1);
						}
						else{
							int j=0; 	// location of the start of a new chunk
							unsigned short bytes_left;	// size of the chunk to be written in each iteration
							char indicator;	// The first character sent in each header
							int new_block=1;	// flag to indicate new block
							while(nbytes_recv>0){
								if(new_block){
									// get the new chunk's header character
									indicator = buff[j];
									// copy the value of the chunk size sent in the header
									memcpy(&bytes_left,&buff[j+1],sizeof(bytes_left));
									// move the buffer head pointer to after the header
									buff = &buff[j+3];
									// Reduce the buffer size to ignore header characters and the characters
									// that have already been written to the file
									nbytes_recv = nbytes_recv -j -3;
									while(bytes_left>0){
										// Write to file either till the end of the chunk or till the 
										// end of the buffer received, whichever is smaller
										int written_bytes = write(filefd,buff,min(bytes_left,nbytes_recv));
										// If write fails, exit 
										if(written_bytes<0){
											close(data_sfd);
											close(filefd);
											perror("Write error");
											exit(1);
										}
										// If the chunk got completed before buffer 
										// i.e the chunk size is smaller than received data
										// implies start of new chunk. Adjust the value of j to indicate
										// to this start of the chunk and set the new_block flag
										if(bytes_left<nbytes_recv){
											j = bytes_left;
											bytes_left = 0;
											new_block =1;
										}
										// If the chunk is fully written as well as buffer is exhausted.
										// May happen anytime in between. Will be the case for sure 
										// when the last chunk is fully written
										else if(bytes_left==nbytes_recv){
											j = 0;	// set the new header location to start of next buffer
											new_block = 1;	// set the new_block flag as this chunk is done.
											// Check the indicator character. If it is 'L', last block is written
											// So we are done with writing everything and can break the loop
											if(indicator=='L'){
												nbytes_recv=0;
												break;
											}
											// If it is not the last block, receive the next buffer of data
											bzero(buff,BUFFLEN);
											bytes_left = 0;
											nbytes_recv = recv(data_sfd,buff,BUFFLEN,0);
										}
										// If the buffer is exhausted but chunk is yet to be written fully,
										// implies the next buffer will start with the continuition of this chunk
										else{
											// Adjust the chunk size left
											bytes_left -= nbytes_recv;
											// and get the new buffer data from the client
											bzero(buff,BUFFLEN);
											nbytes_recv = recv(data_sfd,buff,BUFFLEN,0);
										}
									}
								}
							}
							// After writing is done, close the socket and file descriptor
							close(filefd);
							close(data_sfd);
							// Exit with status 0. For any error, exit status will be 1
							exit(0);
						}
					}
					// Control channel process
					else{
						// Close the file descriptor as it is not needed in control channel
						close(filefd);
						// Wait for the data channel to terminate. child_stat is set during the child's
						// exit call and can be monitored to see the status
						int child_stat;
						waitpid(pid,&child_stat,0);
						// For non zero status, send error code. Else send the success code
						if(child_stat!=0){
							status = PUT_ERROR;
							remove(filename);
						}
						else status = PUT_OK;
						// Convert the status to network byte order before sending
						stat_net = htonl(status);
						send(newsfd,&stat_net,sizeof(stat_net),0);
						printf("Sending status %d\n",status);
					}
				}
			}
			// If the command is none of the above, 
			// Send the status code for the same
			else{
				int status = UNKNOWN_COMMAND;
				stat_net = htonl(status);
				send(newsfd,&stat_net,sizeof(stat_net),0);
				printf("Sending status %d\n",status);
			}
		}

	}
}
/*
	Function to parse the first command for a port command
*/
int parsePort(char* cmd_line,int* port_Y)
{
	// Get the name of the command
	char *token = strtok(cmd_line," ");
	// If it is not a port command
	if(strcmp(token,"port")!=0)
		return FIRST_NOT_PORT;
	// Get the argument after port	
	token = strtok(NULL," ");
	// If no argument is specified
	if(token==NULL){
		return INVALID_ARGUMENTS; 
	}
	// Convert the value from ascii to integer
	int port = atoi(token);
	// Check the value of port to be in the specified range
	if(port < 1024 || port > 65536)
		return PORT_RANGE_INVALID;
	token = strtok(NULL," ");
	// Check if there are any other arguments given. If nothing are given, it is a valid command
	// Set the port accordingly and return the status
	if(token==NULL){
		*port_Y = port;
		return PORT_OK;
	}
	// If more than one argument is mentioned
	else return INVALID_ARGUMENTS; 
}
/*
	Function to trim the leading and trailing spaces of a string
*/
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  // Move the head pointer to first non space character, essentially ignoring all spaces before
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}