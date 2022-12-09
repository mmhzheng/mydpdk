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
    int upsert(flow_key* key, flow_attr* attr){
        // if table entry exists, call do_update func.
        // else insert attr as the new entry.
        table.upsert(*key, [&](flow_attr& in_mem_attr){
            // in_mem_attr._byte_max = std::max();
        }, *attr);
    }

    ~flowbook_table(){
        // TODO: report all table and release memory.
    }
private:
    FlowTable table;
};

#endif // _FLOW_BOOK_TABLE_