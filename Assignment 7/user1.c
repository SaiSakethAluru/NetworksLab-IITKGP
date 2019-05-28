// All required includes and protypes are in this header
#include "rsocket.h"

#define PORT1 53030
#define PORT2 53031

int main()
{
	char input_string[101];
	printf("Enter string\n");
	// Read the string
	scanf("%s",input_string);
	int sockfd;
	// Create the socket
	sockfd = r_socket(AF_INET,SOCK_MRP,0);
	struct sockaddr_in src_addr,dest_addr;
	if(sockfd<0){
		perror("Error in socket creation");
		exit(1);
	}
	memset(&src_addr,0,sizeof(src_addr));
	// Fill the sender details
	src_addr.sin_family = AF_INET;
	src_addr.sin_addr.s_addr = INADDR_ANY;
	src_addr.sin_port = htons(PORT1);
	// Fill the receiver details
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = INADDR_ANY;
	dest_addr.sin_port = htons(PORT2);
	// Bind the socket
	if(r_bind(sockfd,(const struct sockaddr *)&src_addr,sizeof(src_addr))<0){
		perror("Bind error");
		exit(1);
	}
	printf("Sender running\n");
	int i,len = strlen(input_string);
	// Send each character of the entered string one by one
	for(i=0;i<len;i++){
		int ret = r_sendto(sockfd,&input_string[i],1,0,(struct sockaddr *)&dest_addr,sizeof(dest_addr));
		if(ret<0){
			perror("Send error");
			exit(1);
		}
	}
	// Close the socket after sending
	r_close(sockfd);
	return 0;
}