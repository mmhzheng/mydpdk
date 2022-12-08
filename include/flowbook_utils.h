#ifndef _FLOWBOOK_UTILS_H_
#define _FLOWBOOK_UTILS_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RTE_LOGTYPE_FLOWBOOK RTE_LOGTYPE_USER1

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256


#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024


#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

#define MAX_TIMER_PERIOD 86400 /* 1 day max */

#define CHECK_LINK_STATS_INTERVAL 100 /* 100ms */
#define MAX_CHECK_LINK_STATS_TIME 90 /* 9s (90 * 100ms) in total */

/* display usage */
static void
dcbook_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-P] [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -P : Enable promiscuous mode\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
	       "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n",
	       prgname);
}

static int
flowbook_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static unsigned int
flowbook_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
flowbook_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

/* Print out statistics on packets dropped */
// static void
// print_stats(void)
// {
// 	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
// 	unsigned portid;

// 	total_packets_dropped = 0;
// 	total_packets_tx = 0;
// 	total_packets_rx = 0;

// 	// const char clr[] = { 27, '[', '2', 'J', '\0' };
// 	// const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

// 	/* Clear screen and move to top left */
// 	// printf("%s%s", clr, topLeft);

// 	printf("\nPort statistics ====================================");

// 	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
// 		/* skip disabled ports */
// 		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
// 			continue;
// 		printf("\nStatistics for port %u ------------------------------"
// 			   "\nPackets sent: %24"PRIu64
// 			   "\nPackets received: %20"PRIu64
// 			   "\nPackets dropped: %21"PRIu64,
// 			   portid,
// 			   port_statistics[portid].tx,
// 			   port_statistics[portid].rx,
// 			   port_statistics[portid].dropped);

// 		total_packets_dropped += port_statistics[portid].dropped;
// 		total_packets_tx += port_statistics[portid].tx;
// 		total_packets_rx += port_statistics[portid].rx;
// 	}
// 	printf("\nAggregate statistics ==============================="
// 		   "\nTotal packets sent: %18"PRIu64
// 		   "\nTotal packets received: %14"PRIu64
// 		   "\nTotal packets dropped: %15"PRIu64,
// 		   total_packets_tx,
// 		   total_packets_rx,
// 		   total_packets_dropped);
// 	printf("\n====================================================\n");

// 	fflush(stdout);
// }

#endif