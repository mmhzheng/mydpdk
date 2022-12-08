#ifndef _FLOW_BOOK_TABLE_
#define _FLOW_BOOK_TABLE_

#include <libcuckoo/cuckoohash_map.hh>
#include <vector>

struct flow_key {
    uint32_t _srcip;
    uint32_t _dstip;
    uint16_t _srcport;
    uint16_t _dstport;
    uint8_t  _protocol;
};

struct flow_attr {
    uint32_t _start_time;
    uint32_t _last_time;
    uint16_t _packet_tot;
    uint32_t _byte_tot;
	uint16_t _packet_max;   // max pcket number in 10-us window.
	uint32_t _byte_max;     // max byte  number in 10-us window.
    std::vector<uint8_t>  _pktctrs;
    std::vector<uint16_t> _bytectrs;
};

class flowbook_table {
public:
    flowbook_table(){

    }
    void insert(){
        
    }
private:
    libcuckoo::cuckoohash_map<, std::string> Table;
};

#endif // _FLOW_BOOK_TABLE_