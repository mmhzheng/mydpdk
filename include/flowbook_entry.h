#ifndef _FLOWBOOK_ENTRY_H_
#define _FLOWBOOK_ENTRY_H_

#include <cstdint>
#include <vector>
#include <functional>
#include <string>
#include <arpa/inet.h>


/**
 * Definition for the key of a flow record.
*/
struct flow_key {
    uint32_t _srcip;
    uint32_t _dstip;
    uint16_t _srcport;
    uint16_t _dstport;
    uint8_t  _protocol;
    std::string to_string() const{
        std::string format;
        char srcbuf[INET_ADDRSTRLEN + 1];
        char dstbuf[INET_ADDRSTRLEN + 1];
        if (nullptr == inet_ntop(AF_INET, &_srcip, srcbuf, sizeof(srcbuf)) 
            || nullptr == inet_ntop(AF_INET, &_dstip, dstbuf, sizeof(dstbuf)) ){
            return "UnkownIP";
        }
        // Trust sscanf, here is a warn.
        // TODO: modify it to safe.
        sscanf(format.c_str(), "FlowKey=(%s:%hu => %s:%hu, %hhu)", 
                    srcbuf, &_srcport, dstbuf, &_dstport, &_protocol);
        return format;
    }
};

/**
 * Definition for the val of a flow record.
*/
struct flow_attr {
    uint32_t _start_time;       // start time of a flow: only update at the init.
    uint32_t _last_time;        // last update time (used to aging and regard as the end of a flow)
    uint16_t _packet_tot = 0;   // total number of packets of the flow
    uint32_t _byte_tot   = 0;   // total bytes of a flow
	uint16_t _packet_max = 0;   // max pcket number in 10-us window
	uint32_t _byte_max   = 0;   // max byte  number in 10-us window
    std::vector<uint8_t>  _pktctrs;
    std::vector<uint16_t> _bytectrs;
    std::string to_string() const{
        std::string format;
        sscanf(format.c_str(), "FlowAttr=(start_time=%u, last_time=%u, total_pkt=%hu, total_byte=%u)", 
                    &_start_time, &_last_time, &_packet_tot, &_byte_tot);
        return format;
    }
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