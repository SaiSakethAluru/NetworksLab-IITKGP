/*
	Name: Sai Saketh Aluru
	Roll No. 16CS30030
*/
// Standard includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Port number of client
#define PORT 10000
// predefined block size
#define BUFFLEN 20
// Assumed max length of filename
#define FILELEN 100

int main()
{
	int sockfd,i;
	struct sockaddr_in serv_addr;
	char buff[BUFFLEN],filename[FILELEN];
	// Create 
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd<0){
		perror("Failed to create socket");
		exit(1);
	}
	// Fill in server ip and port details
	serv_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1",&serv_addr.sin_addr);
	serv_addr.sin_port = htons(PORT);
	// Connect to the server
	if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
		perror("Unable to connect");
		exit(1);
	}
	printf("Connected to server..\n");
	// Read the filename from the server
	printf("Enter file name: ");
	scanf("%s",filename);
	printf("Entered filename: %s\n",filename);
	// Open the file with the name given. Create it if not present
	int filefd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
	if(filefd<0){
		perror("Error in opening file");
		exit(1);
	}
	// Send the filename to the server
	send(sockfd,filename,strlen(filename)+1,0);
	char code;
	// Receive the character code from the server
	recv(sockfd,&code,sizeof(char),0);
	printf("Received code %c\n",code);
	// If the code is E, it is an error code. Close the connection and terminate
	if(code == 'E'){
		printf("File Not Found\nExiting\n");
		// Remove the file created
		remove(filename);
		// Close the connection and exit
		close(sockfd);
		exit(0);
	}
	// If file is found at the server
	else{
		int fsize,f_size;
		// Receive the filesize from the client
		int nbytes_recv = recv(sockfd,&f_size,sizeof(f_size),MSG_WAITALL);
		if(nbytes_recv<0){
			perror("Unable to receive");
			exit(1);
		}
		else if(nbytes_recv == 0){
			perror("Connection closed");
			exit(1);
		}
		// Convert from network byte order to host byte order
		fsize = ntohl(f_size);
		printf("Received size of the file %d\n",fsize);
		// Determine the number of full length blocks that are expected
		int full_blocks = fsize / BUFFLEN;
		for(i=0;i<full_blocks;i++){
			bzero(buff,BUFFLEN);
			// Receive data from client. Block till the fixed block length size data is received
			recv(sockfd,buff,BUFFLEN,MSG_WAITALL);
			// Write to the file
			write(filefd,buff,BUFFLEN);
		}
		// Determine the size of the last block 
		int last_block = fsize % BUFFLEN;
		// If last block has size 0, no more blocks left. print message and exit
		if(last_block == 0){
			printf("The file transfer is successful. Total number of blocks received = %d, Last block size = %d\n",full_blocks,BUFFLEN);
		}
		// If there is some data left
		else{
			printf("Last block size = %d\n",last_block);
			bzero(buff,BUFFLEN);
			// Receive the last block of size F_SIZE%BUFFLEN. Block till all the bytes are received
			recv(sockfd,buff,last_block,MSG_WAITALL);
			// Write to the file
			write(filefd,buff,last_block);
			printf("The file transfer is successful. Total number of blocks received = %d, Last block size = %d\n",full_blocks+1,last_block);
		}
		// Close the socket and file descriptors
		close(filefd);
		close(sockfd);
		printf("Exiting the client\n");
	}
	return 0;
}