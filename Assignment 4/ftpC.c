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

// Function forward declaration
char *trimwhitespace(char *str);

// Function to find minimum of two numbers
int min(int a, int b)
{
	return a<b?a:b;
}

int main()
{
	int sockfd,status,stat_net,port_Y;
	struct sockaddr_in serv_addr;
	char *cmd_line = (char*)calloc(sizeof(char),BUFFLEN);
	char *buff = (char*)calloc(sizeof(char),BUFFLEN);
	char read_buff[READLEN];
	// Create socket for Cc process
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<0){
		perror("Unable to create socket");
		exit(1);
	}
	// Fill in details of server socket address 
	serv_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1",&serv_addr.sin_addr);
	serv_addr.sin_port = htons(PORT_X);
	// Connect to server
	if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
		perror("Unable to connect");
		exit(1);
	}
	printf("Created client\n");
	bzero(cmd_line,BUFFLEN);
	// Print the prompt and wait for user input
	printf(">  ");
	fflush(stdout);
	fgets(cmd_line,BUFFLEN,stdin);
	// Remove the \n at the end of the command obtained through fgets
	cmd_line[strlen(cmd_line)-1] = '\0';
	printf("Entered command %s\n",cmd_line);
	// Send the \0 terminated string to server
	if(send(sockfd,cmd_line,strlen(cmd_line)+1,0)<0){
		perror("Unable to send");
		exit(1);
	}
	cmd_line = trimwhitespace(cmd_line);
	// Get the name of the command entered
	char* cmd = strtok(cmd_line," ");	
	// Recieve the status code of the first command (expected to be port)
	// from the server
	recv(sockfd,&stat_net,sizeof(stat_net),0);
	status = ntohl(stat_net);
	// Print the appropriate message for each code
	if(status == PORT_OK){
		printf("Received status %d, Port command sent. Success\n",status);
	}
	else if(status == PORT_RANGE_INVALID){
		printf("Received status %d, Port should be betwenn 1024 and 65536 only\n",status);
		close(sockfd);
		exit(0);
	}
	else if(status == FIRST_NOT_PORT){
		printf("Received status %d, First command should be a port command\n",status);
		close(sockfd);
		exit(0);
	}
	else if(status == INVALID_ARGUMENTS){
		printf("Received status %d, Invalid arguments given\n",status);
		close(sockfd);
		exit(0);
	}
	// For a successful port command, get the argument given after port
	// and set the value of port_Y to that
	char* token = strtok(NULL," ");
	port_Y = atoi(token);
	// Iteratively take user inputs
	while(1){
		status = 0;
		bzero(cmd_line,BUFFLEN);
		printf(">  ");
		fflush(stdout);
		fgets(cmd_line,BUFFLEN,stdin);
		cmd_line[strlen(cmd_line)-1] = '\0';
		printf("entered command %s\n",cmd_line);
		// Send the command to the server
		if(send(sockfd,cmd_line,strlen(cmd_line)+1,0)<0){
			perror("Unable to send");
			exit(1);
		}
		// Get the name of the command
		char* cmd = strtok(cmd_line," ");
		// cd command
		if(strcmp(cmd,"cd")==0){
			// Receive the status code reply
			if(recv(sockfd,&stat_net,sizeof(stat_net),0)<0){
				perror("Receive error");
				exit(1);
			}
			status = ntohl(stat_net);
			// Print the status and appropriate message
			printf("Received status %d\n",status);
			if(status==CD_OK){
				printf("CD successful\n");
			}
			else printf("Error in cd command\n");
		}
		// If it is a get command
		else if(strcmp(cmd,"get")==0){
			// Get the name of the file
			char* token = strtok(NULL," ");
			char* filename = "";
			// If the given file name is a path, ignore the directories
			// and get the file name at the end.
			token = strtok(token,"/");
			while(token!= NULL){
				filename = token;
				token = strtok(NULL,"/");
			}
			// Fork and create child process
			pid_t pid = fork();
			if(pid<0){
				perror("Unable to fork");
				exit(1);
			}
			// Child process - data channel
			else if(pid==0){
				int data_sfd,data_clilen,newsfd;
				struct sockaddr_in data_serv_addr,data_cli_addr;
				// Create the socket
				data_sfd = socket(AF_INET,SOCK_STREAM,0);
				if(data_sfd<0){
					perror("Unable to create socket");
					exit(1);
				}
				// Enable the socket to reuse port - Needed for avoiding Address in use error
				int enable_port_reuse = 1;
				if(setsockopt(data_sfd,SOL_SOCKET,SO_REUSEADDR,&enable_port_reuse,sizeof(int))<0){
					perror("setsockopt(SO_REUSEADDR) failed");
				}
				// Fill in information of client's Cd server.
				data_serv_addr.sin_family = AF_INET;
				data_serv_addr.sin_addr.s_addr = INADDR_ANY;
				data_serv_addr.sin_port = htons(port_Y);
				// Bind the socket to the port
				if(bind(data_sfd,(const struct sockaddr *)&data_serv_addr,sizeof(data_serv_addr))<0){
					perror("Bind failure in data channel");
					strcpy(filename,"");
					close(data_sfd);
					exit(1);
				}
				// Listen for clients and accept connection
				listen(data_sfd,5);
				newsfd = accept(data_sfd,(struct sockaddr *)&data_cli_addr,&data_clilen);
				if(newsfd<0){
					perror("Accept error");
					exit(1);
				}
				int filefd;
				printf("Opening filename %s\n",filename);
				if(filename == NULL){
					close(newsfd);
					close(filefd);
					close(data_sfd);
					exit(1);
				}
				// Open the file. Create if it is not present
				if(filename!=NULL)
					filefd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
				bzero(buff,BUFFLEN);
				bzero(read_buff,READLEN);
				// Receive the data from the server
				int nbytes_recv = recv(newsfd,buff,BUFFLEN,0);
				// If nothing is received or blank file, exit
				if(nbytes_recv==0){
					close(filefd);
					close(newsfd);
					close(data_sfd);
					exit(0);
				}
				int j=0;	// location of the start of a new chunk
				unsigned short bytes_left; 	// size of the chunk to be written in each iteration
				char indicator;		// The first character sent in each header
				int new_block = 1;	// flag to indicate new block
				// If recv returns 0, implies server closed the socket
				// Transfer complete
				while(nbytes_recv>0){
					if(new_block){
						// get the new chunk's header character
						indicator = buff[j];
						// copy the value of the chunk size sent in the header
						memcpy(&bytes_left,&buff[j+1],sizeof(bytes_left));
						// move the buffer head pointer to after the header
						// Reduce the buffer size to ignore header characters and the characters
						// that have already been written to the file
						if(nbytes_recv > j+3){
							buff = &buff[j+3];
							nbytes_recv = nbytes_recv -j-3;
						}
						else{
							bzero(buff,BUFFLEN);
							nbytes_recv = recv(newsfd,buff,BUFFLEN,0);
							if(nbytes_recv <=0 )
								break;
						}
						while(bytes_left>0){
							// Write to file either till the end of the chunk or till the 
							// end of the buffer received, whichever is smaller
							int written_bytes = write(filefd,buff,min(bytes_left,nbytes_recv));
							printf("writing %s\n",buff);
							// If write fails, exit 
							if(written_bytes<0){
								close(newsfd);
								close(filefd);
								close(data_sfd);
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
								new_block = 1;
							}
							// If the chunk is fully written as well as buffer is exhausted.
							// May happen anytime in between. Will be the case for sure 
							// when the last chunk is fully written
							else if(bytes_left==nbytes_recv){
								j = 0;	// set the new header location to start of next buffer
								new_block = 1;	// set the new_block flag as this chunk is done.
								bzero(buff,BUFFLEN);
								// Check the indicator character. If it is 'L', last block is written
								// So we are done with writing everything and can break the loop
								if(indicator=='L'){
									nbytes_recv=0;
									break;
								}
								// If it is not the last block, receive the next buffer of data
								bytes_left = 0;
								nbytes_recv = recv(newsfd,buff,BUFFLEN,0);
							}
							// If the buffer is exhausted but chunk is yet to be written fully,
							// implies the next buffer will start with the continuition of this chunk
							else{
								// Adjust the chunk size left
								bytes_left -= nbytes_recv;
								// and get the new buffer data from the client
								bzero(buff,BUFFLEN);
								nbytes_recv = recv(newsfd,buff,BUFFLEN,0);
							}
						}
					}
				}
				// After writing is done, close the socket and file descriptor
				close(filefd);
				close(data_sfd);
				close(newsfd);
				// Exit with status 0. For any error, exit status will be 1
				exit(0);

			}
			// Control channel process
			else{
				// Wait for status message from server
				if(recv(sockfd,&stat_net,sizeof(stat_net),0)<0){
					perror("Receive error");
					exit(1);
				}
				// Print the status and appropriate message
				status = ntohl(stat_net);
				printf("Received status %d\n",status);
				if(status==GET_OK){
					printf("File transfer successful\n");
					continue;
				}
				// FILE_NOT_FOUND = GET_ERROR
				// In case of any error, we need to terminate the data channel process using kill
				else if(status == GET_ERROR){
					printf("Error in GET. Invalid filename or other error\n");
					remove(filename);
					kill(pid,SIGKILL);
				}
				else if(status == INVALID_ARGUMENTS){
					printf("Invalid arguments\n");
					remove(filename);
					kill(pid,SIGKILL);
				}
			}
		}
		// Put Command
		else if(strcmp(cmd,"put")==0){
			// Get filename
			char* filename = strtok(NULL," ");
			// Check for the file name
			int filefd = open(filename,O_RDONLY);
			// if file not found or can't be opened 
			if(filefd<0){
				perror("File open error");
				if(recv(sockfd,&stat_net,sizeof(stat_net),0)<0){
					perror("Receive error");
					exit(1);
				}
				status = ntohl(stat_net);
				printf("Received status %d\n",status);
				printf("Error in put command\n");
				continue;
			}
			pid_t pid = fork();
			if(pid<0){
				perror("Unable to create child process");
				exit(1);
			}
			else if(pid==0){
				int data_sfd,data_clilen,newsfd;
				struct sockaddr_in data_serv_addr,data_cli_addr;
				data_sfd = socket(AF_INET,SOCK_STREAM,0);
				if(data_sfd<0){
					perror("Unable to create socket");
					exit(1);
				}
				int enable_port_reuse = 1;
				if(setsockopt(data_sfd,SOL_SOCKET,SO_REUSEADDR,&enable_port_reuse,sizeof(int))<0){
					perror("setsockopt(SO_REUSEADDR) failed");
				}
				data_serv_addr.sin_family = AF_INET;
				data_serv_addr.sin_addr.s_addr = INADDR_ANY;
				data_serv_addr.sin_port = htons(port_Y);
				if(bind(data_sfd,(const struct sockaddr *)&data_serv_addr,sizeof(data_serv_addr))<0){
					perror("Bind failure");
					// exit(1);
				}
				listen(data_sfd,5);
				newsfd = accept(data_sfd,(struct sockaddr *)&data_cli_addr,&data_clilen);
				if(newsfd<0){
					perror("Accept error");
					exit(1);
				}
				bzero(buff,BUFFLEN);
				bzero(read_buff,READLEN);
				int nbytes_read = read(filefd,read_buff,READLEN-1);
				if(nbytes_read<0){
					perror("Read error");
					close(newsfd);
					close(filefd);
					exit(1);
				}
				while(nbytes_read>0){
					buff[0] = 'Z';
					unsigned short len = strlen(read_buff);
					memcpy(&buff[1],&len,sizeof(len));
					memcpy(&buff[3],read_buff,sizeof(read_buff));
					bzero(read_buff,READLEN);
					nbytes_read = read(filefd,read_buff,READLEN-1);
					if(nbytes_read<0){
						perror("Error in reading from file");
						close(filefd);
						close(newsfd);
						exit(1);
					}
					else if(nbytes_read == 0){
						buff[0] = 'L';
					}
					printf("Sending %s\n",buff);
					int sent_ret = send(newsfd,buff,len+3,MSG_NOSIGNAL);
					if(sent_ret<0){
						perror("Send error");
						close(filefd);
						close(newsfd);
						exit(1);
					}
					else if(sent_ret==0){
						close(filefd);
						close(newsfd);
						// break;
						exit(0);
					}
					bzero(buff,BUFFLEN);
				}
				exit(0);	
			}
			else{
				close(filefd);
				if(recv(sockfd,&stat_net,sizeof(stat_net),0)<0){
					perror("Receive error");
					exit(1);
				}
				status = ntohl(stat_net);
				printf("Received status: %d\n",status);
				if(status==PUT_OK){
					printf("File put successful\n");
				}
				else if(status == PUT_ERROR){ 
					printf("File put has failed. Possible invalid file name\n");
					kill(pid,SIGKILL);
				}
				else if(status == INVALID_ARGUMENTS){
					printf("Invalid arguments\n");
					kill(pid,SIGKILL);
					continue;
				}
			}

		}
		else if(strcmp(cmd,"quit")==0){
			if(recv(sockfd,&stat_net,sizeof(stat_net),0)<0){
				perror("Receive error");
				exit(1);
			}
			status = ntohl(stat_net);
			printf("Received status %d, Quitting the client\n",status);
			close(sockfd);
			exit(0);
		}
		else{
			if(recv(sockfd,&stat_net,sizeof(stat_net),0)<0){
				perror("Receive error");
				exit(1);
			}
			status = ntohl(stat_net);
			printf("Received status %d\n",status);
			if(status == UNKNOWN_COMMAND){
				printf("Unknown command\n");
				continue;
			}
		}
	}

}

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}