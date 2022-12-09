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

#include <stdint.h>

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif


#define FLOW_BOOK_PDU_TYPE_SRT   1     // BG Table Level 1
#define FLOW_BOOK_PDU_TYPE_MID   2	   // BG Table Level 2
#define FLOW_BOOK_PDU_TYPE_LNG   3     // BG Table Level 3
#define FLOW_BOOK_PDU_TYPE_ERF   201   // FG Insert Failed.
#define FLOW_BOOK_PDU_TYPE_ERB   202   // BG Insert Failed.

#define FLOW_BOOK_PDU_TYPE_1_CNT_NUM 1
#define FLOW_BOOK_PDU_TYPE_2_CNT_NUM 2
#define FLOW_BOOK_PDU_TYPE_3_CNT_NUM 4
#define FLOW_BOOK_PDU_TYPE_4_CNT_NUM 8
#define FLOW_BOOK_PDU_TYPE_5_CNT_NUM 16

/**
 * Flowbook header: UDP Port (19987)
 */
struct flowbook_header {   
	uint8_t np_id; 
	uint8_t pdu_num; 
} __rte_packed;

struct flowbook_pdu {
	/* pdu type, number of counters*/
	uint8_t    type; 

	/* flow key */
	rte_be16_t flowkey_srcport;
	rte_be16_t flowkey_dstport;
	rte_be32_t flowkey_srcip;
	rte_be32_t flowkey_dstip;
	uint8_t    flowkey_protocol;

	/* flow attribute*/
	uint16_t   packet_max;
	uint16_t   packet_tot;
	uint32_t   byte_tot;
	uint32_t   byte_max;

	rte_be32_t window_begin;
	rte_be32_t window_end;
} __rte_packed;

struct flowbook_ctrs_type_1 {
	// short table
	uint8_t    pktctrs[FLOW_BOOK_PDU_TYPE_1_CNT_NUM];
	rte_be16_t bytectrs[FLOW_BOOK_PDU_TYPE_1_CNT_NUM];
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif /* _FLOWBOOK_PDU_H_ */
