/*
Name: Sai Saketh Aluru
Roll No. 16CS30030
*/
// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

// Buffer length and port number macros
#define BUFF_LEN 50
#define PORT_NUM 10001
int main()
{
	// sockfds before and after connection
	int sfd, newsfd, i;
	int clientlen;
	struct sockaddr_in serv_addr,client_addr; // socket variables
	char buff[BUFF_LEN]; // buffer for reading and sending
	char filename[100];  // to store file name
	sfd = socket(AF_INET,SOCK_STREAM,0); // create socket
	if(sfd<0){
		perror("Unable to create server socket");
		exit(EXIT_FAILURE);
	}
	// populate values in the structure
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT_NUM);
	// bind the socket
	if(bind(sfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)),sizeof(serv_addr) < 0){
		perror("Unable to bind the server");
		exit(EXIT_FAILURE);
	}
	// listen on the socket for maximum 5 clients
	listen(sfd,5);
	printf("Server waiting for connection...\n");
	int nbytes_read = 0;
	while(1){
		clientlen = sizeof(client_addr);
		// accept new client
		newsfd = accept(sfd,(struct sockaddr *)&client_addr,&clientlen);
		if(newsfd<0){
			perror("Unable to accept client connection");
			exit(EXIT_FAILURE);
		}
		printf("Client connected\n");
		// receive file name from client
		// ---- wrong: should recv in a loop
		recv(newsfd,filename,100,0);		
		printf("Received file name = %s\n",filename);
		int filefd = open(filename,O_RDONLY);
		// if file is not there, filefd < 0,
		if(filefd < 0){
			perror("Unable to open file");
			// close connection
			close(newsfd);
		}
		// file present
		else{
			// initialise buffer before reading
			for(i=0;i<BUFF_LEN;i++)
				buff[i] = '\0';
			// read and send till data is present in file
			while((nbytes_read = read(filefd,buff,BUFF_LEN))>0){
				send(newsfd,buff,nbytes_read,0);
			}
			// close after sending everything
			close(newsfd);
			close(filefd);
		}
		printf("Transfer complete\n");
		memset(filename,0,sizeof(filename)); // reset filename for next iteration
	}
}