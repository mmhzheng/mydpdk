#ifndef _FLOWTOOK_TABLE_H_
#define _FLOWTOOK_TABLE_H_

#include "flowbook_table.h"

flowbook_table::flowbook_table(size_t table_size){
    for(size_t i=0; i<NUMBER_OF_PARALLEL_TABLE; ++i)
    {
        m_table_group_a[i].reserve(table_size / NUMBER_OF_PARALLEL_TABLE);
        m_table_group_b[i].reserve(table_size / NUMBER_OF_PARALLEL_TABLE);
    }
    // The thread is safe here.
    std::atomic_init(&m_table_flag, true); // true: w a r b, flase: w b r a.
    std::atomic_init(&m_total_pkt,  0);
    m_last_report_time = std::chrono::high_resolution_clock::now();
}


/**
 * # THREAD SAFE # 
 * Multiple thread can concurrently call this function.
*/
void flowbook_table::upsert(flow_key key, flow_attr attr){  
    FlowTable* write_table = get_curr_write_table(key);    
    // main update func.
    // If not exist, insert <key, attr>
    // Else, update the key with new attr
    write_table->upsert(key, [&](flow_attr& in_mem_attr){
        in_mem_attr._byte_max = std::max(in_mem_attr._byte_max, attr._byte_max);
        in_mem_attr._packet_max = std::max(in_mem_attr._packet_max, attr._packet_max);
        in_mem_attr._byte_tot += attr._byte_tot;
        in_mem_attr._packet_tot += attr._packet_tot;
        in_mem_attr._last_time = attr._last_time;
        // Below attributes are only updated when inserting.
        // in_mem_attr._start_time;
    }, attr);
    m_total_pkt++;        
}

/**
 * Get current active table instance according to the flow key.
*/
FlowTable* flowbook_table::get_curr_read_table(const flow_key& key){
    // true: w a r b, flase: w b r a.
    FlowTable* active_table_group = m_table_flag.load() == true? m_table_group_b : m_table_group_a;
    size_t idx = key.hash() % NUMBER_OF_PARALLEL_TABLE;
    return active_table_group + idx;
}
FlowTable* flowbook_table::get_curr_read_table(size_t table_id){
    if(table_id >= NUMBER_OF_PARALLEL_TABLE)
        return nullptr;
    // true: w a r b, flase: w b r a.
    FlowTable* active_table_group = m_table_flag.load() == true? m_table_group_b : m_table_group_a;
    return active_table_group + table_id;
}
FlowTable* flowbook_table::get_curr_write_table(const flow_key& key){
    // true: w a r b, flase: w b r a.
    FlowTable* active_table_group = m_table_flag.load() == true? m_table_group_a : m_table_group_b;
    size_t idx = key.hash() % NUMBER_OF_PARALLEL_TABLE;
    // printf("hash %lu, idx %lu\n", key.hash(), idx);
    return active_table_group + idx;
}
FlowTable* flowbook_table::get_curr_write_table(size_t table_id){
    if(table_id >= NUMBER_OF_PARALLEL_TABLE)
        return nullptr;
    // true: w a r b, flase: w b r a.
    FlowTable* active_table_group = m_table_flag.load() == true? m_table_group_a : m_table_group_b;
    return active_table_group + table_id;
}

/**
 * # THREAD UNSAFE # 
 * check table status and report&switch the table, if needed:
 *     a) table load is exeed a threshold.
 *     b) timer exceed.
 * switch table atomically and report the table.
*/
void flowbook_table::check_and_report(){

    bool need_report_flag = false;

    // Check time and table status.
    uint32_t diff_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - m_last_report_time).count();
    if(diff_time >= TABLE_SWITCH_COND_TIMER){
        need_report_flag = true;
    }
    for(size_t i=0; i<NUMBER_OF_PARALLEL_TABLE; ++i)
    {
        if (need_report_flag == true)
            break;
        FlowTable* curr_table = get_curr_write_table(i);
        // Judge if need to switch table. Timer or Load.
        double table_load = curr_table->load_factor();
        if (table_load >= TABLE_SWITCH_COND_LOAD){
            need_report_flag = true;
        }
    }
    if( need_report_flag )
    {
        /* Switch table with CAS. */ 
        bool old_value = m_table_flag.load();
        while (!m_table_flag.compare_exchange_weak(old_value, !old_value)) {}

        /* Reporting statistics */
        TimePoint report_time  = std::chrono::high_resolution_clock::now();

        std::ofstream logfile;
        char log_file_name[64];
        sprintf(log_file_name, "log/flow_status_%ld.log", std::chrono::duration_cast<std::chrono::seconds>(
                                                report_time.time_since_epoch()).count());
        logfile.open(log_file_name);

            /*  Lauch multiple threads to report the table. */
        for(size_t i=0; i<NUMBER_OF_REPORTING_THREAD; i++){
            std::thread threadObj(
                [&]{
                    {// Still need to lock table because there may some residual writing threads try to update this table.
                    // The residual writing threads pre update this table will also be considered.
                    FlowTable* read_table = get_curr_read_table(i);
                    auto lt = read_table->lock_table();
                    for (auto &it : lt){
                        logfile << "THREAD: "<<i << ": " << it.first.to_string() << it.second.to_string() << std::endl;
                        // std::cout << "THREAD: "<<i << ": " << it.first.to_string() << it.second.to_string() << std::endl;
                    }
                    lt.clear();
                    }// The residual writing threads post update this table is no problem.
                }
            );
            threadObj.join(); 
        }

        m_last_report_time = report_time;
        logfile.close();
        // TODO: Print reporting statistics log here.
    }
}

flowbook_table::~flowbook_table(){
    std::ofstream logfile;
    logfile.open("log/flow_status_global.log");
    logfile << "Total Received & Processed Packets: " << m_total_pkt.load() << std::endl;
    logfile.close();
}

#endif