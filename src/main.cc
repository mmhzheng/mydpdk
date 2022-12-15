/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2021 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_vect.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_ethdev.h>

#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>

#include "flowbook_hdr.h"
// #include "flowbook_utils.h"
#include "flowbook_table.h"

#define RTE_LOGTYPE_FLOWBOOK RTE_LOGTYPE_USER1

#define MAX_TX_QUEUE_PER_PORT RTE_MAX_LCORE
#define MAX_RX_QUEUE_PER_PORT 128

#define MAX_LCORE_PARAMS 1024

#define MAX_PKT_BURST     32

#define MEMPOOL_CACHE_SIZE 256
#define MAX_RX_QUEUE_PER_LCORE 16

#define NB_SOCKETS        8

/* Hash parameters. */
#ifdef RTE_ARCH_64
/* default to 4 million hash entries (approx) */
#define L3FWD_HASH_ENTRIES		(1024*1024*4)
#else
/* 32-bit has less address-space for hugepage memory, limit to 1M entries */
#define L3FWD_HASH_ENTRIES		(1024*1024*1)
#endif
#define HASH_ENTRY_NUMBER_DEFAULT	16

/*
 * Configurable number of RX/TX ring descriptors
 * In orther words, rx/tx queue size.
 */
#define RX_DESC_DEFAULT 2048
#define TX_DESC_DEFAULT    0
uint16_t nb_rxd = 2048;
uint16_t nb_txd = 0;   // Need not send packets.

/**< Ports set in promiscuous mode off by default. */
static int promiscuous_on;

/**
 * Global variables.
 */
static int numa_on = 1; /**< NUMA is enabled by default. */
static int per_port_pool = false; /**< Use separate buffer pools per port. */

volatile bool force_quit;

/* ethernet addresses of ports */
struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
/* Per-port statistics struct */
struct flowbook_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;
struct flowbook_port_statistics port_statistics[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
uint32_t enabled_port_mask;

uint32_t table_entry_number = 1024;

struct lcore_rx_queue {
	uint16_t port_id;
	uint8_t queue_id;
} __rte_cache_aligned;

/* Lcore conf */
struct lcore_conf {
	uint16_t n_rx_queue;
	struct lcore_rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
	uint16_t n_tx_port;
	uint16_t tx_port_id[RTE_MAX_ETHPORTS];
	uint16_t tx_queue_id[RTE_MAX_ETHPORTS];
	// struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct lcore_conf lcore_conf[RTE_MAX_LCORE];
struct lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;
static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];
static struct lcore_params lcore_params_array_default[] = {
// port, queue, lcore
	{0, 0, 2},
	{0, 1, 2},
	{0, 2, 2},
	{1, 0, 2},
	{1, 1, 2},
	{1, 2, 2},
	{2, 0, 2},
	{3, 0, 3},
	{3, 1, 3},
};

static struct lcore_params * lcore_params = lcore_params_array_default;
static uint16_t nb_lcore_params = sizeof(lcore_params_array_default) /
				sizeof(lcore_params_array_default[0]);

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
		.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM,
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,           // Randomly (determined by hardware)
			.rss_hf = RTE_ETH_RSS_UDP, // use udp tuple to classify queue.  
		},
	},
};

uint32_t max_pkt_len;

static struct rte_mempool *pktmbuf_pool[RTE_MAX_ETHPORTS][NB_SOCKETS];

static int
check_lcore_params(void)
{
	uint8_t queue, lcore;
	uint16_t i;
	int socketid;

	for (i = 0; i < nb_lcore_params; ++i) {
		queue = lcore_params[i].queue_id;
		if (queue >= MAX_RX_QUEUE_PER_PORT) {
			printf("invalid queue number: %hhu\n", queue);
			return -1;
		}
		lcore = lcore_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			printf("error: lcore %hhu is not enabled in lcore mask\n", lcore);
			return -1;
		}
		if ((socketid = rte_lcore_to_socket_id(lcore) != 0) &&
			(numa_on == 0)) {
			printf("warning: lcore %hhu is on socket %d with numa off \n",
				lcore, socketid);
		}
	}
	return 0;
}

static int
check_port_config(void)
{
	uint16_t portid;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		portid = lcore_params[i].port_id;
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("port %u is not enabled in port mask\n", portid);
			return -1;
		}
		if (!rte_eth_dev_is_valid_port(portid)) {
			printf("port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static uint8_t
get_port_n_rx_queues(const uint16_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port) {
			if (lcore_params[i].queue_id == queue+1)
				queue = lcore_params[i].queue_id;
			else
				rte_exit(EXIT_FAILURE, "queue ids of the port %d must be"
						" in sequence and must start with 0\n",
						lcore_params[i].port_id);
		}
	}
	return (uint8_t)(++queue);
}

static int
init_lcore_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t lcore;

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		nb_rx_queue = lcore_conf[lcore].n_rx_queue;
		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			printf("error: too many queues (%u) for lcore: %u\n",
				(unsigned)nb_rx_queue + 1, (unsigned)lcore);
			return -1;
		} else {
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
				lcore_params[i].port_id;
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
				lcore_params[i].queue_id;
			lcore_conf[lcore].n_rx_queue++;
		}
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	fprintf(stderr, "%s [EAL options] --"
		" -p PORTMASK"
		" [-P]"
		" --config (port,queue,lcore)[,(port,queue,lcore)]"
		" [--rx-queue-size NPKTS]"
		" [--tx-queue-size NPKTS]"
		" [--max-pkt-len PKTLEN]"
		" [--no-numa]"
		" [--hash-entry-num]\n\n"

		"  -p PORTMASK: Hexadecimal bitmask of ports to configure\n"
		"  -P : Enable promiscuous mode\n"
		"  --config (port,queue,lcore): Rx queue configuration\n"
		"  --rx-queue-size NPKTS: Rx queue size in decimal\n"
		"            Default: %d\n"
		"  --tx-queue-size NPKTS: Tx queue size in decimal\n"
		"            Default: %d\n"
		"  --max-pkt-len PKTLEN: maximum packet length in decimal (64-9600)\n"
		"  --no-numa: Disable numa awareness\n"
		"  --table-entry-num: Specify the hash entry number in hexadecimal to be setup\n",
		prgname, RX_DESC_DEFAULT, TX_DESC_DEFAULT);
}

static int
parse_max_pkt_len(const char *pktlen)
{
	char *end = NULL;
	unsigned long len;

	/* parse decimal string */
	len = strtoul(pktlen, &end, 10);
	if ((pktlen[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (len == 0)
		return -1;

	return len;
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static int
parse_table_entry_number(const char *hash_entry_num)
{
	char *end = NULL;
	unsigned long hash_en;
	/* parse hexadecimal string */
	hash_en = strtoul(hash_entry_num, &end, 16);
	if ((hash_entry_num[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (hash_en == 0)
		return -1;

	return hash_en;
}

static int
parse_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_lcore_params = 0;

	while ((p = strchr(p0,'(')) != NULL) {
		++p;
		if((p0 = strchr(p,')')) == NULL)
			return -1;

		size = p0 - p;
		if(size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++){
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of lcore params: %hu\n",
				nb_lcore_params);
			return -1;
		}
		lcore_params_array[nb_lcore_params].port_id =
			(uint8_t)int_fld[FLD_PORT];
		lcore_params_array[nb_lcore_params].queue_id =
			(uint8_t)int_fld[FLD_QUEUE];
		lcore_params_array[nb_lcore_params].lcore_id =
			(uint8_t)int_fld[FLD_LCORE];
		++nb_lcore_params;
	}
	lcore_params = lcore_params_array;
	return 0;
}

static void
parse_queue_size(const char *queue_size_arg, uint16_t *queue_size, int rx)
{
	char *end = NULL;
	unsigned long value;

	/* parse decimal string */
	value = strtoul(queue_size_arg, &end, 10);
	if ((queue_size_arg[0] == '\0') || (end == NULL) ||
		(*end != '\0') || (value == 0)) {
		if (rx == 1)
			rte_exit(EXIT_FAILURE, "Invalid rx-queue-size\n");
		else
			rte_exit(EXIT_FAILURE, "Invalid tx-queue-size\n");

		return;
	}

	if (value > UINT16_MAX) {
		if (rx == 1)
			rte_exit(EXIT_FAILURE, "rx-queue-size %lu > %d\n",
				value, UINT16_MAX);
		else
			rte_exit(EXIT_FAILURE, "tx-queue-size %lu > %d\n",
				value, UINT16_MAX);

		return;
	}

	*queue_size = value;
}

#define MAX_JUMBO_PKT_LEN  9600

static const char short_options[] =
	"p:"  /* portmask */
	"P"   /* promiscuous */
	;

#define CMD_LINE_OPT_CONFIG "config"
#define CMD_LINE_OPT_RX_QUEUE_SIZE "rx-queue-size"
#define CMD_LINE_OPT_TX_QUEUE_SIZE "tx-queue-size"
#define CMD_LINE_OPT_NO_NUMA "no-numa"
#define CMD_LINE_OPT_MAX_PKT_LEN "max-pkt-len"
#define CMD_LINE_OPT_TABLE_ENTRY_NUM "table-entry-num"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_LINE_OPT_CONFIG_NUM,
	CMD_LINE_OPT_RX_QUEUE_SIZE_NUM,
	CMD_LINE_OPT_TX_QUEUE_SIZE_NUM,
	CMD_LINE_OPT_NO_NUMA_NUM,
	CMD_LINE_OPT_MAX_PKT_LEN_NUM,
	CMD_LINE_OPT_TABLE_ENTRY_NUM_NUM
};

static const struct option lgopts[] = {
	{CMD_LINE_OPT_CONFIG, 1, 0, CMD_LINE_OPT_CONFIG_NUM},
	{CMD_LINE_OPT_RX_QUEUE_SIZE, 1, 0, CMD_LINE_OPT_RX_QUEUE_SIZE_NUM},
	{CMD_LINE_OPT_TX_QUEUE_SIZE, 1, 0, CMD_LINE_OPT_TX_QUEUE_SIZE_NUM},
	{CMD_LINE_OPT_NO_NUMA, 0, 0, CMD_LINE_OPT_NO_NUMA_NUM},
	{CMD_LINE_OPT_MAX_PKT_LEN, 1, 0, CMD_LINE_OPT_MAX_PKT_LEN_NUM},
	{CMD_LINE_OPT_TABLE_ENTRY_NUM, 1, 0, CMD_LINE_OPT_TABLE_ENTRY_NUM_NUM},
	{NULL, 0, 0, 0}
};

/*
 * This expression is used to calculate the number of mbufs needed
 * depending on user input, taking  into account memory for rx and
 * tx hardware rings, cache per lcore and mtable per port per lcore.
 * RTE_MAX is used to ensure that NB_MBUF never goes below a minimum
 * value of 8192
 */
#define NB_MBUF(nports) RTE_MAX(	\
	(nports*nb_rx_queue*nb_rxd +		\
	nports*nb_lcores*MAX_PKT_BURST +	\
	nports*n_tx_queue*nb_txd +		\
	nb_lcores*MEMPOOL_CACHE_SIZE),		\
	(unsigned)8192)

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	argvopt = argv;

	/* Error or normal output strings. */
	while ((opt = getopt_long(argc, argvopt, short_options,
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				fprintf(stderr, "Invalid portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;

		case 'P':
			promiscuous_on = 1;
			break;

		/* long options */
		case CMD_LINE_OPT_CONFIG_NUM:
			ret = parse_config(optarg);
			if (ret) {
				fprintf(stderr, "Invalid config\n");
				print_usage(prgname);
				return -1;
			}
			break;

		case CMD_LINE_OPT_RX_QUEUE_SIZE_NUM:
			parse_queue_size(optarg, &nb_rxd, 1);
			break;

		case CMD_LINE_OPT_TX_QUEUE_SIZE_NUM:
			parse_queue_size(optarg, &nb_txd, 0);
			break;

		case CMD_LINE_OPT_NO_NUMA_NUM:
			numa_on = 0;
			break;

		case CMD_LINE_OPT_MAX_PKT_LEN_NUM:
			max_pkt_len = parse_max_pkt_len(optarg);
			break;

		case CMD_LINE_OPT_TABLE_ENTRY_NUM_NUM:
			ret = parse_table_entry_number(optarg);
			if ((ret > 0) && (ret <= L3FWD_HASH_ENTRIES)) {
				table_entry_number = ret;
			} else {
				fprintf(stderr, "invalid hash entry number\n");
				print_usage(prgname);
				return -1;
			}
			break;

		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}


/**
 * Setup memory for portid.
 * if portid == 0, all ports will use a shared mbuf pool.
*/
int
init_mem(uint16_t portid, unsigned int nb_mbuf)
{
	int socketid;
	unsigned lcore_id;
	char s[64];

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socketid = rte_lcore_to_socket_id(lcore_id);
		else
			socketid = 0;

		if (socketid >= NB_SOCKETS) {
			rte_exit(EXIT_FAILURE,
				"Socket %d of lcore %u is out of range %d\n",
				socketid, lcore_id, NB_SOCKETS);
		}

		if (pktmbuf_pool[portid][socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d:%d",
				 portid, socketid);
			pktmbuf_pool[portid][socketid] =
				rte_pktmbuf_pool_create(s, nb_mbuf,
					MEMPOOL_CACHE_SIZE, 0,
					RTE_MBUF_DEFAULT_BUF_SIZE, socketid);
			if (pktmbuf_pool[portid][socketid] == NULL)
				rte_exit(EXIT_FAILURE,
					"Cannot init mbuf pool on socket %d\n",
					socketid);
			else
				printf("Allocated mbuf pool on socket %d\n",
					socketid);
		}
	}
	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", portid,
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}


static uint32_t
eth_dev_get_overhead_len(uint32_t max_rx_pktlen, uint16_t max_mtu)
{
	uint32_t overhead_len;

	if (max_mtu != UINT16_MAX && max_rx_pktlen > max_mtu)
		overhead_len = max_rx_pktlen - max_mtu;
	else
		overhead_len = RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	return overhead_len;
}

int
config_port_max_pkt_len(struct rte_eth_conf *conf,
		struct rte_eth_dev_info *dev_info)
{
	uint32_t overhead_len;

	if (max_pkt_len == 0)
		return 0;

	if (max_pkt_len < RTE_ETHER_MIN_LEN || max_pkt_len > MAX_JUMBO_PKT_LEN)
		return -1;

	overhead_len = eth_dev_get_overhead_len(dev_info->max_rx_pktlen,
			dev_info->max_mtu);
	conf->rxmode.mtu = max_pkt_len - overhead_len;

	if (conf->rxmode.mtu > RTE_ETHER_MTU)
		conf->txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	return 0;
}

/**
 * Setup port configuration and map port-queue to lcores.
 *  1. configure offload functions and enable RSS function.
 *  2. configure hardware with specified rx/tx queues.
 *  3. configure port memory (seperated or shared).
 *  4. map port queue to lcore by lcore_conf
*/
static void
l3fwd_poll_resource_setup(void)
{
	uint8_t nb_rx_queue, queue, socketid;
	struct rte_eth_dev_info dev_info;
	uint32_t n_tx_queue, nb_lcores;
	struct rte_eth_txconf *txconf;
	struct lcore_conf *qconf;
	uint16_t queueid, portid;
	unsigned int nb_ports;
	unsigned int lcore_id;
	int ret;

	if (check_lcore_params() < 0)
		rte_exit(EXIT_FAILURE, "check_lcore_params failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");

	nb_ports = rte_eth_dev_count_avail();

	if (check_port_config() < 0)
		rte_exit(EXIT_FAILURE, "check_port_config failed\n");

	nb_lcores = rte_lcore_count();

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_conf local_port_conf = port_conf;

		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* init port */
		printf("Initializing port %d ... ", portid );
		fflush(stdout);

		nb_rx_queue = get_port_n_rx_queues(portid);
		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		printf("Creating queues: nb_rxq=%d nb_txq=%u... ",
			nb_rx_queue, (unsigned)n_tx_queue );

		ret = rte_eth_dev_info_get(portid, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device (port %u) info: %s\n",
				portid, strerror(-ret));

		ret = config_port_max_pkt_len(&local_port_conf, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Invalid max packet length: %u (port %u)\n",
				max_pkt_len, portid);

		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

		local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
			dev_info.flow_type_rss_offloads;

		if (dev_info.max_rx_queues == 1)
			local_port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;

		if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
				port_conf.rx_adv_conf.rss_conf.rss_hf) {
			// printf("Port %u modified RSS hash function based on hardware support,"
			// 	"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			// 	portid,
			// 	port_conf.rx_adv_conf.rss_conf.rss_hf,
			// 	local_port_conf.rx_adv_conf.rss_conf.rss_hf);
			printf("Port %u modified RSS hash function.", portid);
		}

		ret = rte_eth_dev_configure(portid, nb_rx_queue,
					(uint16_t)n_tx_queue, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"Cannot configure device: err=%d, port=%d\n",
				ret, portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, "
				 "port=%d\n", ret, portid);

		ret = rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot get MAC address: err=%d, port=%d\n",
				 ret, portid);

		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");

		/* init memory */
		if (!per_port_pool) {
			/* portid = 0; this is *not* signifying the first port,
			 * rather, it signifies that portid is ignored.
			 */
			ret = init_mem(0, NB_MBUF(nb_ports));
		} else {
			ret = init_mem(portid, NB_MBUF(1));
		}
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "init_mem failed\n");

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("txq=%u,%d,%d ", lcore_id, queueid, socketid);
			fflush(stdout);

			txconf = &dev_info.default_txconf;
			txconf->offloads = local_port_conf.txmode.offloads;
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     socketid, txconf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_tx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);

			qconf = &lcore_conf[lcore_id];
			qconf->tx_queue_id[portid] = queueid;
			queueid++;

			qconf->tx_port_id[qconf->n_tx_port] = portid;
			qconf->n_tx_port++;
		}
		printf("\n");
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		printf("\nInitializing rx queues on lcore %u ... ", lcore_id );
		fflush(stdout);
		/* init RX queues */
		for(queue = 0; queue < qconf->n_rx_queue; ++queue) {
			struct rte_eth_rxconf rxq_conf;

			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;

			if (numa_on)
				socketid =
				(uint8_t)rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("rxq=%d,%d,%d ", portid, queueid, socketid);
			fflush(stdout);

			ret = rte_eth_dev_info_get(portid, &dev_info);
			if (ret != 0)
				rte_exit(EXIT_FAILURE,
					"Error during getting device (port %u) info: %s\n",
					portid, strerror(-ret));

			rxq_conf = dev_info.default_rxconf;
			rxq_conf.offloads = port_conf.rxmode.offloads;
			if (!per_port_pool)
				ret = rte_eth_rx_queue_setup(portid, queueid,
						nb_rxd, socketid,
						&rxq_conf,
						pktmbuf_pool[0][socketid]);
			else
				ret = rte_eth_rx_queue_setup(portid, queueid,
						nb_rxd, socketid,
						&rxq_conf,
						pktmbuf_pool[portid][socketid]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
				"rte_eth_rx_queue_setup: err=%d, port=%d\n",
				ret, portid);
		}
	}
}

/* A tsc-based timer responsible for triggering table reporting check */
static uint64_t timer_period = 5; /* default period is 3 seconds */


static flowbook_table g_flowtable(DEBUG_TABLE_SIZE);

/**
 * @m: packet mbuf ref.
 * @p: portid: receiving port.
 * 
*/
static void
flowbook_recording(struct rte_mbuf *m, unsigned portid, unsigned queueid)
{
	struct rte_ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;
	void *l3;
	void *l4;
	int hdr_len;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	struct rte_udp_hdr *udp_hdr;
	
	flow_key  key;
	flow_attr attr;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	// Note that the field is big ending (be).
	ether_type = eth_hdr->ether_type; 
	// Move the pointer.
	l3 = (uint8_t *)eth_hdr + sizeof(struct rte_ether_hdr);
	if ( ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4) ) {
		ipv4_hdr = (struct rte_ipv4_hdr *)l3;
		hdr_len = rte_ipv4_hdr_len(ipv4_hdr);
		key._srcip = ipv4_hdr->src_addr;
		key._dstip = ipv4_hdr->dst_addr;
		if ( hdr_len == sizeof(struct rte_ipv4_hdr) ){
			packet_type |= RTE_PTYPE_L3_IPV4;
			l4 = (uint8_t *)ipv4_hdr + hdr_len;
			key._protocol = ipv4_hdr->next_proto_id;
			if ( key._protocol == IPPROTO_TCP ){
				packet_type |= RTE_PTYPE_L4_TCP;
				tcp_hdr = (struct rte_tcp_hdr *) l4;
				key._srcport = rte_be_to_cpu_16(tcp_hdr->src_port);
				key._dstport = rte_be_to_cpu_16(tcp_hdr->dst_port);
			}
			else if ( key._protocol == IPPROTO_UDP )
			{
				packet_type |= RTE_PTYPE_L4_UDP;
				udp_hdr = (struct rte_udp_hdr *) l4;
				key._srcport = rte_be_to_cpu_16(udp_hdr->src_port);
				key._dstport = rte_be_to_cpu_16(udp_hdr->dst_port);
			}
		} else {
			packet_type |= RTE_PTYPE_L3_IPV4_EXT;
		}
		/* print the parsed flow */
		RTE_LOG(INFO, FLOWBOOK, "[Port %d: Queue %d] %s\n", portid, queueid, key.to_string().c_str()); 
		// TODO use a real flow attr.
		attr._byte_tot = m->pkt_len;
		attr._packet_tot = 1;
		attr._start_time = rte_rdtsc();
		attr._last_time  = rte_rdtsc();
		g_flowtable.upsert(key, attr);
	} else {
		// Currently only support ipv4 packets.
		packet_type |= RTE_PTYPE_L3_IPV6;
	}
	m->packet_type = packet_type;
    /* do not send anymore */
}

/* main processing loop */
static void
flowbook_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid, queueid, nb_rx;
	struct lcore_conf *qconf;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];

	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, FLOWBOOK, "lcore %u has nothing to do\n", lcore_id);
		return;
	}
	RTE_LOG(INFO, FLOWBOOK, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {
		portid = qconf->rx_queue_list[i].port_id;
        queueid = qconf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, FLOWBOOK, " -- lcoreid=%u portid=%u queueid=%u\n", 
            lcore_id, portid, queueid);
	}

	while (!force_quit) {
		/*
		 * Show flow table periodly.
		 */
		cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;
		/* if timer is enabled */
		if (timer_period > 0) {
			/* advance the timer */
			timer_tsc += diff_tsc;
			/* if timer has reached its timeout */
			if (unlikely(timer_tsc >= timer_period)) {
				/* do this only on main core */
				if (lcore_id == rte_get_main_lcore()) {
					g_flowtable.check_and_report();
					/* reset the timer */
					timer_tsc = 0;
				}
			}
		}
		prev_tsc = cur_tsc;
        
		/* Read packet from RX queues. */
		for (i = 0; i < qconf->n_rx_queue; ++i) {
			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
				MAX_PKT_BURST);
			if (unlikely(nb_rx == 0))
				continue;
            port_statistics[portid].rx += nb_rx;
            for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				// m is just an address.
				// the packet body has not been loaded.
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				flowbook_recording(m, portid, queueid);
			}
			rte_pktmbuf_free_bulk(pkts_burst, nb_rx); // Free packets in bulk.
		}
		/* End of read packet from RX queues. */
	}
}

static int
flowbook_launch_one_lcore(__rte_unused void *dummy)
{
	flowbook_main_loop();
	return 0;
}


int
main(int argc, char **argv)
{
    /* reuseful temp vars */
	uint16_t portid;
	int ret;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/**************************************************************
	 * parse application arguments (after the EAL ones) 
     * TODO: reduce unused parameters.
	 *************************************************************/
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L3FWD parameters\n");

	/**************************************************************
	 *  Configure hardware queues and bind to mbuf pools.
	 *  NOTE: most of them should not be changed.
	 *************************************************************/
	l3fwd_poll_resource_setup();
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0) {
			continue;
		}
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_dev_start: err=%d, port=%d\n",
				ret, portid);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on) {
			ret = rte_eth_promiscuous_enable(portid);
			if (ret != 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_promiscuous_enable: err=%s, port=%u\n",
					rte_strerror(-ret), portid);
		}
	}
	printf("\n");
    /* initialize port stats */
	memset(&port_statistics, 0, sizeof(port_statistics));
	check_all_ports_link_status(enabled_port_mask);


    /**************************************************************
	 *  Setup main packets processing threads.
	 *************************************************************/
	ret = 0;
    rte_eal_mp_remote_launch(flowbook_launch_one_lcore, NULL, CALL_MAIN);
	

	/**************************************************************
	 *  Wait lcore exit and clean up.
	 *  NOTE: should not change these codes.
	 *************************************************************/
    rte_eal_mp_wait_lcore();
    RTE_ETH_FOREACH_DEV(portid) {
        if ((enabled_port_mask & (1 << portid)) == 0)
            continue;
        printf("Closing port %d...", portid);
        ret = rte_eth_dev_stop(portid);
        if (ret != 0)
            printf("rte_eth_dev_stop: err=%d, port=%u\n",
                    ret, portid);
        rte_eth_dev_close(portid);
        printf(" Done\n");
    }
	rte_eal_cleanup();
	printf("Bye...\n");
	return ret;
}
