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
#include <netinet/tcp.h>
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
#include <heap.c>
#include <inttypes.h>
#include <math.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define IPSIZ sizeof(char) * 15
#define UINT32SIZ sizeof(uint32_t)
#define IPRANGE pow(2,32)
#define TOPIPSIZ 10

/* counters to accumulate statistics */
struct my_ctrs {
	uint64_t pkts, bytes, events;
	uint32_t *hugebuf;
	struct timeval t;
};

static struct my_ctrs *ctr;
static uint32_t **heap;

void* receiver(void *data)
{
	struct sockaddr_in source,dest;
	struct my_ctrs *ctr1 = (struct my_ctrs *) data;
	struct	nm_desc	*d;
	struct	pollfd fds;
	u_char	*buf;
	u_char *ptr;
	d = nm_open("netmap:em2", NULL, 0, 0);
	fds.fd	= NETMAP_FD(d);

	char *source_ip, *dest_ip;
	struct	nm_pkthdr h;
	struct ether_header *eptr;
	struct ip *iph;
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
	ioctl(raw_socket,SIOCGIFFLAGS, &ifr);
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

	fds.events = POLLIN;
	int i=1;
	for (;;) {
		poll(&fds,	1, -1);
		while ( (buf = nm_nextpkt(d, &h)) ) {
			//i = read(d, buf, sizeof(buf));
			//printf("hlen %d, buffer size %d, bytes read: %d\n", h.len, sizeof(buf));
			if (1) {
				eptr = (struct ether_header*)buf;
				if (ntohs (eptr->ether_type) == ETHERTYPE_IP)
				{
					iph = (struct ip*)(buf + sizeof(struct ether_header));
					//printf("%u -> ",ctr1->hugebuf[iph->ip_src.s_addr] );
					++ctr1->hugebuf[iph->ip_src.s_addr];
					//printf("%u \n",ctr1->hugebuf[iph->ip_src.s_addr] );
					++ctr1->hugebuf[iph->ip_dst.s_addr];

				ctr1->pkts++;
				ctr1->bytes += h.len;
				ctr1->events++;
			}
		}
	}
	nm_close(d);
}

void* collector(void *data)
{
	struct my_ctrs *ctr1 = (struct my_ctrs *) data;

	while(1) {
			printf("calculating top ips \n");
			heap = initHeap(ctr1->hugebuf, TOPIPSIZ);
	}
}

int main( int arc, char **argv)
{
	printf("STARTED \n");

	//linux hugepages allocation for better memory performance
	int memsiz_gig = 18;
	uint64_t memsiz_byt = memsiz_gig * pow(2,30);
	int hugepaged = open("/mnt/hugetlbfs/akbabaipv4", O_CREAT|O_RDWR, 0600);
	if (hugepaged == -1)
	printf("ERROR while opening hugepage.\n" );
	else
	printf("Hugepage opened.\n");
	uint32_t *hugebuf = (uint32_t *)mmap(0, memsiz_byt, PROT_READ | PROT_WRITE, MAP_PRIVATE, hugepaged, 0);
	memset(hugebuf,0,memsiz_byt);
	if ((int)hugebuf == -1) {
		printf("ERROR with mmap \n");
			return -1;
	} else
		printf("%d Gigabytes of memory mapped.\n", memsiz_gig);
  memset(hugebuf,0,memsiz_byt);
	ctr = (struct my_ctrs*) malloc(sizeof(struct my_ctrs));
	ctr->hugebuf = hugebuf;
	ctr->pkts = 0;
	ctr->bytes = 0;
	ctr->events =0;

	pthread_t tid, tid2;
	if (pthread_create(&tid, NULL, &receiver, ctr) == -1) {
		D("Unable to create thread: %s", strerror(errno));
	}
	if (pthread_create(&tid2, NULL, &collector, ctr) == -1) {
		D("Unable to create thread: %s", strerror(errno));
	}

	char *ipstring;
	struct in_addr ipaddr;
	ipstring = malloc(IPSIZ);

	for(;;) {
	usleep(1000 * 1000);
	printf ("\npackets %u, bytes %u, events %u\n", ctr->pkts, ctr->bytes, ctr->events);
	printf("### TOP IPS\n");
	int i;
	for (i = 0; i < TOPIPSIZ; i++) {
		ipaddr.s_addr = (uint32_t)((heap[i] - ctr->hugebuf));
		//printf("# %d. IP uint32_t: %u - pointer: %p \n", i, ipaddr.s_addr, (heap[i] - ctr->hugebuf));
		memset(ipstring, 0, IPSIZ);
		memcpy(ipstring,inet_ntoa(ipaddr), IPSIZ);
		printf("#  %d. IP: %s Packets: %u\n", i, ipstring, *heap[i]);
	}
	printf("###\n");
}
	return 0;
}
