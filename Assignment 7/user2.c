// All required includes and protypes are in this header
#include "rsocket.h"
// Port numbers for the sockets
#define PORT1 53030
#define PORT2 53031

int main()
{
	int sockfd;
	// Create an MRP socket
	sockfd = r_socket(AF_INET,SOCK_MRP,0);
	struct sockaddr_in src_addr,dest_addr;
	if(sockfd<0){
		perror("Error in socket creation");
		exit(1);
	}
	// Fill sender details
	memset(&src_addr,0,sizeof(src_addr));
	src_addr.sin_family = AF_INET;
	src_addr.sin_addr.s_addr = INADDR_ANY;
	src_addr.sin_port = htons(PORT1);
	// Fill receiver details
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = INADDR_ANY;
	dest_addr.sin_port = htons(PORT2);
	// Bind the socket
	if(r_bind(sockfd,(const struct sockaddr *)&dest_addr,sizeof(dest_addr))<0){
		perror("Bind error");
		exit(1);
	}
	printf("Receiver running\n");
	char input;
	int len;
	// Receive all characters in a loop.
	// Here, we are not terminating the loop as we don't know when the receiver will 
	// close its end and since the order of delivery is not preserved, we can't use end markers like \0
	while(1){
		len = sizeof(src_addr);
		r_recvfrom(sockfd,&input,1,0,(struct sockaddr *)&src_addr,&len);
		printf("---------------------------\n");
		printf("Received %c\n",input);
		fflush(stdout);
		printf("---------------------------\n");
	}
	r_close(sockfd);
	return 0;
}