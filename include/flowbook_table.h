#ifndef _FLOW_BOOK_TABLE_
#define _FLOW_BOOK_TABLE_

#include <libcuckoo/cuckoohash_map.hh>
#include "flowbook_entry.h"


#define DEFAULT_TABLE_SIZE  500000000   // 500M
#define DEBUG_TABLE_SIZE    100         // 100

using FlowTable=libcuckoo::cuckoohash_map<flow_key, flow_attr>;

class flowbook_table {
public:
    flowbook_table(size_t table_size = DEFAULT_TABLE_SIZE){
        table.reserve(table_size);
    }

    // main update func.
    // If not exist, insert <key, attr>
    // Else, update the key with new attr
    void upsert(flow_key key, flow_attr attr){
        table.upsert(key, [&](flow_attr& in_mem_attr){
            in_mem_attr._byte_max = std::max(in_mem_attr._byte_max, attr._byte_max);
            in_mem_attr._packet_max = std::max(in_mem_attr._packet_max, attr._packet_max);
            in_mem_attr._byte_tot += attr._byte_tot;
            in_mem_attr._packet_tot += attr._packet_tot;
            in_mem_attr._last_time = attr._last_time;
        }, attr);
    }

    void show() {
        printf("%s\n", "Show table status once ----------------------");
        auto lt = table.lock_table();
        for (auto &it : lt){
            printf("%s, %s", it.first.to_string().c_str(), it.second.to_string().c_str());
        }
    }

    ~flowbook_table(){
        // TODO: report all table and release memory.
        // auto lt = table.lock_table();
        // for (const auto &it : lt) {
        //     delete &it.first;
        //     delete &it.second;
        // }
    }
private:
    FlowTable table;
};

#endif // _FLOW_BOOK_TABLE_