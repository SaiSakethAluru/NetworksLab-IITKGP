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
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
// Port number of server
#define PORT 10000
// Predefined block size
#define BUFFLEN 20
// Assumed max length of filename
#define FILELEN 100
int main()
{
	int sockfd,newsfd,clilen,i;
	struct sockaddr_in serv_addr, cli_addr;
	char buff[BUFFLEN], filename[FILELEN], code;
	// create socket descriptor
	sockfd = socket(AF_INET, SOCK_STREAM,0);
	if(sockfd < 0){
		perror("Socket creation error");
		exit(1);
	}
	// Enable the reuse of port address immediately after closing.
	int enable = 1;
	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&enable, sizeof(enable))<0){
		perror("setsockopt failed");
		exit(1);
	}
	// Fill in server ip and port details
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);
	// Bind the socket to the port
	if(bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
		perror("Bind failed");
		exit(1);
	}
	printf("Server is running...\n");
	// listen for upto 5 clients
	listen(sockfd,5);
	int nbytes_recv,filefd;
	while(1){
		// Accept new client connection
		clilen = sizeof(cli_addr);
		newsfd = accept(sockfd,(struct sockaddr *)&serv_addr,&clilen);
		if(newsfd<0){
			perror("Error in accepting client");
			exit(1);
		}
		printf("New client connected..\n");
		bzero(filename,FILELEN);
		int recv_done = 0;		// flag to indicate if \0 of filename has received or not
		while(!recv_done){
			bzero(buff,BUFFLEN);
			// receive part of filename
			nbytes_recv = recv(newsfd,buff,BUFFLEN-1,0);
			if(nbytes_recv < 0){
				perror("Error in receiving filename");
				exit(1);
			}
			// append to the filename till now
			strcat(filename,buff);
			// Check if \0 is received or not. If received, then filename is completely received
			for(i=0;i<BUFFLEN-1;i++){
				if(buff[i]=='\0'){
					recv_done = 1;
					break;
				}
			}
		}
		printf("Received filename %s\n",filename);
		// Open filename
		filefd = open(filename,O_RDONLY);
		// If error in opening
		if(filefd<0){
			perror("Unable to open file");
			// Send error code 'E' to client and close the connection
			code = 'E';
			send(newsfd,&code,sizeof(code),0);
			close(newsfd);
		}
		// If file opened successfully
		else{
			// Send success code to the client
			code = 'L';
			send(newsfd,&code,sizeof(code),0);
			// Get the file size
			struct stat st;
			if(fstat(filefd,&st)<0){
				perror("fstat failed");
				exit(1);
			}
			// Convert the file size from off_t type to int
			int f_size = (int)st.st_size;
			printf("File size = %d\n",f_size);
			// Convert the integer file size from host byte order to network byte order
			int fsize = htonl(f_size);
			// Send the file size to client
			if(send(newsfd,&fsize,sizeof(fsize),0)<0){
				perror("Error in sending file size");
				exit(1);
			}
			int nbytes_read;
			while(1){
				// Read data from the file
				nbytes_read = read(filefd,buff,BUFFLEN);
				if(nbytes_read<0){
					perror("read error");
					close(newsfd);
					exit(1);
				}
				// If reading is completed
				else if(nbytes_read == 0)
					break;
				// Send the read data to client
				if(send(newsfd,buff,BUFFLEN,0)<0){
					perror("sending error");
					close(newsfd);
					break;
				}
				bzero(buff,BUFFLEN);
			}
			// close socket and file descriptors
			close(filefd);
			close(newsfd);
			printf("Transer complete\n");
		}
	}
}