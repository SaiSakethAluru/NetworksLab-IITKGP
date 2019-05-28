// standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// port number for connection
#define TCPPORT 10000
#define BUFFLEN 1024
#define FILELEN 1000
int main()
{
	int sockfd,i,nbytes;
	struct sockaddr_in serv_addr;
	char buff[BUFFLEN],filename[FILELEN];
	// create socket
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0){
		perror("Unable to create socket");
		exit(EXIT_FAILURE);
	}
	// fill in server details
	serv_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1",&serv_addr.sin_addr);
	serv_addr.sin_port = htons(TCPPORT);
	// connect to the server
	if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
		perror("Unable to connect to server");
		exit(EXIT_FAILURE);
	}
	printf("Connected to server\n");
	// open a new file to copy the bag of words to 
	printf("Enter filename: ");
	scanf("%s",filename);
	send(sockfd,filename,strlen(filename)+1,0);
	int fileid = open(filename,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
	if(fileid<0){
		perror("Unable to open file");
		exit(EXIT_FAILURE);
	}
	int word_count = 0,j=0;
	char temp_buff[BUFFLEN];
	bzero(temp_buff,BUFFLEN);
	int done = 0;
	while(!done){
		// receive data
		nbytes = recv(sockfd,buff,BUFFLEN-1,0);
		if(nbytes == 0){
			printf("File not found\n" );
			exit(EXIT_FAILURE);
		}
		// loop over the buffer to identify \0 indicating end of words
		for(i=0;i<nbytes;i++){
			if(buff[i]!='\0'){
				temp_buff[j] = buff[i];
				j++;
			}
			if(buff[i]=='\0'){
				temp_buff[j] = buff[i];
				// if \0\0 is received, data transfer is complete.
				// print the no. of words and exit
				if(strcmp(temp_buff,"")==0){
					done = 1;
					break;
				}
				word_count++;
				printf("Received %s",temp_buff);
				write(fileid,temp_buff,strlen(temp_buff));
				j=0;
				bzero(temp_buff,BUFFLEN);
			}
		}
		bzero(buff,BUFFLEN);
	}
	printf("\n\nNo. of words received : %d\n",word_count);
	close(fileid);
	close(sockfd);
	return 0;
}
