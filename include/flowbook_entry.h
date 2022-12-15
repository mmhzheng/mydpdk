/**
 * Define flow key and flow attribute.
 * Author: Hao Zheng
 * Date: 2022/12/15
 */

#ifndef _FLOWBOOK_ENTRY_H_
#define _FLOWBOOK_ENTRY_H_

#include <cstdint>
#include <vector>
#include <functional>
#include <string>
#include <arpa/inet.h>
#include "flowbook_hash.h"


/**
 * Definition for the key of a flow record.
*/
struct flow_key {
    uint32_t _srcip;
    uint32_t _dstip;
    uint16_t _srcport;
    uint16_t _dstport;
    uint8_t  _protocol;
    size_t hash() const{
        /**
         * TODO:  choose a better hash func.
        */
        static flow_hasher flow_hasher;
        size_t h1 = flow_hasher.run((const char*)&_srcip, 4);
        size_t h2 = flow_hasher.run((const char*)&_dstip, 4);
        size_t h3 = flow_hasher.run((const char*)&_srcport, 2);
        size_t h4 = flow_hasher.run((const char*)&_dstport, 2);
        size_t h5 = flow_hasher.run((const char*)&_protocol, 1);
        return h1 ^ h2 ^ h3 ^ h4 ^ h5;
    }
    std::string to_string() const{
        char format[100];
        char srcbuf[INET_ADDRSTRLEN + 1];
        char dstbuf[INET_ADDRSTRLEN + 1];
        if (nullptr == inet_ntop(AF_INET, &_srcip, srcbuf, sizeof(srcbuf)) 
            || nullptr == inet_ntop(AF_INET, &_dstip, dstbuf, sizeof(dstbuf)) ){
            return "UnkownIP";
        }
        // Trust sscanf, here is a warn.
        // TODO: modify it to safe.
        sprintf(format, "FlowKey=(%s:%hu => %s:%hu, %hhu)", srcbuf, _srcport, dstbuf, _dstport, _protocol);
        return std::string(format);
    }
};

/**
 * Definition for the val of a flow record.
*/
struct flow_attr {
    uint64_t _start_time;       // start time of a flow: only update at the init.
    uint64_t _last_time;        // last update time (used to aging and regard as the end of a flow)
    uint16_t _packet_tot = 0;   // total number of packets of the flow
    uint32_t _byte_tot   = 0;   // total bytes of a flow
	uint16_t _packet_max = 0;   // max pcket number in 10-us window
	uint32_t _byte_max   = 0;   // max byte  number in 10-us window
    std::vector<uint8_t>  _pktctrs;
    std::vector<uint16_t> _bytectrs;
    std::string to_string() const{
        char format[100];
        sprintf(format, "FlowAttr=(start_time=%lu, last_time=%lu, total_pkt=%hu, total_byte=%u)", 
                                 _start_time, _last_time, _packet_tot, _byte_tot);
        return std::string(format);
    }
};

namespace std {
    template <> struct hash<flow_key> {
        size_t operator()(const flow_key &kb) const 
        { 
            return kb.hash();
        }
    };

    template <> struct equal_to<flow_key> {
        bool operator()(const flow_key &lhs, const flow_key &rhs) const {
            if(lhs._dstip != rhs._dstip)
                return false;
            if(lhs._srcip != rhs._srcip)
                return false;
            if(lhs._srcport != rhs._srcport)
                return false;
            if(lhs._dstport != rhs._dstport)
                return false;
            if(lhs._protocol != rhs._protocol)
                return false;
            return true;
        }
    };
}

# endif