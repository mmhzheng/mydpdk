#ifndef _FLOW_BOOK_TABLE_
#define _FLOW_BOOK_TABLE_

#include <libcuckoo/cuckoohash_map.hh>
#include "flowbook_entry.h"
#include <atomic>
#include <chrono>
#include <fstream>

#define DEFAULT_TABLE_SIZE  500000000   // 500M
#define DEBUG_TABLE_SIZE    100         // 100

#define TABLE_SWITCH_COND_TIMER  10  // 10 seconds
#define TABLE_SWITCH_COND_LOAD   0.7        

using FlowTable = libcuckoo::cuckoohash_map<flow_key, flow_attr>;
using TimePoint = std::chrono::_V2::system_clock::time_point;

class flowbook_table {

public:
    flowbook_table(size_t table_size = DEFAULT_TABLE_SIZE){
        m_table_a.reserve(table_size);
        m_table_a.reserve(table_size);

        // The thread is safe here.
        std::atomic_init(&m_table_flag, true); // true: w a r b, flase: w b r a.
        m_last_report_time = std::chrono::high_resolution_clock::now();
    }


    /**
     * # THREAD SAFE # 
     * Multiple thread can concurrently call this function.
    */
    void upsert(flow_key key, flow_attr attr){  
        FlowTable* write_table = get_curr_write_table();    
        // main update func.
        // If not exist, insert <key, attr>
        // Else, update the key with new attr
        write_table->upsert(key, [&](flow_attr& in_mem_attr){
            in_mem_attr._byte_max = std::max(in_mem_attr._byte_max, attr._byte_max);
            in_mem_attr._packet_max = std::max(in_mem_attr._packet_max, attr._packet_max);
            in_mem_attr._byte_tot += attr._byte_tot;
            in_mem_attr._packet_tot += attr._packet_tot;
            in_mem_attr._last_time = attr._last_time;
        }, attr);        
    }

    FlowTable* get_curr_read_table(){
        // true: w a r b, flase: w b r a.
        return m_table_flag.load() == true? &m_table_b : &m_table_a;
    }

    FlowTable* get_curr_write_table(){
        // true: w a r b, flase: w b r a.
        return m_table_flag.load() == true? &m_table_a : &m_table_b;
    }

    /**
     * # THREAD UNSAFE # 
     * check table status and report&switch the table, if needed:
     *     a) table load is exeed a threshold.
     *     b) timer exceed.
     * switch table atomically and report the table.
    */
    void check_and_report(){
        
        
        // Get current table.
        FlowTable* write_table = get_curr_write_table();

        // Judge if need to switch table. Timer or Load.
        double table_load = write_table->load_factor();
        uint32_t diff_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::high_resolution_clock::now() - m_last_report_time).count();

        // printf("check report: %d, %f \n", diff_time, table_load);
        if(diff_time >= TABLE_SWITCH_COND_TIMER || table_load >= TABLE_SWITCH_COND_LOAD)
        {
            // Switch table with CAS.
            bool old_value = m_table_flag.load();
            while (!m_table_flag.compare_exchange_weak(old_value, !old_value)) {}
            FlowTable* read_table = get_curr_read_table();
            // if (read_table == & m_table_a)
            //     printf("read from table %s \n", "a");
            // else
            //     printf("read from table %s \n", "b");

            /* Reporting statistics */
            TimePoint report_time  = std::chrono::high_resolution_clock::now();
            uint32_t  report_count = 0;

            std::ofstream logfile;
            char log_file_name[64];
            sprintf(log_file_name, "flow_status_%ld.log", std::chrono::duration_cast<std::chrono::seconds>(
                                                    report_time.time_since_epoch()).count());
            logfile.open(log_file_name);
            
            // Still need to lock table because there may some residual writing threads try to update this table.
            {// The residual writing threads pre update this table will also be considered.
                auto lt = read_table->lock_table();
                for (auto &it : lt){
                    // printf("%s, %s \n", it.first.to_string().c_str(), it.second.to_string().c_str());
                    // report the table.
                    logfile << it.first.to_string() << it.second.to_string() << std::endl;
                    report_count++;
                }
                lt.clear();
            }// The residual writing threads post update this table is no problem.
            m_last_report_time = report_time;
            logfile.close();
            // TODO: Print reporting statistics log here.
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
    std::atomic_bool m_table_flag;
    FlowTable m_table_a;
    FlowTable m_table_b;

    TimePoint m_last_report_time;
};

#endif // _FLOW_BOOK_TABLE_