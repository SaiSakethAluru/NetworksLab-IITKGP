/*
Name: Sai Saketh Aluru
Roll no: 16CS30030
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// Maximum sizes of buffer array and WORDi array
#define MAXSIZE 1024
#define COUNT_SIZE 10
int main()
{
	int sockfd;
	struct sockaddr_in servaddr;
	//create socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd<0){
		perror("Error in creating client socket");
		exit(EXIT_FAILURE);
	}
	printf("Created socket\n");
	memset(&servaddr,0,sizeof(servaddr));
	// fill address details
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(9000);
	
	int n;
	socklen_t len;
	char file[MAXSIZE];
	char buffer[MAXSIZE];
	printf("Enter name of the file: ");
	scanf("%s",file);
	// send file name
	sendto(sockfd,(const char*) file, strlen(file)+1,0,(const struct sockaddr*)&servaddr, sizeof(servaddr));
	printf("CLIENT: Sent file name %s\n",file);
	len = sizeof(servaddr);
	n = recvfrom(sockfd,(char*) buffer, MAXSIZE, 0, (struct sockaddr *) &servaddr, &len);
	printf("Received %s\n",buffer);
	if(strcmp(buffer,"NOTFOUND")==0){
		printf("File not found\n");
		exit(0);
	}
	FILE *fptr;
	fptr = fopen("client_file.txt","w");
	int count = 1;
	while(1){
		char send[] = "WORD";
		char count_to_str[COUNT_SIZE];
		sprintf(count_to_str,"%d",count++);
		strcat(send,count_to_str);
		sendto(sockfd,(const char*) send,strlen(send)+1,0,(const struct sockaddr *)&servaddr, sizeof(servaddr));
		printf("CLIENT: Sent %s\n",send);
		len = sizeof(servaddr);
		n = recvfrom(sockfd,(char *) buffer, MAXSIZE, 0, (struct sockaddr *) &servaddr, &len);
		printf("Received %s",buffer);
		if(strcmp(buffer,"END")==0){
			break;
		}
		printf(" writing to file\n");
		fprintf(fptr,"%s\n",buffer);
	}
	fclose(fptr);
	printf("\nCLIENT: Transfer complete\n");

	return 0;
}
