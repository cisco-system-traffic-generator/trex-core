#include <stdint.h>
#include "trex_defs.h"
#include "dpdk_port_map.h"
#include <assert.h>


/* default MAP == direct */                                         
CTRexPortMapper::CTRexPortMapper(){
    m_max_trex_vports   = TREX_MAX_PORTS;   
    m_max_rte_eth_ports = TREX_MAX_PORTS; 
    int i;
    for (i=0; i<TREX_MAX_PORTS; i++) {
        m_map[i]=i;
        m_rmap[i]=i;
    }
}


void CTRexPortMapper::set(uint8_t rte_ports,dpdk_map_args_t &  pmap){
    
    m_max_trex_vports = pmap.size();
    m_max_rte_eth_ports =rte_ports;

    int i;
    for (i=0; i<pmap.size(); i++) {
        m_map[i]=pmap[i];
        set_rmap(m_map[i],i); /* set reverse */
    }
}


                
CTRexPortMapper * CTRexPortMapper::m_ins;


CTRexPortMapper * CTRexPortMapper::Ins(){
    if (!m_ins) {
        m_ins = new CTRexPortMapper();
    }
    return (m_ins);
}


void CTRexPortMapper::Dump(FILE *fd){
    int i;
    fprintf(fd," TRex port mapping \n");
    fprintf(fd," ----------------- \n");
    for (i=0; i<(int)m_max_trex_vports; i++) {
        fprintf(fd," TRex vport: %d dpdk_rte_eth: %d \n",i,(int)m_map[i]);
    }
}



