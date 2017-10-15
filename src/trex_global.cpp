/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "trex_global.h"
#include "bp_sim.h"



CRteMemPool         CGlobalInfo::m_mem_pool[MAX_SOCKETS_SUPPORTED];
uint32_t            CGlobalInfo::m_nodes_pool_size = 10*1024;
CParserOption       CGlobalInfo::m_options;
CGlobalMemory       CGlobalInfo::m_memory_cfg;
CPlatformSocketInfo CGlobalInfo::m_socket;
CGlobalInfo::queues_mode CGlobalInfo::m_q_mode = CGlobalInfo::Q_MODE_NORMAL;


void CGlobalMemory::Dump(FILE *fd){
    fprintf(fd," Total Memory : \n");

    const std::string * names =get_mbuf_names();

    uint32_t c_size=64;
    uint32_t c_total=0;

    int i=0;
    for (i=0; i<MBUF_ELM_SIZE; i++) {
        if ( (i>MBUF_9k) && (i<MBUF_DP_FLOWS)){
            continue;
        }
        if ( i<TRAFFIC_MBUF_64 ){
            c_total= m_mbuf[i] *c_size;
            c_size=c_size*2;
        }

        fprintf(fd," %-40s  : %lu \n",names[i].c_str(),(ulong)m_mbuf[i]);
    }
    c_total += (m_mbuf[MBUF_DP_FLOWS] * sizeof(CGenNode));

    fprintf(fd," %-40s  : %lu \n","get_each_core_dp_flows",(ulong)get_each_core_dp_flows());
    fprintf(fd," %-40s  : %s  \n","Total memory",double_to_human_str(c_total,"bytes",KBYE_1024).c_str() );
}


void CGlobalMemory::set(const CPlatformMemoryYamlInfo &info,float mul){
    int i;
    for (i=0; i<MBUF_ELM_SIZE; i++) {
        m_mbuf[i]=(uint32_t)((float)info.m_mbuf[i]*mul);
    }
    /* no need to multiply */
    m_mbuf[MBUF_64]   += info.m_mbuf[TRAFFIC_MBUF_64];
    m_mbuf[MBUF_128]  += info.m_mbuf[TRAFFIC_MBUF_128];
    m_mbuf[MBUF_256]  += info.m_mbuf[TRAFFIC_MBUF_256];
    m_mbuf[MBUF_512]  += info.m_mbuf[TRAFFIC_MBUF_512];
    m_mbuf[MBUF_1024] += info.m_mbuf[TRAFFIC_MBUF_1024];
    m_mbuf[MBUF_2048] += info.m_mbuf[TRAFFIC_MBUF_2048];
    m_mbuf[MBUF_4096] += info.m_mbuf[TRAFFIC_MBUF_4096];
    m_mbuf[MBUF_9k]   += info.m_mbuf[MBUF_9k];

    // We want minimum amount of mbufs from each type, in order to support various extream TX scenarios.
    // We can consider allowing the user to manually define less mbufs. If for example, a user knows he
    // will not send packets larger than 2k, we can zero the 4k and 9k pools, saving lots of memory.
    uint32_t min_pool_size = m_pool_cache_size * m_num_cores * 2;
    for (i = MBUF_64; i <= MBUF_9k; i++) {
        if (m_mbuf[i] < min_pool_size) {
            m_mbuf[i] = min_pool_size;
        }
    }
}


void CRteMemPool::dump_in_case_of_error(FILE *fd, rte_mempool_t *mp) {
    fprintf(fd, " Error: Failed allocating mbuf for holding %d bytes from socket %d \n", mp->elt_size, m_pool_id);
    fprintf(fd, " Try to enlarge the amount of mbufs in the configuration file '/etc/trex_cfg.yaml'\n");
    dump(fd);
}

void CRteMemPool::add_to_json(Json::Value &json, std::string name, rte_mempool_t * pool){
    uint32_t p_free = rte_mempool_count(pool);
    uint32_t p_size = pool->size;
    json[name].append((unsigned long long)p_free);
    json[name].append((unsigned long long)p_size);
}


void CRteMemPool::dump_as_json(Json::Value &json){
    add_to_json(json, "64b", m_small_mbuf_pool);
    add_to_json(json, "128b", m_mbuf_pool_128);
    add_to_json(json, "256b", m_mbuf_pool_256);
    add_to_json(json, "512b", m_mbuf_pool_512);
    add_to_json(json, "1024b", m_mbuf_pool_1024);
    add_to_json(json, "2048b", m_mbuf_pool_2048);
    add_to_json(json, "4096b", m_mbuf_pool_4096);
    add_to_json(json, "9kb", m_mbuf_pool_9k);
}


bool CRteMemPool::dump_one(FILE *fd, const char *name, rte_mempool_t *pool) {
    float p = 100.0 * (float) rte_mempool_count(pool) / (float)pool->size;
    fprintf(fd, " %-30s  : %u out of %u (%.2f %%) free %s \n", name
            , rte_mempool_count(pool), pool->size, p, (p < 5.0) ? "<-- need to enlarge" : "" );

    if (p < 5.0)
        return false;
    else
        return true;
}

void CRteMemPool::dump(FILE *fd) {
    bool ok = true;
    ok &= dump_one(fd, "mbuf_64", m_small_mbuf_pool);
    ok &= dump_one(fd, "mbuf_128", m_mbuf_pool_128);
    ok &= dump_one(fd, "mbuf_256", m_mbuf_pool_256);
    ok &= dump_one(fd, "mbuf_512", m_mbuf_pool_512);
    ok &= dump_one(fd, "mbuf_1024", m_mbuf_pool_1024);
    ok &= dump_one(fd, "mbuf_2048", m_mbuf_pool_2048);
    ok &= dump_one(fd, "mbuf_4096", m_mbuf_pool_4096);
    ok &= dump_one(fd, "mbuf_9k", m_mbuf_pool_9k);

    if (! ok) {
        fprintf(fd, "In order to enlarge the amount of allocated mbufs, need to add section like this to config file:\n");
        fprintf(fd, "memory:\n");
        fprintf(fd, "    mbuf_xx: <num>\n");
        fprintf(fd, "For example:\n");
        fprintf(fd, "    mbuf_9k: 5000\n");
        fprintf(fd, "See getting started manual for details\n");
    }
}

////////////////////////////////////////

void CGlobalInfo::dump_pool_as_json(Json::Value &json){
    CPlatformSocketInfo * lpSocket =&m_socket;

    for (int i=0; i<(int)MAX_SOCKETS_SUPPORTED; i++) {
        if (lpSocket->is_sockets_enable((socket_id_t)i)) {
            std::string socket_id = "cpu-socket-" + std::to_string(i);
            m_mem_pool[i].dump_as_json(json["mbuf_stats"][socket_id]);
        }
    }
}

std::string CGlobalInfo::dump_pool_as_json_str(void){
    Json::Value json;
    dump_pool_as_json(json);
    return (json.toStyledString());
}

void CGlobalInfo::free_pools(){
    CPlatformSocketInfo * lpSocket =&m_socket;
    CRteMemPool * lpmem;
    int i;
    for (i=0; i<(int)MAX_SOCKETS_SUPPORTED; i++) {
        if (lpSocket->is_sockets_enable((socket_id_t)i)) {
            lpmem= &m_mem_pool[i];
            utl_rte_mempool_delete(lpmem->m_small_mbuf_pool);
            utl_rte_mempool_delete(lpmem->m_mbuf_pool_128);
            utl_rte_mempool_delete(lpmem->m_mbuf_pool_256);
            utl_rte_mempool_delete(lpmem->m_mbuf_pool_512);
            utl_rte_mempool_delete(lpmem->m_mbuf_pool_1024);
            utl_rte_mempool_delete(lpmem->m_mbuf_pool_2048);
            utl_rte_mempool_delete(lpmem->m_mbuf_pool_4096);
            utl_rte_mempool_delete(lpmem->m_mbuf_pool_9k);
    }
    utl_rte_mempool_delete(m_mem_pool[0].m_mbuf_global_nodes);
  }
}

/*
 * Create mbuf pools. Number of mbufs allocated for each pool is taken from config file.
 * The numbers of the pool used for RX packets is increased.
 * rx_buffers - how many additional buffers to allocate for rx packets
 * rx_pool - which pool is being used for rx packets
 */
void CGlobalInfo::init_pools(uint32_t rx_buffers, uint32_t rx_pool) {
        /* this include the pkt from 64- */
    CGlobalMemory * lp=&CGlobalInfo::m_memory_cfg;
    CPlatformSocketInfo * lpSocket =&m_socket;

    CRteMemPool * lpmem;
    lp->m_mbuf[rx_pool] += rx_buffers;

    for (int sock = 0;  sock < MAX_SOCKETS_SUPPORTED; sock++) {
        if (lpSocket->is_sockets_enable((socket_id_t)sock)) {
            lpmem = &m_mem_pool[sock];
            lpmem->m_pool_id = sock;
            struct {
                char pool_name[100];
                rte_mempool_t **pool_p;
                uint32_t pool_type;
                uint32_t mbuf_size;
            }  pools [] = {
                { "small-pkt-const", &lpmem->m_small_mbuf_pool, MBUF_64, CONST_SMALL_MBUF_SIZE },
                { "_128-pkt-const", &lpmem->m_mbuf_pool_128, MBUF_128, CONST_128_MBUF_SIZE },
                { "_256-pkt-const", &lpmem->m_mbuf_pool_256, MBUF_256, CONST_256_MBUF_SIZE },
                { "_512-pkt-const", &lpmem->m_mbuf_pool_512, MBUF_512, CONST_512_MBUF_SIZE },
                { "_1024-pkt-const", &lpmem->m_mbuf_pool_1024, MBUF_1024, CONST_1024_MBUF_SIZE },
                { "_2048-pkt-const", &lpmem->m_mbuf_pool_2048, MBUF_2048, CONST_2048_MBUF_SIZE },
                { "_4096-pkt-const", &lpmem->m_mbuf_pool_4096, MBUF_4096, CONST_4096_MBUF_SIZE },
                { "_9k-pkt-const", &lpmem->m_mbuf_pool_9k, MBUF_9k, CONST_9k_MBUF_SIZE },
            };

            for (int j = 0; j < sizeof(pools)/ sizeof(pools[0]); j++) {
                *pools[j].pool_p = utl_rte_mempool_create(pools[j].pool_name,
                                                         lp->m_mbuf[pools[j].pool_type]
                                                         , pools[j].mbuf_size, 32, sock);
                if (*pools[j].pool_p == NULL) {
                    fprintf(stderr, "Error: Failed creaating %s mbuf pool with %d mbufs. Exiting\n"
                            , pools[j].pool_name, lp->m_mbuf[pools[j].pool_type]);
                    exit(1);
                }
            }

        }
    }

    /* global always from socket 0 */
    m_mem_pool[0].m_mbuf_global_nodes = utl_rte_mempool_create_non_pkt("global-nodes",
                                                         lp->m_mbuf[MBUF_GLOBAL_FLOWS],
                                                         sizeof(CGenNode),
                                                         128,
                                                         SOCKET_ID_ANY);
    assert(m_mem_pool[0].m_mbuf_global_nodes);
}

static void  dump_mac_addr(FILE* fd,uint8_t *p){
    int i;
    for (i=0; i<6; i++) {
        uint8_t a=p[i];
        if (i==5) {
            fprintf(fd,"%02x",a);
        }else{
            fprintf(fd,"%02x:",a);
        }
    }

}


void CPreviewMode::Dump(FILE *fd){
    fprintf(fd," flags           : %x\n", m_flags);
    fprintf(fd," write_file      : %d\n", getFileWrite()?1:0);
    fprintf(fd," verbose         : %d\n", (int)getVMode() );
    fprintf(fd," realtime        : %d\n", (int)getRealTime() );
    fprintf(fd," flip            : %d\n", (int)getClientServerFlip() );
    fprintf(fd," cores           : %d\n", (int)getCores()  );
    fprintf(fd," single core     : %d\n", (int)getSingleCore() );
    fprintf(fd," flow-flip       : %d\n", (int)getClientServerFlowFlip() );
    fprintf(fd," no clean close  : %d\n", (int)getNoCleanFlowClose() );
    fprintf(fd," zmq_publish     : %d\n", (int)get_zmq_publish_enable() );
    fprintf(fd," vlan mode       : %d\n",  get_vlan_mode());
    fprintf(fd," client_cfg      : %d\n", (int)get_is_client_cfg_enable() );
    fprintf(fd," mbuf_cache_disable  : %d\n", (int)isMbufCacheDisabled() );
    /* imarom: not in he flags anymore */
    //fprintf(fd," tcp_mode        : %d\n", (int)get_tcp_mode()?1:0 );
}

void CPreviewMode::set_vlan_mode_verify(uint8_t mode) {
        // validate that there is no vlan both in platform config file and traffic profile
    if ((CGlobalInfo::m_options.preview.get_vlan_mode() != CPreviewMode::VLAN_MODE_NONE) &&
        ( CGlobalInfo::m_options.preview.get_vlan_mode() != mode ) ) {
        fprintf(stderr, "Error: You are not allowed to specify vlan both in platform config file (--cfg) and traffic config file (-f)\n");
        fprintf(stderr, "       Please remove vlan definition from one of the files, and try again.\n");
        exit(1);
    }
    set_vlan_mode(mode);
}


/*
 * Test if option value is within allowed range.
 * val - Value to test
 * min, max - minimum, maximum allowed values.
 * opt_name - option name for error report.
 */
bool CParserOption::is_valid_opt_val(int val, int min, int max, const std::string &opt_name) {
    if (val < min || val > max) {
    std::cerr << "Value " << val << " for option " << opt_name << " is out of range. Should be (" <<  min << "-" << max << ")." << std::endl;
    return false;
    }

    return true;
}

void CParserOption::dump(FILE *fd){
    preview.Dump(fd);
    fprintf(fd," cfg file        : %s \n",cfg_file.c_str());
    fprintf(fd," mac file        : %s \n",client_cfg_file.c_str());
    fprintf(fd," out file        : %s \n",out_file.c_str());
    fprintf(fd," client cfg file : %s \n",out_file.c_str());
    fprintf(fd," duration        : %.0f \n",m_duration);
    fprintf(fd," factor          : %.0f \n",m_factor);
    fprintf(fd," mbuf_factor     : %.0f \n",m_mbuf_factor);
    fprintf(fd," latency         : %d pkt/sec \n",m_latency_rate);
    fprintf(fd," zmq_port        : %d \n",m_zmq_port);
    fprintf(fd," telnet_port     : %d \n",m_telnet_port);
    fprintf(fd," expected_ports  : %d \n",m_expected_portd);   
    fprintf(fd," tw_bucket_usec  : %f usec \n",get_tw_bucket_time_in_sec()*1000000.0);   
    fprintf(fd," tw_buckets      : %lu usec \n",(ulong)get_tw_buckets());   
    fprintf(fd," tw_levels       : %lu usec \n",(ulong)get_tw_levels());   


    if (preview.get_vlan_mode() == CPreviewMode::VLAN_MODE_LOAD_BALANCE) {
       fprintf(fd," vlans (for load balance) : [%d,%d] \n",m_vlan_port[0],m_vlan_port[1]);
    }

    int i;
    for (i = 0; i < TREX_MAX_PORTS; i++) {
        fprintf(fd," port : %d dst:",i);
        CMacAddrCfg * lp=&m_mac_addr[i];
        dump_mac_addr(fd,lp->u.m_mac.dest);
        fprintf(fd,"  src:");
        dump_mac_addr(fd,lp->u.m_mac.src);
        if (preview.get_vlan_mode() == CPreviewMode::VLAN_MODE_NORMAL) {
            fprintf(fd, " vlan:%d", m_ip_cfg[i].get_vlan());
        }
        fprintf(fd,"\n");
    }

}

void CParserOption::verify() {
    /* check for mutual exclusion options */
    if ( preview.get_is_client_cfg_enable() ) {
        if ( preview.get_vlan_mode() == CPreviewMode::VLAN_MODE_LOAD_BALANCE ) {
            throw std::runtime_error("--client_cfg_file option can not be combined with specifing VLAN in traffic profile");
        }

        if (preview.get_mac_ip_overide_enable()) {
            throw std::runtime_error("MAC override can not be combined with --client_cfg_file option");
        }
    }
}

