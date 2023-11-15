/**
 * Packet header define for Flowbook.
 * Author: Hao Zheng
 * Date: 2022/12/08
 */

#ifndef _FLOWBOOK_PDU_H_
#define _FLOWBOOK_PDU_H_

/**
 * @file
 *
 * UDP-related defines
 */

#include <generic/rte_byteorder.h>
#include <stdint.h>

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLOW_BOOK_PDU_MAX_CTRS   100

/**
 * Flowbook header: UDP Port (19987)
 */
struct flowbook_header {   
	uint8_t np_id; 
	uint8_t pdu_num; 
} __rte_packed;

struct utrace_pdu {
	rte_be32_t timestamp;
	rte_be32_t queue_len;
}__rte_packed;

struct flowbook_pdu {
	/* flow key */
	rte_be32_t   flowkey_srcip;
	rte_be32_t   flowkey_dstip;
	rte_be16_t   flowkey_srcport;
	rte_be16_t   flowkey_dstport;
	uint8_t      flowkey_protocol;

	/* flow attribute*/
	rte_be16_t   packet_max;
	rte_be16_t   packet_tot;
	rte_be32_t   byte_tot;
	rte_be32_t   byte_max;

	rte_be32_t   start_wid;
	uint8_t      max_wid_index;

	/* WARN, notice the counter index must <= max_offset */
	uint8_t    pktctrs[FLOW_BOOK_PDU_MAX_CTRS];
	rte_be16_t bytectrs[FLOW_BOOK_PDU_MAX_CTRS];
} __rte_packed;


#ifdef __cplusplus
}
#endif

#endif /* _FLOWBOOK_PDU_H_ */
