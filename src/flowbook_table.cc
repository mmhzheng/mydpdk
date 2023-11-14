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

    #ifdef ENABLE_DB
    // Init database connection.
    try{
        for(size_t i=0; i<NUMBER_OF_PARALLEL_TABLE; ++i){
            m_db_connpool[i] = new pqxx::connection("dbname = dcbook_hw_test user = postgres password = postgres  \
                                            hostaddr = 127.0.0.1 port = 5432");
            if(m_db_connpool[i]->is_open()){
                std::cout <<"THREAD ID: "<< i<< ", opened database " << m_db_connpool[i]->dbname() << " successfully!"<< std::endl;
            }else{
                std::cerr <<"THREAD ID: "<< i<< ", opened database " << m_db_connpool[i]->dbname() << " failed !"<< std::endl; 
            }
        }
    }
    catch (std::exception const &e){
        std::cerr << "Error: " << e.what() << std::endl;
        exit(0);
    }
    #endif
}


/**
 * # THREAD SAFE # 
 * Multiple thread can concurrently call this function.
*/
void flowbook_table::upsert(flow_key key, flow_attr attr){  
    FlowTable* write_table = get_curr_write_table(key);    
    // Assuming write_table is a std::unordered_map with the same key type as 'key' and value type as 'flow_attr'
    auto it = write_table->find(key);

    if (it == write_table->end()) {
        // Key does not exist, insert new element
        write_table->insert({key, attr});
    } else {
        // Key exists, update the element
        flow_attr& in_mem_attr = it->second;
        in_mem_attr._byte_max = std::max(in_mem_attr._byte_max, attr._byte_max);
        in_mem_attr._packet_max = std::max(in_mem_attr._packet_max, attr._packet_max);
        in_mem_attr._byte_tot += attr._byte_tot;
        in_mem_attr._packet_tot += attr._packet_tot;
        in_mem_attr._max_wid = attr._max_wid;
        // Handle any other attributes that need to be updated
    }
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


#ifdef ENABLE_DB
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
                        try{
                            // Insert to pgsql table HERE.
                            pqxx::work txn{*m_db_connpool[i]};
                            for (auto &it : lt) {
                                logfile << "THREAD: "<<i << ": " << it.first.to_string() << it.second.to_string() << std::endl;
                                // Prepare sqls here.
                                char upsert_flow_info_sql[1024];
                                char quert_flow_id_sql[256];
                                sprintf(upsert_flow_info_sql, 
                                    "INSERT INTO tb_flow_info(srcip, dstip, srcport, dstport, protocol,"                     
                                                                "pkt_tot, pkt_max, byte_tot, byte_max, wid_begin, wid_last) "
                                    "VALUES (%u, %u, %hu, %hu, %hhu, %u, %u, %u, %u, %u, %u) "
                                    "ON CONFLICT(srcip, dstip, srcport, dstport, protocol) DO UPDATE " 
                                    "SET pkt_tot=tb_flow_info.pkt_tot+%u, "
                                        "pkt_max=CASE WHEN %u > tb_flow_info.pkt_max "
                                                "THEN %u "
                                                "ELSE tb_flow_info.pkt_max "
                                                "END, "
                                        "byte_tot=tb_flow_info.byte_tot+%u, "
                                        "byte_max=CASE WHEN %u > tb_flow_info.byte_max "
                                                "THEN %u "
                                                "ELSE tb_flow_info.byte_max "
                                                "END, "
                                        "wid_last=%u; ",
                                    it.first._srcip, it.first._dstip, it.first._srcport, it.first._dstport, it.first._protocol,
                                    it.second._packet_tot, it.second._packet_max, it.second._byte_tot, it.second._byte_max, it.second._start_wid, it.second._max_wid,
                                    it.second._packet_tot, 
                                    it.second._packet_max, it.second._packet_max, 
                                    it.second._byte_tot, 
                                    it.second._byte_max, it.second._byte_max, 
                                    it.second._max_wid
                                ); // END construct SQL 1.
                                sprintf(quert_flow_id_sql,
                                    "SELECT fid FROM tb_flow_info "
                                    "WHERE srcip=%u AND dstip=%u AND srcport=%hu AND dstport=%hu AND protocol=%hhu; ",
                                    it.first._srcip, it.first._dstip, it.first._srcport, it.first._dstport, it.first._protocol
                                ); // END construct SQL 2.
                                txn.exec0(upsert_flow_info_sql);
                                pqxx::result r = txn.exec(quert_flow_id_sql);
                                if(r.empty()){
                                    std::cerr << "Cannot find flow with SQL: "<< quert_flow_id_sql << std::endl;
                                    continue;
                                }
                                uint32_t fid = r.at(0)["fid"].as<uint32_t>();
                                // DEBUG INFO
                                std::cout << "Get fid= "<< fid << std::endl;
                                for(uint32_t wid=0; wid<=it.second._max_wid; ++wid){
                                    char upsert_flow_wid_cnt_sql[256];
                                    sprintf(upsert_flow_wid_cnt_sql,
                                        "INSERT INTO tb_flow_wid_counter(fid, wid, pkt_count, byte_count) "
                                        "VALUES (%u, %u, %hhu, %hu) "
                                        "ON CONFLICT(fid, wid) DO UPDATE " 
                                        "SET pkt_count=tb_flow_wid_counter.pkt_count+%u, "
                                            "byte_count=tb_flow_wid_counter.byte_count+%u; ",
                                        fid, wid, 1, 1,
                                        1, 1 // TODO: Set right packet count.
                                    ); // END construct SQL 3.
                                    txn.exec0(upsert_flow_wid_cnt_sql);
                                }
                                std::cout << "THREAD: "<<i << ": " << it.first.to_string() << it.second.to_string() << std::endl;
                            }
                            // Not really needed, since we made no changes, but good habit to be
                            // explicit about when the transaction is done.
                            txn.commit(); 
                        }
                        catch (pqxx::sql_error const &e){
                                std::cerr << "SQL error: " << e.what() << std::endl;
                                std::cerr << "Query was: " << e.query() << std::endl;
                                // TODO: try to reconnect.
                            }
                        catch (std::exception const &e){
                            std::cerr << "Error: " << e.what() << std::endl;
                            // TODO: try to reconnect.
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
#endif


flowbook_table::~flowbook_table(){
    std::ofstream logfile;
    logfile.open("log/flow_status_global.log");
    logfile << "Total Received & Processed Packets: " << m_total_pkt.load() << std::endl;
    logfile.close();

    #ifdef ENABLE_DB
    // release database connection
    for(size_t i=0; i<NUMBER_OF_PARALLEL_TABLE; ++i){
        m_db_connpool[i]->disconnect();
        std::cout <<"THREAD ID: "<< i<< ", disconect database " << m_db_connpool[i]->dbname() << "successfully!"<< std::endl;
    }
    #endif
}
#endif