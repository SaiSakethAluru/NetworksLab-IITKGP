#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>

// Global variables for I/O handling
int sockfd,flag = 0;
struct sockaddr_in serv_addr;

// Signal handler for I/O
void sigio_handler(int signo)
{
	if(signo == SIGIO){
		char buffer[100];
		struct sockaddr_in cli_addr;
		int cli_len = sizeof(cli_addr);
		// Receive message
		int n = recvfrom(sockfd,buffer,100,0,(struct sockaddr *)&cli_addr,&cli_len);
		printf("Received %s\n",buffer);
		// Echo back the message
		sendto(sockfd,buffer,n,0,(struct sockaddr *)&cli_addr,cli_len);
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
		perror("Socket");
		exit(1);
	}
	// Set flags for the socket
	fcntl(sockfd,F_SETOWN,getpid());
	int curr_flags = fcntl(sockfd,F_GETFL);
	fcntl(sockfd,F_SETFL,curr_flags|O_ASYNC);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(12345);
	// Bind the socket
	if(bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr))<0){
		perror("bind");
		exit(1);
	}
	printf("Server Running...\n");
	// Wait for I/O completion
	while(!flag){
		usleep(1000);
	}
	// Close the socket
	close(sockfd);
	return 0;
}