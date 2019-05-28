#include "rsocket.h"

/* TODO:
Test mutex locks
How to close socket on receiver end
*/
// Global variables

// Receive buffer - stores messages along with sender ip-port
recv_buff_node *receive_buffer;
int front=0;	// Index to read from in receive buffer
int rear=0;		// Index to write to in receive buffer

// Receive message id table - list of all message ids received till now
int *received_msg_id_table;
int msg_count = 0;		// Number of msgs received - index for received_msg_id_table

// Unacknowledged message table
unack_msg_node* unack_msg_table;
int unack_msg_last = -1;	// Index of last entered location in unack_msg_table

// Thread related variables
pthread_t tid;
pthread_mutex_t mutex;
pthread_mutexattr_t mutex_a;

// Total transmissions done through the socket - Used to calculate metric
int num_tranmissions = 0;

// Function to create socket and initialise data structures
int r_socket(int domain, int type, int protocol)
{
	// Create a UDP socket
	int sfd = socket(domain,SOCK_DGRAM,protocol);
	if(sfd<0){
		return -1;
	}
	else{
		// Used for dropMessage function to generate random numbers
		srand(time(NULL));
		// Initialise data structures and thread-X
		pthread_mutexattr_init(&mutex_a);
		pthread_mutex_init(&mutex,&mutex_a);
		int *param = (int*)malloc(sizeof(int));
		*param = sfd;
		pthread_create(&tid,NULL,thread_X,param);	// add attributes
		receive_buffer = (recv_buff_node*) calloc(100,sizeof(recv_buff_node));
		received_msg_id_table = (int*) calloc(100,sizeof(int));
		unack_msg_table = (unack_msg_node*) calloc(100,sizeof(unack_msg_node));
		return sfd;
	}
}
// Function to bind the socket to the port - similar to bind call
int r_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	return bind(sockfd,addr,addrlen);
}

// Function to send messages.
ssize_t r_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	// Message ids - taken as a static variable and updated for each message
	static unsigned int id_count = 0;
	// Initialise new buffer for appending id to the user message
	char *buffer = (char*)malloc(sizeof(char)*110);
	bzero(buffer,110);
	buffer[0] = 'M';	// indicate app msg, 'A' is used for ACK
	// Convert and append the msgid in network byte order
	int id_htonl = htonl(id_count);
	memcpy(&buffer[1],&id_htonl,sizeof(id_htonl));
	memcpy(&buffer[sizeof(id_count)+1],buf,len);
	// -----------------lock
	pthread_mutex_lock(&mutex);
	unack_msg_table[unack_msg_last+1].id = id_count;
	memcpy(unack_msg_table[unack_msg_last+1].message,buffer,len+sizeof(id_count)+1);
	unack_msg_table[unack_msg_last+1].msg_len = len+sizeof(id_count)+1;
	unack_msg_table[unack_msg_last+1].dest_addr = *(struct sockaddr_in *)dest_addr;
	gettimeofday(&unack_msg_table[unack_msg_last+1].tv,NULL);
	id_count++;			// increment message id
	unack_msg_last++;	// increment index of unacknowledge message table
	printf("Transmit\n");
	// Make the udp send call
	int nbytes_sent = sendto(sockfd,buffer,len+sizeof(id_count)+1,flags,dest_addr,addrlen);
	num_tranmissions++;		// Increment number of sendto call count
	// ------------------unlock
	pthread_mutex_unlock(&mutex);
	if(nbytes_sent < 0){
		perror("Unable to send");
	}
	// Return the number of bytes send -> similar to sendto call
	return nbytes_sent;
}

// Function to read data from the receive buffer
ssize_t r_recvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen)
{
	//------------lock
	pthread_mutex_lock(&mutex);
	// If the buffer is empty
	while(front==rear){
		// ----------- temp unlock
		pthread_mutex_unlock(&mutex);
		sleep(1);	// Sleep for 1 sec and then check again
		//------------ relock
		pthread_mutex_lock(&mutex);
	}
	// If buffer is non-empty
	// Deteremine the size of message to read - need to read either asked size len, or the message 
	// length actually present, whichever is minimum
	int size = min(len,receive_buffer[front].msg_len);
	// Copy the message into the buf present, passed by the user
	memcpy(buf,receive_buffer[front].message,size);
	// Populate the src address field of the message sender
	*src_addr = *(struct sockaddr *)&receive_buffer[front].source_addr;
	// Populate the addrlen field from the sender sockaddr_in structure
	*addrlen = sizeof(receive_buffer[front].source_addr);
	front = (front+1)%100;		// Update the value of the buffer index.
	// ------------unlock
	pthread_mutex_unlock(&mutex);
	// Return the size of message returned
	return size;
}
// Function for thread-X, takes the sockfd value as parameter.
void* thread_X(void *param)
{
	// Typecase the provided parameter into sockfd and free the pointer
	int sockfd = *((int *)param);
	free(param);
	// Required for select call
	fd_set readfds;
	int nfds = sockfd+1;
	// Populate the timeout value
	struct timeval tv;
	tv.tv_sec = TIMEOUT_T_SEC;
	tv.tv_usec = TIMEOUT_T_USEC;
	int ret_val;
	while(1){
		// Wait on select call
		FD_ZERO(&readfds);
		FD_SET(sockfd,&readfds);
		ret_val = select(nfds,&readfds,NULL,NULL,&tv);
		// If select comes out due to timeout
		if(ret_val==0){
			// Retransmit expired messages and wait for another timeout
			HandleRetransmit(sockfd);
			tv.tv_sec = TIMEOUT_T_SEC;
			tv.tv_usec = TIMEOUT_T_USEC;
		}
		// If some message is received
		else{
			if(FD_ISSET(sockfd,&readfds)){
				char buffer[110];
				bzero(buffer,110);
				struct sockaddr_in src_addr;
				int len;
				len = sizeof(src_addr);
				// Receive message
				int msg_len = recvfrom(sockfd,buffer,110,0,(struct sockaddr *)&src_addr,&len);
				// Compute probability for dropping
				if(dropMessage(DROP_PROB)==0){
					// If not dropping, handle the received message
					HandleReceive(sockfd,buffer,(const struct sockaddr *)&src_addr,msg_len);
				}
				else{
					// Else do nothing
					printf("Dropping message\n");
				}
			}
		}
	}
}	
// Function to handle retransmission of expired, unacknowledged messages
void HandleRetransmit(int sockfd)
{
	int i;
	struct timeval curr_time;
	gettimeofday(&curr_time,NULL);
	//----------------------lock
	pthread_mutex_lock(&mutex);
	// Loop over the unacknowledged messages.
	for(i=0;i<=unack_msg_last;i++){
		// If the message has timed out,
		if(curr_time.tv_sec-unack_msg_table[i].tv.tv_sec>=TIMEOUT_T_SEC){
			// Retransmit the message
			if(sendto(sockfd,unack_msg_table[i].message,unack_msg_table[i].msg_len,0,(const struct sockaddr *)&unack_msg_table[i].dest_addr,sizeof(unack_msg_table[i].dest_addr))<0){
				perror("Unable to send");
				exit(1);
			}
			// Update the time for the message
			unack_msg_table[i].tv.tv_sec = curr_time.tv_sec;
			unack_msg_table[i].tv.tv_usec = curr_time.tv_usec;
			num_tranmissions++;
		}
	}
	pthread_mutex_unlock(&mutex);
	//----------------------unlock
}
// Function to handle the message received by thread-X
void HandleReceive(int sockfd,char* buffer,const struct sockaddr * src_addr,int msg_len)
{
	// If the message is an application message
	if(buffer[0] == 'M'){
		HandleAppMsgRecv(sockfd,buffer,(const struct sockaddr *)src_addr,msg_len);
	}
	// If the message is an acknowledgemet
	else{
		HandleACKMsgRecv(buffer);
	}
}
// Function to handle acknowledgement messages
void HandleACKMsgRecv(char* buffer)
{
	int i;
	int id;
	memcpy(&id,&buffer[1],sizeof(id));
	id = ntohl(id);
	//----------------------lock
	pthread_mutex_lock(&mutex);
	for(i=0;i<=unack_msg_last;i++){
		if(unack_msg_table[i].id==id){
			printf("Received ack for message %d\n",id);
			// Swap the message to be deleted with the last message
			unack_msg_table[i] = unack_msg_table[unack_msg_last];
			// Decrement the last message, effectively deleting the acknowledged message
			unack_msg_last--;
			break;
		}
	}
	//----------------------unlock
	pthread_mutex_unlock(&mutex);
}

// Handle application message
void HandleAppMsgRecv(int sockfd,char* buffer,const struct sockaddr *src_addr,int msg_len)
{
	int i;
	int id;
	// Copy the id recieved
	memcpy(&id,&buffer[1],sizeof(id));
	id = ntohl(id);
	//---------------------lock
	pthread_mutex_lock(&mutex);
	for(i=0;i<msg_count;i++){
		// If the message is a duplicate, only send ack
		if(received_msg_id_table[i] == id){
			sendAck(id,sockfd,(const struct sockaddr *)src_addr);
			//------------------------------unlock
			pthread_mutex_unlock(&mutex);
			return;
		}
	}
	// If it is not a duplicate, copy the message to the receive buffer
	memcpy(receive_buffer[rear].message,&buffer[sizeof(id)+1],msg_len);
	receive_buffer[rear].source_addr = *(struct sockaddr_in *)src_addr;
	receive_buffer[rear].msg_len = msg_len;
	received_msg_id_table[msg_count] = id;
	rear = (rear+1)%100;
	msg_count++;
	pthread_mutex_unlock(&mutex);
	//------------------------------unlock
	// Send ack for the received message
	sendAck(id,sockfd,(const struct sockaddr *)src_addr);
}
//	Function to send acknowledgement for the message with given id
void sendAck(int id, int sockfd, const struct sockaddr *dest_addr)
{
	char buffer[sizeof(int)+1];
	buffer[0] = 'A';		// Indicate message as an ACK
	int id_htonl = htonl(id);
	memcpy(&buffer[1],&id_htonl,sizeof(id_htonl));
	// call to udp send for sending the ack
	if(sendto(sockfd,buffer,sizeof(id_htonl)+1,0,dest_addr,sizeof(*dest_addr))<0){
		perror("Unable to send");
		exit(1);
	}
}
// Function to find minimum of two numbers
int min(int a,int b)
{
	return a<b?a:b;
}

// Function to determine when to drop a message based on a probability measure
int dropMessage(float p)
{
	float rand_num = (float)rand()/((float)RAND_MAX+1);
	return rand_num<p;
}

// Function to close the socket
int r_close(int sfd)
{
	//----------------------lock
	pthread_mutex_lock(&mutex);
	// Check if there is any unacknowledged message left
	while(unack_msg_last>=0){
		pthread_mutex_unlock(&mutex);
		sleep(1);
		pthread_mutex_lock(&mutex);
	}
	// Print the number of transmissions before closing the socket
	printf("Total transmissions happened: %d\n",num_tranmissions);
	//------------------------------unlock
	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&mutex);
	free(received_msg_id_table);
	free(receive_buffer);
	free(unack_msg_table);
	pthread_cancel(tid);
	return close(sfd);
}