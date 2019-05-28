/*
Name: Sai Saketh Aluru
Roll no: 16CS30030
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// maxsize of buffer array for sending/receiving data
#define MAXSIZE 1024

int main()
{
	// define socket descriptor and address variables
	int sockfd;
	struct sockaddr_in servaddr, clientaddr;
	// creating socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0){
		perror(" error in creating socket");
		exit(EXIT_FAILURE);
	}
	memset(&servaddr,0,sizeof(servaddr));
	memset(&clientaddr,0,sizeof(clientaddr));
	
	// fill in address of server
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(32164);
	
	// bind socket
	if(bind(sockfd,(const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
		perror("error in binding server socket");
		exit(EXIT_FAILURE);
	}
	printf("Server Running...\n");
	
	int n;
	socklen_t len;
	char buffer[MAXSIZE];
	len = sizeof(clientaddr);
	n = recvfrom(sockfd,(char *) buffer, MAXSIZE, 0, (struct sockaddr *) &clientaddr, &len);
	printf("Received filename %s\n",buffer);
	FILE *fptr;
	fptr = fopen(buffer,"r");
	// file is not present
	if(fptr == NULL){
		char *notfound = "NOTFOUND";
		sendto(sockfd,(const char *) notfound, strlen(notfound)+1,0,(const struct sockaddr *) &clientaddr, sizeof(clientaddr));
		printf("SERVER: NOTFOUND\n");
		exit(0);
	}
	char word[MAXSIZE];
	fscanf(fptr,"%s",word);
	sendto(sockfd,(const char*) word,strlen(word)+1,0,(const struct sockaddr *) &clientaddr,sizeof(clientaddr));
	printf("SERVER: Sent %s\n",word);			
	while(1)
	{
		len = sizeof(clientaddr);
		n = recvfrom(sockfd,(char *) buffer,MAXSIZE,0,(struct sockaddr *) &clientaddr, &len);
		printf("Received %s\n",buffer);
		fscanf(fptr,"%s",word);
		sendto(sockfd,(const char*) word,strlen(word)+1,0,(const struct sockaddr *) &clientaddr,sizeof(clientaddr));
		printf("SERVER: Sent %s\n",word);
		if(strcmp(word,"END")==0){
			break;
		}
	}
	fclose(fptr);
	printf("SERVER: Transfer complete\n");
	return 0;
}
