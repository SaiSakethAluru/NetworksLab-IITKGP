// includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// port number of server
#define UDPPORT 15000
#define BUFFLEN 1024

int main()
{
	int sockfd;
	struct sockaddr_in servaddr;
	// create udp socket
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd<0){
		perror("Error in creating client socket");
		exit(EXIT_FAILURE);
	}
	printf("Created socket\n");
	memset(&servaddr,0,sizeof(servaddr));
	//fill server details
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(UDPPORT);

	int n;
	socklen_t len;
	char buff[BUFFLEN];
	printf("Enter domain name: ");
	scanf("%s",buff);
	// send the domain name to server
	sendto(sockfd,(const char *)buff, strlen(buff)+1,0,(const struct sockaddr *)&servaddr,sizeof(servaddr));
	printf("Ip address of %s:\n",buff );
	len = sizeof(servaddr);
	bzero(buff,BUFFLEN);
	int nbytes;
	// receive ip addresses from server
	while((nbytes = recvfrom(sockfd,(char *)buff,BUFFLEN,0,(struct sockaddr *)&servaddr,&len))>0){
		printf("%s\n",buff);
		len = sizeof(servaddr);
		// if blank line is received, terminate
		if(strcmp(buff,"")==0)
			break;
		bzero(buff,BUFFLEN);
	}
	// close the socket and exit
	close(sockfd);
	return 0;
}