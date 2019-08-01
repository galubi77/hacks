#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <net/if.h> 
#include <linux/ip.h> 
#include <linux/tcp.h> 
#include <netdb.h> 
#include <sys/ioctl.h> 

#define DEVICE "eth0"


/* __________________________ */
 /* Calculating the checksum */ 
 /* __________________________ */ 
unsigned short in_cksum(unsigned short *addr, int len) 
{ 
	unsigned short result; 
	unsigned int sum = 0;
	/* Adding all 2-byte words */
	while (len > 1)
	{ 
		sum += *addr++; 
		len -= 2; 
	}
	/* If there is a byte left over, adding it to the sum */ 
	if (len == 1)
	{		
		sum += *(unsigned char*) addr; 
	}
	sum=(sum >> 16) + (sum & 0xFFFF); 
	/* Adding the carry */ 
	sum += (sum >> 16); 
	/* Adding the carry again */ 
	result = ~sum; 
	/* Inverting the result */
	return result;
	
}	
		/* __________________________________ */
		/* Assembling and sending a packet */
		/* __________________________________ */ 
void send_packet(int sd, unsigned short port, struct sockaddr_in source, struct hostent* hp)
{ 
	struct sockaddr_in servaddr; 
	struct tcphdr tcp_hdr; 
	/* Pseudo packet structure */ 
	struct pseudo_hdr 
	{ 
		unsigned int source_address; 
		unsigned int dest_address; 
		unsigned char place_holder; 
		unsigned char protocol; 
		unsigned short length; 
		struct tcphdr tcp;
	} pseudo_hdr; 
	
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_port = htons(port); 
	servaddr.sin_addr = *((struct in_addr *)hp->h_addr);
	/* Filling the TCP header */
	tcp_hdr.source = getpid(); 
	tcp_hdr.dest = htons(port); 
	tcp_hdr.seq = htons(getpid() + port); 
	tcp_hdr.ack_seq = 0;
	tcp_hdr.res1 = 0 ;
	tcp_hdr.doff = 5;
	tcp_hdr.fin = 0;
	tcp_hdr.syn = 1;
	tcp_hdr.rst = 0;
	tcp_hdr.psh = 0;
	tcp_hdr.ack = 0;
	tcp_hdr.urg = 0;
	tcp_hdr.ece = 0;
	tcp_hdr.cwr = 0;
	tcp_hdr.window = htons(128); 
	tcp_hdr.check = 0; 
	tcp_hdr.urg_ptr =0;
	/* Filling the pseudo header */ 
	pseudo_hdr.source_address = source.sin_addr.s_addr; 
	pseudo_hdr.dest_address = servaddr.sin_addr.s_addr; 
	pseudo_hdr.place_holder= 0; 
	pseudo_hdr.protocol= IPPROTO_TCP; 
	pseudo_hdr.length = htons(sizeof(struct tcphdr));
	/* Pasting the filled TCP header after the pseudo header */ 
	bcopy(&tcp_hdr, &pseudo_hdr.tcp, sizeof(struct tcphdr)); 
	/* Calculating the TCP header checksum */ 
	tcp_hdr.check = in_cksum((unsigned short *)&pseudo_hdr, sizeof(struct pseudo_hdr)); 
	/* Sending the TCP packet */ 
	if (sendto(sd,&tcp_hdr,sizeof(struct tcphdr),0,(struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
	{
			   perror("sendto() failed");
	}
}
	 /* _______________________________________________________ */
	 /* Receiving the reply packet and checking the flags */
	 /* _______________________________________________________ */ 
int recv_packet(int sd)
{ 
	char recvbuf[1500]; 
	struct tcphdr *tcphdr = (struct tcphdr *) (recvbuf + sizeof(struct iphdr));
	while(1)
	{ 
		if (recv(sd,recvbuf, sizeof(recvbuf), 0) < 0)
		{
			perror("recv() failed"); 
		}
		if (tcphdr->dest == getpid())
		{
			if(tcphdr->syn == 1 && tcphdr->ack == 1) 
			{	
				return 1; 
			}
			else
			{
				return 0;
			}
		}
	}
}

/* _____________________ */
/* The main() function */ 
/* _____________________ */ 
int main(int argc, char *argv[])
{ 
	int sd; 
	struct ifreq *ifr; 
	struct hostent* hp; 
	int port, portlow, porthigh; 
	unsigned int dest; 
	struct sockaddr_in source; 
	struct servent* srvport; 
	
	if (argc != 4)
	{ 
		fprintf(stderr, "Usage: %s <address> <portlow> <porthigh>\n", argv[0]); 
		exit(-1);
	}		
		
	hp = gethostbyname(argv[1]); 
	if (hp == NULL) 
	{ 
		herror("gethostbyname() failed"); 
		exit(-1); 
	} 
	portlow = atoi(argv[2]); 
	porthigh = atoi(argv[3]); 
	if( (sd = socket(PF_INET, SOCK_RAW, IPPROTO_TCP)) < 0)
	{ 
		perror("socket() failed"); 
		exit(-1);
	}
	
	fprintf(stderr, "Running scan...\n"); 
	/* Obtaining the IP address of the interface and placing it into the source address structure */ 
	sprintf(ifr->ifr_name, "%s", DEVICE); 
	ioctl(sd, SIOCGIFADDR, ifr); 
	memcpy((char*)&source, (char*)&(ifr->ifr_addr), sizeof(struct sockaddr));
	for (port = portlow; port <= porthigh; port++)
	{ 
		send_packet(sd, port, source, hp);
		if(recv_packet(sd)==1)
		{
			srvport=getservbyport(htons(port),"tcp");
			if(srvport==NULL)
			{
				printf("Open: %d unknown\n",port);
			}
			else
			{
				printf("Open: %d (%s)\n",port,srvport->s_name);
			}
			fflush(stdout);
		}
	}
	close(sd);
	return 0;
}
	 
	 
	 
	 
	 