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
#include <errno.h>
#include <poll.h>

#define BUFF_LEN 100
#define PORT_NUM 10000

int main()
{
	int sfd,i;
	struct sockaddr_in serv_addr;
	char buff[BUFF_LEN];
	char filename[100];
	fd_set read_fd;
	struct timeval tv;
	sfd = socket(AF_INET,SOCK_STREAM,0);
	if(sfd<0){
		perror("Unable to create socket");
		exit(EXIT_FAILURE);
	}
	serv_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1",&serv_addr.sin_addr);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Enter filename: ");
	scanf("%s",filename);
	if(connect(sfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr))<0){
		perror("Unable to connect to server");
		exit(EXIT_FAILURE);
	}
	printf("Connected..\n");
	int nbytes = send(sfd,filename,strlen(filename)+1,0);
	if(nbytes<0){
		perror("Unable to send");
		exit(EXIT_FAILURE);
	}
	struct pollfd pfd;
	pfd.fd = sfd;
	pfd.events = POLLIN | POLLHUP | POLLRDNORM;
	pfd.revents = 0;
	if(poll(&pfd,1,500)>0){
		if(recv(sfd,buff,BUFF_LEN,MSG_PEEK|MSG_DONTWAIT)==0){
			printf("File not found\n");
			close(sfd);
			exit(0);
		}
	}
	int fileid = open(filename,O_WRONLY|O_CREAT,S_IRWXU);
	if(fileid < 0){
		perror("Unable to open new file");
		exit(EXIT_FAILURE);
	}
	int byte_count = 0,word_count=0;
	FD_ZERO(&read_fd);
	int isWord = 0;
	while(1){
		int retval=0;
		FD_SET(sfd,&read_fd);
		tv.tv_sec = 0;
		tv.tv_usec = 500;
		for(i=0;i<BUFF_LEN;i++){
			buff[i] = '\0';
		}
		retval = select(sfd+1,&read_fd,NULL,NULL,&tv);
		if(retval<0){
			perror("Client select error");
			exit(EXIT_FAILURE);
		}
		else if(retval>0){
			nbytes = recv(sfd,buff,BUFF_LEN,0);
			if(nbytes < 0){
				perror("Error in receiving");
				exit(EXIT_FAILURE);
			}
			byte_count += nbytes;
			write(fileid,buff,nbytes);
			for(int i=0;i<strlen(buff);i++){
				if(isWord && (buff[i]==','||buff[i]==';'||buff[i]==':'||buff[i]=='.'||buff[i]==' '||buff[i]=='\t')){
					word_count++;
					isWord = 0;
				}
				else if(!isWord && !(buff[i]==','||buff[i]==';'||buff[i]==':'||buff[i]=='.'||buff[i]==' '||buff[i]=='\t')){
					isWord = 1;
				}
			}
		}
		else break;
	}
	if(isWord)
		word_count++;
	printf("The file transfer is successful. Size of the file = %d bytes, no. of words = %d\n",byte_count,word_count);
	close(sfd);
	return 0;
}