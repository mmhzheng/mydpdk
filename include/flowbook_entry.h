#ifndef _FLOWBOOK_ENTRY_H_
#define _FLOWBOOK_ENTRY_H_

#include <cstdint>
#include <vector>
#include <functional>

/**
 * Definition for the key of a flow record.
*/
struct flow_key {
    uint32_t _srcip;
    uint32_t _dstip;
    uint16_t _srcport;
    uint16_t _dstport;
    uint8_t  _protocol;
};

/**
 * Definition for the val of a flow record.
*/
struct flow_attr {
    uint32_t _start_time;   // start time of a flow: only update at the init.
    uint32_t _last_time;    // last update time (used to aging and regard as the end of a flow)
    uint16_t _packet_tot;   // total number of packets of the flow
    uint32_t _byte_tot;     // total bytes of a flow
	uint16_t _packet_max;   // max pcket number in 10-us window
	uint32_t _byte_max;     // max byte  number in 10-us window
    std::vector<uint8_t>  _pktctrs;
    std::vector<uint16_t> _bytectrs;
};

namespace std {
    template <> struct hash<flow_key> {
        size_t operator()(const flow_key &kb) const 
        { 
            // TODO: implement a hash function.
            return 0;
        }
    };

    template <> struct equal_to<flow_key> {
        bool operator()(const flow_key &lhs, const flow_key &rhs) const {
            return true;
        }
    };
}

# endif