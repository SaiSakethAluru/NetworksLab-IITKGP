/*
Name: Sai Saketh Aluru
Roll No. 16CS30030
*/
// Standard includes
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
// Buffer length and port number macros
#define BUFF_LEN 50
#define PORT_NUM 10001

int main()
{
	int sfd,i;
	struct sockaddr_in serv_addr; 	// socket variables
	char buff[BUFF_LEN]; 	// buffer for receive and write
	char filename[100];		// storing filename
	sfd = socket(AF_INET,SOCK_STREAM,0);	// create socket
	if(sfd<0){
		perror("Unable to create socket");
		exit(EXIT_FAILURE);
	}
	// fill in server socket details
	serv_addr.sin_family = AF_INET;
	// ---- doubt: can we simple assign inaddr_any to serv_addr.sin_addr.s_addr? 
	inet_aton("127.0.0.1",&serv_addr.sin_addr);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Enter filename: ");
	scanf("%s",filename);	// read file name
	// connect to server
	if(connect(sfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr))<0){
		perror("Unable to connect to server");
		exit(EXIT_FAILURE);
	}
	printf("Connected..\n");
	// send file name
	int nbytes = send(sfd,filename,strlen(filename)+1,0);
	if(nbytes<0){
		perror("Unable to send");
		exit(EXIT_FAILURE);
	}
	// wait for receiving from server
	nbytes = recv(sfd,buff,BUFF_LEN,0);
	if(nbytes<0){
		perror("Receive error");
		exit(EXIT_FAILURE);
	}
	// recv returns 0 if connection is closed by server => file not found
	if(nbytes==0){
		printf("File not found\n");
		close(sfd);
		exit(0);
	}
	// if server sent something, 
	else{
		// open new file
		int fileid = open("client_file.txt",O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
		if(fileid < 0){
			perror("Unable to open new file");
			exit(EXIT_FAILURE);
		}
		// initialise counts to zero
		int byte_count = 0,word_count=0;
		// flag to determine if current part of buffer character is a word or not
		int isWord = 0;
		while(1){
			// write the received data to file
			write(fileid,buff,nbytes);
			byte_count += nbytes; 	// update byte count
			for(int i=0;i<strlen(buff);i++){
				// count words
				// we scanned word till now and encountered delimeter
				if(isWord && (buff[i]==','||buff[i]==';'||buff[i]==':'||buff[i]=='.'||buff[i]==' '||buff[i]=='\t'||buff[i]=='\n')){
					word_count++;
					isWord = 0;
				}
				// we scanned delimeter till now and reached another word's first character
				else if(!isWord && !(buff[i]==','||buff[i]==';'||buff[i]==':'||buff[i]=='.'||buff[i]==' '||buff[i]=='\t'||buff[i]=='\n')){
					isWord = 1;
				}
			}
			// reset buffer for next iteration
			for(i=0;i<BUFF_LEN;i++){
				buff[i] = '\0';
			}
			// receive next buffer
			nbytes = recv(sfd,buff,BUFF_LEN,0);
			// recv gives zero if everything is read and connection is closed.
			if(nbytes == 0)
				break;
		}
		// if last word is not followed by delimeter
		if(isWord)
		word_count++;
		// print message
		printf("The file transfer is successful. Size of the file = %d bytes, no. of words = %d\n",byte_count,word_count);
		close(sfd); 	// close connection from client side
		close(fileid);
	}
	return 0;
}