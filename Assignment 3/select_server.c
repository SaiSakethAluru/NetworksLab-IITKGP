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

// Port numbers for both sockets
#define TCPPORT 10000
#define UDPPORT 15000
#define BUFFLEN 1024

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
	// create udp socket
	udpsfd = socket(AF_INET,SOCK_DGRAM,0);
	if(udpsfd < 0){
		perror("Cannot create udp socket");
		exit(EXIT_FAILURE);
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
	// fill tcp server details
	tcp_serv_addr.sin_family = AF_INET;
	tcp_serv_addr.sin_addr.s_addr = INADDR_ANY;
	tcp_serv_addr.sin_port = htons(TCPPORT);
	// bind tcp server to sfd
	if(bind(tcpsfd,(const struct sockaddr *)&tcp_serv_addr,sizeof(tcp_serv_addr)) < 0){
		perror("Unable to bind udp server");
		exit(EXIT_FAILURE);
	}
	printf("Created TCP socket..\n");
	// listen on tcp socket for upto 5 clients
	listen(tcpsfd,5);
	// used for dns application
	struct hostent *hp;
	// used for select call
	fd_set readfs;
	int select_ret,nfds;
	while(1){
		// set the bits of readfs for both the sockets
		FD_ZERO(&readfs);
		FD_SET(udpsfd,&readfs);
		FD_SET(tcpsfd,&readfs);
		// get max value till which bit to check in readfs
		nfds = max(udpsfd,tcpsfd)+1;
		// wait for either sockets to have a client
		select_ret = select(nfds,&readfs,0,0,0);
		if(select_ret<0){
			perror("Error in select");
			exit(EXIT_FAILURE);
		}
		// If udp client is present, create a child process to handle the client.
		// parent goes back to select to wait on the sockets
		if(FD_ISSET(udpsfd,&readfs)){
			memset(&cli_addr,0,sizeof(cli_addr));
			if(fork()==0){
				clilen = sizeof(cli_addr);
				// receive domain names
				int nbytes = recvfrom(udpsfd,(char *)buff,BUFFLEN,0,(struct sockaddr *)&cli_addr,&clilen);
				printf("\nGot address %s from UDP client\n",buff);
				if(nbytes < 0){
					perror("Error in udp recvfrom");
					exit(EXIT_FAILURE);
				}
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
		}
		// if it is a tcp client
		if(FD_ISSET(tcpsfd,&readfs)){
			printf("Found TCP socket..\n");
			clilen = sizeof(cli_addr);
			// accept the connection to the client
			newsfd = accept(tcpsfd,(struct sockaddr *)&cli_addr,&clilen);
			if(newsfd<0){
				perror("Unable to accept tcp client");
				exit(EXIT_FAILURE);
			}
			// create child process
			if(fork()==0){	
				// close the original tcp socket as it is not needed in child process.
				close(tcpsfd);
				// open word.txt
				int fileid = open("word.txt",O_RDONLY);
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
				// close the socket and terminate child process
				close(newsfd); 
				exit(0);
			}
			// parent doesn't need the accepted socket. hence close it.
			close(newsfd);
		}
	}
}
