// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

// Port numbers for both sockets
#define TCPPORT 10000
#define UDPPORT 15000
#define BUFFLEN 1024
#define FILELEN 1000
// function to compute max of two numbers, used for select
int max(int a,int b)
{
	return a>b?a:b;
}
int main()
{
	int udpsfd,tcpsfd,newsfd;
	struct sockaddr_in udp_serv_addr,tcp_serv_addr,cli_addr;
	struct in_addr ip_addr;
	int clilen, i;
	char buff[BUFFLEN];
	char filename[FILELEN];
	// create udp socket
	udpsfd = socket(AF_INET,SOCK_DGRAM,0);
	if(udpsfd < 0){
		perror("Cannot create udp socket");
		exit(EXIT_FAILURE);
	}
	// Enable the reuse of port address immediately after closing.
	int enable = 1;
	if(setsockopt(udpsfd,SOL_SOCKET,SO_REUSEADDR,&enable, sizeof(enable))<0){
		perror("setsockopt failed");
		exit(1);
	}
	// fill in udp server details
	udp_serv_addr.sin_family = AF_INET;
	udp_serv_addr.sin_addr.s_addr = INADDR_ANY;
	udp_serv_addr.sin_port = htons(UDPPORT);
	// bind the socket to the socket descriptor
	if(bind(udpsfd,(const struct sockaddr *)&udp_serv_addr,sizeof(udp_serv_addr)) < 0){
		perror("Unable to bind udp server");
		exit(EXIT_FAILURE);
	}
	printf("Created UDP socket..\n");
	// create tcp socket
	tcpsfd = socket(AF_INET,SOCK_STREAM,0);
	if(tcpsfd < 0){
		perror("Cannot create tcp socket");
		exit(EXIT_FAILURE);
	}
	// Enable the reuse of port address immediately after closing.
	if(setsockopt(tcpsfd,SOL_SOCKET,SO_REUSEADDR,&enable, sizeof(enable))<0){
		perror("setsockopt failed");
		exit(1);
	}
	// fill tcp server details
	tcp_serv_addr.sin_family = AF_INET;
	tcp_serv_addr.sin_addr.s_addr = INADDR_ANY;
	tcp_serv_addr.sin_port = htons(TCPPORT);
	// bind tcp server to sfd
	if(bind(tcpsfd,(const struct sockaddr *)&tcp_serv_addr,sizeof(tcp_serv_addr)) < 0){
		perror("Unable to bind udp server");
		exit(EXIT_FAILURE);
	}
	// Set the tcp socket to be non-blocking
	fcntl(tcpsfd,F_SETFL,O_NONBLOCK);
	printf("Created TCP socket..\n");
	// listen on tcp socket for upto 5 clients
	listen(tcpsfd,5);
	// used for dns application
	struct hostent *hp;
	while(1){
		// sleep for 1ms
		usleep(1000);
		// Accept tcp client connection. Since the socket is non blocking, accept won't block
		// If there is no connection at the moment, accept returns error with 
		// errno set to EAGAIN or EWOULDBLOCK
		clilen = sizeof(cli_addr);
		// Handle all tcp connections in a loop
		while(1){
			newsfd = accept(tcpsfd,(struct sockaddr *)&cli_addr,&clilen);
			if(newsfd<0){
				if(errno == EAGAIN || errno == EWOULDBLOCK){
					break;
				}
				else{
					perror("Error in accept call");
					exit(1);
				}
			}
			// If newsfd is a valid socket descriptor for the client connection
			else{
				printf("\nTCP Client connected\n");
				// Fork and create child process to handle the client
				if(fork()==0){	
					// close the original tcp socket as it is not needed in child process.
					close(tcpsfd);
					// receive filename from client
					bzero(filename,FILELEN);
					int recv_done = 0;		// flag to indicate if \0 of filename has received or not
					int nbytes_recv;
					while(!recv_done){
						bzero(buff,BUFFLEN);
						// receive part of filename
						nbytes_recv = recv(newsfd,buff,BUFFLEN-1,0);
						if(nbytes_recv < 0){
							perror("Error in receiving filename");
							exit(1);
						}
						// append to the filename till now
						strcat(filename,buff);
						// Check if \0 is received or not. If received, then filename is completely received
						for(i=0;i<BUFFLEN-1;i++){
							if(buff[i]=='\0'){
								recv_done = 1;
								break;
							}
						}
					}
					int fileid = open(filename,O_RDONLY);
					if(fileid < 0){
						perror("Unable to open file");
						close(newsfd);
						exit(EXIT_FAILURE);
					}
					bzero(buff,BUFFLEN);
					int nbytes,j=0;
					char temp_buff[BUFFLEN];
					bzero(temp_buff,BUFFLEN);
					// read data from file
					while((nbytes = read(fileid,buff,BUFFLEN-1))>0){
						// loop over the buffer, identify \n indicating end of words,
						// add a \0 to the end and then send the word to client
						for(i=0;i<nbytes;i++){
							if(buff[i] != '\n'){
								temp_buff[j] = buff[i];
								j++;
							}
							if(buff[i]=='\n'){
								temp_buff[j] = '\n';
								temp_buff[j+1] = '\0';
								j=0;
								send(newsfd,temp_buff,strlen(temp_buff)+1,0);
								printf("Sending %s",temp_buff);
								bzero(temp_buff,BUFFLEN);
							}
						}
						// if the last word is not followed by a \n.
						if(j!=0){
							temp_buff[j] = '\0';
							send(newsfd,temp_buff,strlen(temp_buff)+1,0);
							printf("Sending %s",temp_buff);
						}
						bzero(buff,BUFFLEN);
					}
					// send blank line (\0\0) indicating end of data
					strcpy(buff,"");
					send(newsfd,buff,strlen(buff)+1,0);
					printf("\nTransfer complete\n");
					// close the socket, file descriptor and terminate child process
					close(newsfd); 
					close(fileid);
					exit(0);
				}
				else{
					// The accepted connection is not needed in parent process. Hence close it
					close(newsfd);
				}
			}
		}
		// Clear buffer and client address structure
		bzero(buff,BUFFLEN);
		memset(&cli_addr,0,sizeof(cli_addr));
		clilen = sizeof(cli_addr);
		// Receive the host name from client. Make the receive call non-blocking 
		// If there is no data received, then recvfrom return -1 
		// with errno set to EAGAIN or EWOULDBLOCK
		int nbytes = recvfrom(udpsfd,(char *)buff,BUFFLEN,MSG_DONTWAIT,(struct sockaddr *)&cli_addr,&clilen);
		if(nbytes < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				// Do nothing as nothing is received. Go back to start of while loop and sleep
			}
			else{
				perror("Error in recvfrom call");
				exit(1);
			}
		}
		// If data is received from client
		else{
			// Fork and create child process to handle the client
			if(fork()==0){
				printf("\nGot address %s from UDP client\n",buff);
				// get the host address from the domain
				hp = gethostbyname(buff);
				if(hp == NULL){
					printf("Invalid address\n");
					exit(EXIT_FAILURE);
				}
				int j = 0;
				// send each ip address in the list to the client back
				while(hp->h_addr_list[j]!=NULL){
					ip_addr = *(struct in_addr *)(hp->h_addr_list[j]);
					bzero(buff,BUFFLEN);
					strcpy(buff,inet_ntoa(ip_addr));
					printf("%s\n",buff);
					sendto(udpsfd,(const char *) buff,strlen(buff)+1,0,(const struct sockaddr *)&cli_addr,sizeof(cli_addr));
					j++;
				}
				// send blank line to indicate completion
				sendto(udpsfd,(const char *)"",strlen(""),0,(const struct sockaddr *)&cli_addr,sizeof(cli_addr));
				// close the socket and terminate the child process
				close(udpsfd); 
				exit(0);
			}
			else{
				// Parent need not do anything - Go back to start of while loop and sleep
			}
		}		
	}
}
