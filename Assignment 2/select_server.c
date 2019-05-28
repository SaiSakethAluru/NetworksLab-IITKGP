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

#define BUFF_LEN 100
#define PORT_NUM 10000
int main()
{
	int sfd, newsfd, i;
	int clientlen;
	struct sockaddr_in serv_addr,client_addr;
	char buff[BUFF_LEN];
	char filename[100] = "";
	struct timeval tv;
	fd_set read_fd;
	sfd = socket(AF_INET,SOCK_STREAM,0);
	if(sfd<0){
		perror("Unable to create server socket");
		exit(EXIT_FAILURE);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT_NUM);
	if(bind(sfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)),sizeof(serv_addr) < 0){
		perror("Unable to bind the server");
		exit(EXIT_FAILURE);
	}
	listen(sfd,5);
	printf("Server waiting for connection...\n");
	int nbytes_read = 0;
	while(1){
		clientlen = sizeof(client_addr);
		newsfd = accept(sfd,(struct sockaddr *)&client_addr,&clientlen);
		if(newsfd<0){
			perror("Unable to accept client connection");
			exit(EXIT_FAILURE);
		}
		printf("Client connected\n");
		int retval=0;
		FD_ZERO(&read_fd);
		while(1){
			FD_SET(newsfd,&read_fd);
			tv.tv_sec = 0;
			tv.tv_usec = 100;
			for(i=0;i<BUFF_LEN;i++){
				buff[i] = '\0';
			}
			retval = select(newsfd+1,&read_fd,NULL,NULL,&tv);
			if(retval<0){
				perror("Error in server read select");
				exit(EXIT_FAILURE);
			}
			else if(retval>0){
				recv(newsfd,buff,BUFF_LEN,MSG_DONTWAIT);
				strcat(filename,buff);
			}
			else break;
		}
		printf("Received file name = %s\n",filename);
		int filefd = open(filename,O_RDONLY);
		if(filefd < 0){
			perror("Unable to open file");
			close(newsfd);
		}
		else{
			for(i=0;i<BUFF_LEN;i++)
				buff[i] = '\0';
			while((nbytes_read = read(filefd,buff,BUFF_LEN))>0){
				send(newsfd,buff,nbytes_read,0);
			}
		}
		printf("Transfer complete\n");
		strcpy(filename,"");
	}
}