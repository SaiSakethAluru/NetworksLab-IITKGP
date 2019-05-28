/*
	Out of signal driven I/O and Non-blocking I/O, it is better to use nonblocking I/O as it
	is safer. In signal driven I/O, we don't have any control regarding when the I/O is
	performed, as the signal can come at anytime depending on when the data is received. As the singal handler function's prototype is fixed, we have limited ways of doing 
	asynchronous I/O. 
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

int sockfd,flag = 0;
struct sockaddr_in serv_addr;

// Signal Handler
void sigio_handler(int signo)
{
	// Process I/O
	if(signo == SIGIO){
		char buffer[100];
		int serv_len = sizeof(serv_addr);
		recvfrom(sockfd,buffer,100,0,(struct sockaddr *)&serv_addr,&serv_len);
		printf("Received %s\n",buffer);
		flag = 1;
	}
	// Reinstall signal handler
	signal(SIGIO,sigio_handler);
}
int main()
{
	// Install signal handler
	signal(SIGIO,sigio_handler);
	// Create socket
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd<0){
		perror("sockfd");
		exit(1);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(12345);
	// set flags for the socket
	fcntl(sockfd,F_SETOWN,getpid());
	int curr_flags = fcntl(sockfd,F_GETFL);
	fcntl(sockfd,F_SETFL,curr_flags|O_ASYNC);
	// Read input from user
	printf("Enter string to echo:\n");
	char buffer[100];
	bzero(buffer,100);
	scanf("%s",buffer);
	// Send to server
	sendto(sockfd,buffer,strlen(buffer)+1,0,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	// Wait for I/O completion
	while(!flag){
		usleep(1000);
	}
	// close the socket
	close(sockfd);
	return 0;
}