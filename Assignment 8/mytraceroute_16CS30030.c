// Standard Includes
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <linux/udp.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <netdb.h>

// macros for required values
#define PAYLOAD_LEN 52
#define BUFF_LEN sizeof(struct iphdr)+sizeof(struct udphdr)+PAYLOAD_LEN
#define PORT_NUM 32164
#define UDP_PORT 12345
#define ICMP_PORT 54321

// Function declarations
uint16_t ip_checksum(void* vdata,size_t length);
uint16_t udp_checksum(const void *buff, size_t len, in_addr_t src_addr, in_addr_t dest_addr);

int main(int argc, char* argv[])
{
	// Args check
	if(argc != 2){
		printf("Usage: ./mytraceroute <destination domain name>\n");
		exit(0);
	}
	// Calculate ip address
	char domain[BUFF_LEN];
	strcpy(domain,argv[1]);
	struct hostent *hp;
	hp = gethostbyname(domain);
	struct in_addr ip_addr;
	ip_addr = *(struct in_addr *)hp->h_addr_list[0];

	// Declare data structures
	printf("IP = %s\n\n",inet_ntoa(ip_addr));
	int icmpsfd, udpsfd;
	struct sockaddr_in saddr_icmp, saddr_udp, send_addr;
	struct sockaddr_in recvaddr;
	int saddr_icmp_len, saddr_udp_len,recvaddr_len;
	char buffer[BUFF_LEN];
	char recv_buffer[BUFF_LEN];
	char *payload,*recv_payload;
	clock_t time1,time2;

	// Create sockets
	udpsfd = socket(AF_INET,SOCK_RAW,IPPROTO_UDP);
	if(udpsfd<0){
		perror("Unable to create udp raw socket");
		exit(1);
	}
	saddr_udp.sin_family = AF_INET;
	saddr_udp.sin_addr.s_addr = INADDR_ANY;
	saddr_udp.sin_port = htons(UDP_PORT);
	// Specify that IP header will also be provided
	int enable = 1;
	setsockopt(udpsfd,IPPROTO_IP,IP_HDRINCL,&enable,sizeof(int));
	// Bind the udp socket
	if(bind(udpsfd,(const struct sockaddr *)&saddr_udp,sizeof(saddr_udp))<0){
		perror("Unable to bind udp socket");
		exit(1);
	}
	// create icmp socket
	icmpsfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
	if(icmpsfd<0){
		perror("Unable to create icmp socket");
		exit(1);
	}	
	saddr_icmp.sin_family = AF_INET;
	saddr_icmp.sin_port = htons(ICMP_PORT);
	saddr_icmp.sin_addr.s_addr = INADDR_ANY;
	// bind the icmp socket
	if(bind(icmpsfd,(struct sockaddr *)&saddr_icmp,sizeof(saddr_icmp)) < 0){
		perror("Bind icmp error");
		exit(1);
	}	
	int ttl = 1;
	struct iphdr *header_ip, recv_header_ip;
	struct udphdr *header_udp;
	struct icmphdr recv_header_icmp;

	send_addr.sin_family = AF_INET;
	send_addr.sin_port = htons(PORT_NUM);
	send_addr.sin_addr.s_addr = ip_addr.s_addr;
	// Fill the header - all values except ttl
	// ****** IP Header
	bzero(buffer,BUFF_LEN);
	header_ip = ((struct iphdr *)buffer);
	header_udp = ((struct udphdr *)(buffer+sizeof(struct iphdr)));
	header_ip->tos = 0;
	header_ip->version = 4;	// IPv4 version
	header_ip->ihl = 5;		// Header length
	header_ip->saddr = saddr_udp.sin_addr.s_addr;
	header_ip->daddr = send_addr.sin_addr.s_addr;
	header_ip->id = 0;	// automatically filled when 0
	header_ip->frag_off = 0;
	header_ip->protocol = IPPROTO_IP;	// for UDP protocol
	header_ip->tot_len = BUFF_LEN;	// Always filled in
	header_ip -> check = 0;			// Always filled in
	//********* UDP Header
	header_udp->dest = send_addr.sin_port;			// Destination port address
	header_udp->source = saddr_udp.sin_port;		// Source port address
	// Make some random payload
	payload = ((char *)(buffer + sizeof(struct iphdr) + sizeof(struct udphdr)));
	strcpy(payload,"Test");
	// Fill length and checksum
	header_udp->len = sizeof(struct udphdr)+PAYLOAD_LEN;
	header_udp->check = udp_checksum(buffer+sizeof(struct iphdr),sizeof(struct udphdr)+strlen(payload)+1,saddr_udp.sin_addr.s_addr,ip_addr.s_addr);
	printf("Hop Count\tIP Address\tResponse_time\n");
	while(ttl<=30){
		// Fill ip header
		header_ip->ttl = ttl;	// specify ttl 
		printf("%d\t\t",ttl);
		ttl++;
		int i;
		fd_set readfds;
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		int done = 0;
		for(i=0;i<3;i++){
			FD_ZERO(&readfds);
			FD_SET(icmpsfd,&readfds);			
			// Send the packet
			int nbytes_sent = sendto(udpsfd,buffer,BUFF_LEN,0,(const struct sockaddr *)&send_addr,sizeof(send_addr));
			// Start timer
			time1 = clock();
			if(nbytes_sent<0){
				perror("Unable to send");
				exit(1);
			}
			// Select waits on icmp socket and timeout
			int select_ret = select(icmpsfd+1,&readfds,0,0,&timeout);
			// If message is returned
			if(FD_ISSET(icmpsfd,&readfds)){
				recvaddr_len = sizeof(recvaddr);
				// Receive the message on the icmp socket
				recvfrom(icmpsfd,recv_buffer,BUFF_LEN,0,(struct sockaddr *)&recvaddr,&recvaddr_len);
				// Stop the timer
				time2 = clock();
				// Extract the headers
				recv_header_ip = *((struct iphdr *)recv_buffer);
				recv_header_icmp = *((struct icmphdr *)(recv_buffer+sizeof(recv_header_ip)));
				recv_payload = (char *)(recv_buffer + sizeof(struct iphdr)+sizeof(struct icmphdr));
				// If it is an icmp message with type Destination Unreachable - Final host reached
				if(recv_header_ip.protocol == 1 && recv_header_icmp.type == 3){
					done = 1;
					// Print the ip and time taken
					printf("%s\t",inet_ntoa(*(struct in_addr *)&recv_header_ip.saddr));
					printf("%f\n",(float)(time2-time1)/CLOCKS_PER_SEC);
					break;
				}
				// If it is an icmp message with type Time To Live expired - intermediate router
				else if(recv_header_ip.protocol == 1 && recv_header_icmp.type == 11){
					// Print ip address and time taken
					printf("%s\t",inet_ntoa(*(struct in_addr *)&recv_header_ip.saddr));
					printf("%f\n",(float)(time2-time1)/CLOCKS_PER_SEC);
					// Reset timer for select call
					timeout.tv_sec = 1;
					timeout.tv_usec = 0;
					break;
				}
				else{
					// not our packet. Ignore
				}
			}
			else{
				// If three trails failed, print *
				if(i==2){
					printf("*\t\t*\n");
				}
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
			}
		}
		// If all routers are found
		if(done)
			break;
	}
	close(udpsfd);
	close(icmpsfd);
}

// IP Checksum function -- Not needed as IP checksum is always filled by OS
uint16_t ip_checksum(void* vdata,size_t length) {
    // Cast the data pointer to one that can be indexed.
    char* data=(char*)vdata;

    // Initialise the accumulator.
    uint32_t acc=0xffff;

    // Handle complete 16-bit blocks.
    for (size_t i=0;i+1<length;i+=2) {
        uint16_t word;
        memcpy(&word,data+i,2);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Handle any partial block at the end of the data.
    if (length&1) {
        uint16_t word=0;
        memcpy(&word,data+length-1,1);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Return the checksum in network byte order.
    return htons(~acc);
}

// UDP Checksum function
uint16_t udp_checksum(const void *buff, size_t len, in_addr_t src_addr, in_addr_t dest_addr)
{
	const uint16_t *buf=buff;
	uint16_t *ip_src=(void *)&src_addr, *ip_dst=(void *)&dest_addr;
	uint32_t sum;
	size_t length=len;

	// Calculate the sum                                            //
	sum = 0;
	while (len > 1)
	{
	     sum += *buf++;
	     if (sum & 0x80000000)
	             sum = (sum & 0xFFFF) + (sum >> 16);
	     len -= 2;
	}

	if ( len & 1 )
	     // Add the padding if the packet lenght is odd          //
	     sum += *((uint8_t *)buf);

	// Add the pseudo-header                                        //
	sum += *(ip_src++);
	sum += *ip_src;

	sum += *(ip_dst++);
	sum += *ip_dst;

	sum += htons(IPPROTO_UDP);
	sum += htons(length);

	// Add the carries                                              //
	while (sum >> 16)
	     sum = (sum & 0xFFFF) + (sum >> 16);

	// Return the one's complement of sum                           //
	return ( (uint16_t)(~sum)  );
}