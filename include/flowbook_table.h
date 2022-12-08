#ifndef _FLOW_BOOK_TABLE_
#define _FLOW_BOOK_TABLE_

#include <libcuckoo/cuckoohash_map.hh>

class flowbook_table {
public:
    flowbook_table(){

    }
    void insert(){
        
    }
private:
    libcuckoo::cuckoohash_map<int, std::string> Table;
};

#endif // _FLOW_BOOK_TABLE_