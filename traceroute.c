#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <netinet/ip.h> 
#include <netinet/ip_icmp.h>
#include <netinet/udp.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netdb.h> 
#include <sys/time.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUFSIZE 1500

 /* UDP data structure */ 
 struct outdata 
 { 
	int outdata_seq;	     /*sequence number*/
	int outdata_ttl;         /*TTL value*/
	struct timeval outdata_tv;	/*Packet transmittal time*/

 }; 

char recvbuf[BUFSIZE];
char sendbuf[BUFSIZE];


int sendfd ; /*Descriptor of the socket for sending UDP */
int recvfd ; /*Descriptor of the raw socket for recieving  ICMP messgaes */


/*the sockaddr() structure for sending a packet*/
struct sockaddr_in sasend;
/*the sockaddr() structure for binding the source packet*/
struct sockaddr_in sabind;
/*the sockaddr() structure for recieving a packet*/
struct sockaddr_in sarecv;
/*the last sockaddr() structure for recieving a packet*/
struct sockaddr_in salast;

int sport;
int dport;

int ttl;
int probe;
int max_ttl=30; /*maximum value for TTL field*/
int nprobes=3;  /*Number of probing packets*/
int dport =32768+66;/*first destenation port*/
/*Length of the UDP data field*/
int datalen = sizeof(struct outdata);

/*Function prototypes*/
void tv_sub(struct timeval*,struct timeval*);
int packet_ok(int,struct timeval*);
/**************************************/
/*The main() function******************/

int main(int argc,char *argv[])
{
int seq; 
int code; 
int done;
double rtt; 
struct hostent *hp; 
struct outdata *outdata; 
struct timeval tvrecv; 
const int on = 1;
int ret;

if (argc != 2)
{ 
fprintf(stderr, "Usage: %s <hostname>\n", argv[1]);
 exit(-1); 
} 
 hp = gethostbyname(argv[1]);
 if (hp == NULL)
{ 
  perror("gethostbyname() failed"); 
  exit(-1);
}  
if ((recvfd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) 
{ 
	perror("socket() failed");
	exit(-1);
}

/* Restoring the initial rights */ 
/*setuid(getuid());*/ 
sendfd = socket(PF_INET, SOCK_DGRAM, 0);
if (sendfd < 0)
{ 
perror("socket() failed"); 
exit(-1);
} 


sport = (getpid() & 0xffff) | 0x8000; /* The UDP source port number */ 
 bzero(&sasend, sizeof(sasend)); 
 sasend.sin_family = AF_INET; 
 sasend.sin_addr= *((struct in_addr *) hp->h_addr);
 
 sabind.sin_family = AF_INET;
 sabind.sin_port = htons(sport);
 if (bind(sendfd, (struct sockaddr *)&sabind, sizeof(sabind)) != 0)
	 perror("bind() failed"); 
 
	seq = 0; 
	done = 0; 
 
for (ttl = 1; ttl <= max_ttl && done ==0; ttl++)
{ 
  setsockopt(sendfd, SOL_IP, IP_TTL, &ttl, sizeof(int)); 
  bzero(&salast, sizeof(salast));
  
  printf("%2d ",ttl);
  fflush(stdout);  
  for (probe = 0; probe < nprobes; probe++)
  {
	  outdata = (struct outdata *)sendbuf;
	  outdata->outdata_seq = ++seq;
	  outdata->outdata_ttl = ttl; 
	  gettimeofday(&outdata->outdata_tv, NULL); 
	  sasend.sin_port = htons(dport + seq); 
	  if (sendto(sendfd, sendbuf, datalen, 0, (struct sockaddr *)&sasend, sizeof(sasend)) < 0)
	  { 
        printf("sendto errno %d\n\r",errno);
		perror("sendto() failed");
		exit(-1);
	  }
	  if ( (code = packet_ok(seq, &tvrecv)) == -3) 
	  {
		  printf(" *"); /* The wait time expired; no answer. */ 
	  }
	  else 
	  { 
		if (memcmp(&sarecv.sin_addr, &salast.sin_addr, sizeof(sarecv.sin_addr)) != 0) 
		{ 
			if ( (hp=gethostbyaddr(&sarecv.sin_addr, sizeof(sarecv.sin_addr), sarecv.sin_family)) != 0)
				printf(" %s (%s)", inet_ntoa(sarecv.sin_addr), hp->h_name); 
			else
				printf(" %s", inet_ntoa(sarecv.sin_addr)); 
			memcpy(&salast.sin_addr, &sarecv.sin_addr, sizeof(salast.sin_addr));
		} 
		tv_sub(&tvrecv, &outdata->outdata_tv); 
		rtt = tvrecv.tv_sec * 1000.0 + tvrecv.tv_usec / 1000.0; 
		printf(" %.3f ms", rtt);
		if (code == -1) 
			++done;
	  }
      fflush(stdout);
	}
	printf("\n");
}
return 0;
}
 /* _________________________________________________________________ */ 
 /* Parsing a received packet */
 /* */ 
 /* The function returns: */ 
 /* -3 when the wait time expires. */ 
 /* -2 when an ICMP "time exceeded in transit" message is received; */ 
 /* the program continues executing. */
 /* -1 when an ICMP "port unreachable" message is received; */
 /* the program terminates execution. */
 /* _________________________________________________________________*/
 
int packet_ok(int  seq, struct timeval *tv)
 {
	int n;
	int len; 
	int hlen1; 
	int hlen2; 
	struct ip *ip; 
	struct ip *hip; 
	struct icmp *icmp; 
	struct udphdr *udp; 
	fd_set fds; 
	struct timeval wait; 
	wait.tv_sec = 4 ;/* Waiting for a reply for 4 seconds, the longest */ 
	wait.tv_usec = 0;
	for (;;)
	{
		len = sizeof(sarecv); 
		FD_ZERO(&fds); 
		FD_SET(recvfd, &fds);
		if (select(recvfd + 1, &fds, NULL, NULL, &wait) > 0)
        {			
			n = recvfrom(recvfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&sarecv, &len);
		}
		else if (!FD_ISSET(recvfd, &fds))
		{
			return (-3); 
		}
		else
		{
			perror("recvfrom() failed");
		}		

		gettimeofday(tv, NULL);
			
		ip=(struct ip *) recvbuf; /*start of the IP header*/
		hlen1=ip->ip_hl<<2;       /*Length of the IP header*/
		
		/* Start of the ICMP header */
		icmp = (struct icmp *) (recvbuf + hlen1);
		/* Start of the saved IP header */
		hip = (struct ip *) (recvbuf + hlen1 + 8);
		/* Length of the saved IP header */ 
		hlen2 = hip->ip_hl << 2; 
		/* Start of the saved UDP header */ 
		udp = (struct udphdr *) (recvbuf + hlen1 + 8 + hlen2);
		
		if (icmp->icmp_type == ICMP_TIMXCEED &&	
			icmp->icmp_code ==ICMP_TIMXCEED_INTRANS)
		{ 
			if (hip->ip_p ==IPPROTO_UDP && 
				udp->source == htons(sport) && 
				udp->dest ==htons(dport + seq))
				{				
					return (-2);
				}
		}
		if (icmp->icmp_type== ICMP_UNREACH) 
		{ 
			if(hip->ip_p == IPPROTO_UDP &&
				udp->source == htons(sport) && 
				udp->dest == htons(dport + seq))
			{ 
				if (icmp->icmp_code == ICMP_UNREACH_PORT)
				{
					return (-1);
				}					
			} 
		}
	}
 }
 
/* ________________________________________________ */
 /* Subtracting one timeval structure from another */ 
 /* ________________________________________________ */
 void tv_sub(struct timeval *out, struct timeval *in)
 {
	 if ( (out->tv_usec -= in->tv_usec) < 0)
	{ 
		out->tv_sec--; out->tv_usec += 1000000;
	} 
	out->tv_sec -= in->tv_sec;
 }		
