#include <stdio.h>
#include <ctype.h>	// isprint()
#include <unistd.h>	// sysconf()
#include <sys/poll.h>
#include <arpa/inet.h>	/* ntohs */
#include <sys/sysctl.h>	/* sysctl */
#include <ifaddrs.h>	/* getifaddrs */
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <netinet/ether.h>      /* ether_aton */
#include <linux/if_packet.h>    /* sockaddr_ll */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>
#include <time.h>

#define VIRT_HDR_1	10	/* length of a base vnet-hdr */
#define VIRT_HDR_2	12	/* length of the extenede vnet-hdr */
#define VIRT_HDR_MAX	VIRT_HDR_2
struct virt_header {
	uint8_t fields[VIRT_HDR_MAX];
};

#define MAX_BODYSIZE	16384
#define MAX_PKTSIZE	MAX_BODYSIZE	/* XXX: + IP_HDR + ETH_HDR */

/* counters to accumulate statistics */
struct my_ctrs {
	uint64_t pkts, bytes, events;
	struct timeval t;
};

static struct my_ctrs *ctr;

void* receiver(void *data)
{
	struct sockaddr_in source,dest;
	struct my_ctrs *ctr1 = (struct my_ctrs *) data;
	struct	nm_desc	*d;
	struct	pollfd fds;
	u_char	*buf;
	struct	nm_pkthdr h;
	struct ether_header *eptr;
	u_char *ptr;
	d = nm_open("netmap:em2", NULL, 0, 0);
	fds.fd	= NETMAP_FD(d);

	struct ifreq ifr;
	int raw_socket;
	memset (&ifr, 0, sizeof (struct ifreq));
	if ((raw_socket = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL))) < 1)
    {
        printf ("ERROR: Could not open socket, Got #?\n");
        exit (1);
    }
    /* Set the device to use */
  strcpy (ifr.ifr_name, "em2");
	ioctl(d,SIOCGIFFLAGS, &ifr);
	/* Get the current flags that the device might have */
	if (ioctl (raw_socket, SIOCGIFFLAGS, &ifr) == -1)
	{
			perror ("Error: Could not retrive the flags from the device.\n");
			exit (1);
	}
	ifr.ifr_flags |= IFF_PROMISC;
	if (ioctl (raw_socket, SIOCSIFFLAGS, &ifr) == -1)
	{
			perror ("Error: Could not set flag IFF_PROMISC");
			exit (1);
	}
	//fds.fd	= NETMAP_FD(d);
	fds.events = POLLIN;
	int i=1;
	for (;;) {
		poll(&fds,	1, -1);
		while ( (buf = nm_nextpkt(d, &h)) ) {
			//i = read(d, buf, sizeof(buf));
			//printf("hlen %d, buffer size %d, bytes read: %d\n", h.len, sizeof(buf));
			if (1) {
				eptr = (struct ether_header*)buf;
				//memset(&source, 0, sizeof(source));
				//source.sin_addr.s_addr = iph->saddr;
				//memset(&dest, 0, sizeof(dest));
				//dest.sin_addr.s_addr = iph->daddr;
				//printf("Protocol: %d, Source: %s, Dest: %s, Len: %d, Length Header: %d,  \n", iph->protocol,
											//inet_ntoa(source.sin_addr), inet_ntoa(dest.sin_addr), ntohs(iph->tot_len),h.len);

							    /* Do a couple of checks to see what packet type we have..*/
							    if (ntohs (eptr->ether_type) == ETHERTYPE_IP)
							    {
							        printf("Ethernet type hex:%x dec:%d is an IP packet\n",
							                ntohs(eptr->ether_type),
							                ntohs(eptr->ether_type));
							    }else  if (ntohs (eptr->ether_type) == ETHERTYPE_ARP)
							    {
							        printf("Ethernet type hex:%x dec:%d is an ARP packet\n",
							                ntohs(eptr->ether_type),
							                ntohs(eptr->ether_type));
							    }else {
							        printf("Ethernet type %x not IP", ntohs(eptr->ether_type));
							        exit(1);
							    }

							    /* copied from Steven's UNP */
							    ptr = eptr->ether_dhost;
							    i = ETHER_ADDR_LEN;
							    printf(" Destination Address:  ");
							    do{
							        printf("%s%x",(i == ETHER_ADDR_LEN) ? " " : ":",*ptr++);
							    }while(--i>0);
							    printf("\n");

							    ptr = eptr->ether_shost;
							    i = ETHER_ADDR_LEN;
							    printf(" Source Address:  ");
							    do{
							        printf("%s%x",(i == ETHER_ADDR_LEN) ? " " : ":",*ptr++);
							    }while(--i>0);
							    printf("\n");
				ctr1->pkts++;
				ctr1->bytes += h.len;
				ctr1->events++;
			}
		}
	}
	nm_close(d);
}

	int main( int arc, char **argv)
	{
		printf("STARTED");
		ctr = (struct my_ctrs*) malloc(sizeof(struct my_ctrs));
		ctr->pkts = 0;
		ctr->bytes = 0;
		ctr->events =0;
		pthread_t tid;
		if (pthread_create(&tid, NULL, &receiver, ctr) == -1) {
			D("Unable to create thread: %s", strerror(errno));
		}
		for(;;) {
			usleep(1000 * 1000);
			printf ("packets %d, bytes %d, events %d\n", ctr->pkts, ctr->bytes, ctr->events);
		}
		return 1;
	}
