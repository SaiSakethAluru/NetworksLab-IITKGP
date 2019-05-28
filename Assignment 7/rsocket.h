#ifndef RSOCKET_H
#define RSOCKET_H
// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

// Macros required
#define TIMEOUT_T_SEC 2
#define TIMEOUT_T_USEC 0
#define DROP_PROB 0.5
#define SOCK_MRP 1

// Datatype of receive buffer
typedef struct {
	char message[101];
	struct sockaddr_in source_addr;
	int msg_len;
}recv_buff_node;

// Datatype for unacknowledged message
typedef struct {
	int id;
	char message[110];
	int msg_len;
	struct sockaddr_in dest_addr;
	struct timeval tv;
}unack_msg_node;

// Function prototypes
int r_socket(int domain, int type, int protocol);
int r_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t r_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t r_recvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen);
void* thread_X(void *param);
void HandleRetransmit(int sockfd);
void HandleReceive(int sockfd,char* buffer,const struct sockaddr * src_addr,int msg_len);
void HandleAppMsgRecv(int sockfd,char* buffer,const struct sockaddr *src_addr,int msg_len);
void sendAck(int id, int sockfd, const struct sockaddr *dest_addr);
void HandleACKMsgRecv(char* buffer);
int min(int a,int b);
int dropMessage(float p);
int r_close(int sfd);

#endif


