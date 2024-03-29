/**
 * Define flow table based on libcuckoo.
 * Author: Hao Zheng
 * Date: 2022/12/15
 * Updated: 2023/1/9 add c++ interfaces.
 */
#ifndef _FLOW_BOOK_TABLE_
#define _FLOW_BOOK_TABLE_

#include "flowbook_entry.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>
#include <unordered_map>
// postgreSQL cxx interfaces.

#ifdef ENABLE_DB
#include <pqxx/pqxx>
#endif

#define DEFAULT_TABLE_SIZE  500000000   // 500M
#define DEBUG_TABLE_SIZE    1024      

#define TABLE_SWITCH_COND_TIMER    15  // 10 seconds
#define TABLE_SWITCH_COND_LOAD     0.7        

#define NUMBER_OF_REPORTING_THREAD  4
#define NUMBER_OF_PARALLEL_TABLE    NUMBER_OF_REPORTING_THREAD

using FlowTable = std::unordered_map<flow_key, flow_attr>;
using TimePoint = std::chrono::_V2::system_clock::time_point;

class flowbook_table {

public:
    flowbook_table(size_t table_size = DEFAULT_TABLE_SIZE);
    ~flowbook_table();

    /**
     * # THREAD SAFE # 
     * Multiple thread can concurrently call this function.
    */
    void upsert(flow_key key, flow_attr attr);

    /**
     * Get current active table instance according to the flow key.
    */
    FlowTable* get_curr_read_table(const flow_key& key);
    FlowTable* get_curr_read_table(size_t table_id);
    FlowTable* get_curr_write_table(const flow_key& key);
    FlowTable* get_curr_write_table(size_t table_id);

    #ifdef ENABLE_DB 
    /**
     * # THREAD UNSAFE # 
     * check table status and report&switch the table, if needed:
     *     a) table load is exeed a threshold.
     *     b) timer exceed.
     * switch table atomically and report the table.
     * TODO: add multithread support.
    */
    void check_and_report();
    #endif

private:
    std::atomic_bool m_table_flag;

    // Table Partitiion. Make multiple threads concurrently report the tables.
    FlowTable m_table_group_a[NUMBER_OF_REPORTING_THREAD];
    FlowTable m_table_group_b[NUMBER_OF_REPORTING_THREAD];

    #ifdef ENABLE_DB
    // Database connection pool for data written.
    pqxx::connection* m_db_connpool[NUMBER_OF_REPORTING_THREAD];
    #endif 

    TimePoint m_last_report_time;

    // Global statistics.
    std::atomic<int> m_total_pkt;
};

#endif // _FLOW_BOOK_TABLE_