/*
  TRex team
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

#include "trex_driver_igb.h"
#include "trex_driver_defines.h"
#include "dpdk/drivers/net/e1000/base/e1000_regs.h"
#include "dpdk/drivers/net/ixgbe/base/ixgbe_type.h"


int CTRexExtendedDriverBase1G::get_min_sample_rate(void){
    return (RX_CHECK_MIX_SAMPLE_RATE_1G);
}


// in 1G we need to wait if links became ready to soon
void CTRexExtendedDriverBase1G::wait_after_link_up(){
    wait_x_sec(6 + CGlobalInfo::m_options.m_wait_before_traffic);
}

int CTRexExtendedDriverBase1G::wait_for_stable_link(){
    wait_x_sec(9 + CGlobalInfo::m_options.m_wait_before_traffic);
    return(0);
}

void CTRexExtendedDriverBase1G::update_configuration(port_cfg_t * cfg){
    cfg->m_tx_conf.tx_thresh.pthresh = TX_PTHRESH_1G;
    cfg->m_tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    cfg->m_tx_conf.tx_thresh.wthresh = 0;
}

void CTRexExtendedDriverBase1G::update_global_config_fdir(port_cfg_t * cfg){
    // Configuration is done in configure_rx_filter_rules by writing to registers
}

#define E1000_RXDCTL_QUEUE_ENABLE	0x02000000
// e1000 driver does not support the generic stop/start queue API, so we need to implement ourselves
int CTRexExtendedDriverBase1G::stop_queue(CPhyEthIF * _if, uint16_t q_num) {
    uint32_t reg_val = _if->pci_reg_read( E1000_RXDCTL(q_num));
    reg_val &= ~E1000_RXDCTL_QUEUE_ENABLE;
    _if->pci_reg_write( E1000_RXDCTL(q_num), reg_val);
    return 0;
}

int CTRexExtendedDriverBase1G::configure_rx_filter_rules(CPhyEthIF * _if){
    if ( get_is_stateless() ) {
        return configure_rx_filter_rules_stateless(_if);
    } else {
        return configure_rx_filter_rules_statefull(_if);
    }

    return 0;
}

int CTRexExtendedDriverBase1G::configure_rx_filter_rules_statefull(CPhyEthIF * _if) {
    uint16_t hops = get_rx_check_hops();
    uint16_t v4_hops = (hops << 8)&0xff00;
    uint8_t protocol;

    if (CGlobalInfo::m_options.m_l_pkt_mode == 0) {
        protocol = IPPROTO_SCTP;
    } else {
        protocol = IPPROTO_ICMP;
    }
    /* enable filter to pass packet to rx queue 1 */
    _if->pci_reg_write( E1000_IMIR(0), 0x00020000);
    _if->pci_reg_write( E1000_IMIREXT(0), 0x00081000);
    _if->pci_reg_write( E1000_TTQF(0),   protocol
                        | 0x00008100 /* enable */
                        | 0xE0010000 /* RX queue is 1 */
                        );


    /* 16  :   12 MAC , (2)0x0800,2      | DW0 , DW1
       6 bytes , TTL , PROTO     | DW2=0 , DW3=0x0000FF06
    */
    int i;
    // IPv4: bytes being compared are {TTL, Protocol}
    uint16_t ff_rules_v4[6]={
        (uint16_t)(0xFF06 - v4_hops),
        (uint16_t)(0xFE11 - v4_hops),
        (uint16_t)(0xFF11 - v4_hops),
        (uint16_t)(0xFE06 - v4_hops),
        (uint16_t)(0xFF01 - v4_hops),
        (uint16_t)(0xFE01 - v4_hops),
    }  ;
    // IPv6: bytes being compared are {NextHdr, HopLimit}
    uint16_t ff_rules_v6[2]={
        (uint16_t)(0x3CFF - hops),
        (uint16_t)(0x3CFE - hops),
    }  ;
    uint16_t *ff_rules;
    uint16_t num_rules;
    uint32_t mask=0;
    int  rule_id;

    if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
        ff_rules = &ff_rules_v6[0];
        num_rules = sizeof(ff_rules_v6)/sizeof(ff_rules_v6[0]);
    }else{
        ff_rules = &ff_rules_v4[0];
        num_rules = sizeof(ff_rules_v4)/sizeof(ff_rules_v4[0]);
    }

    clear_rx_filter_rules(_if);

    uint8_t len = 24;
    for (rule_id=0; rule_id<num_rules; rule_id++ ) {
        /* clear rule all */
        for (i=0; i<0xff; i+=4) {
            _if->pci_reg_write( (E1000_FHFT(rule_id)+i) , 0);
        }

        if (CGlobalInfo::m_options.preview.get_vlan_mode() != CPreviewMode::VLAN_MODE_NONE) {
            len += 8;
            if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
                // IPv6 VLAN: NextHdr/HopLimit offset = 0x18
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+0) , PKT_NTOHS(ff_rules[rule_id]) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+8) , 0x03); /* MASK */
            }else{
                // IPv4 VLAN: TTL/Protocol offset = 0x1A
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+0) , (PKT_NTOHS(ff_rules[rule_id])<<16) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(3*16)+8) , 0x0C); /* MASK */
            }
        }else{
            if (  CGlobalInfo::m_options.preview.get_ipv6_mode_enable() ){
                // IPv6: NextHdr/HopLimit offset = 0x14
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+4) , PKT_NTOHS(ff_rules[rule_id]) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+8) , 0x30); /* MASK */
            }else{
                // IPv4: TTL/Protocol offset = 0x16
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+4) , (PKT_NTOHS(ff_rules[rule_id])<<16) );
                _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)+8) , 0xC0); /* MASK */
            }
        }

        // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
        _if->pci_reg_write( (E1000_FHFT(rule_id)+0xFC) , (1<<16) | (1<<8)  | len);

        mask |=(1<<rule_id);
    }

    /* enable all rules */
    _if->pci_reg_write(E1000_WUFC, (mask<<16) | (1<<14) );

    return (0);
}


// Sadly, DPDK has no support for i350 filters, so we need to implement by writing to registers.
int CTRexExtendedDriverBase1G::configure_rx_filter_rules_stateless(CPhyEthIF * _if) {
    /* enable filter to pass packet to rx queue 1 */
    _if->pci_reg_write( E1000_IMIR(0), 0x00020000);
    _if->pci_reg_write( E1000_IMIREXT(0), 0x00081000);

    uint8_t len = 24;
    uint32_t mask = 0;
    int rule_id;

    clear_rx_filter_rules(_if);

    rule_id = 0;
    mask |= 0x1 << rule_id;
    // filter for byte 18 of packet (msb of IP ID) should equal ff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)) ,  0x00ff0000);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x04); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate IP.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x00000008);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    // same as 0, but with vlan. type should be vlan. Inside vlan, should be IP with lsb of IP ID equals 0xff
    rule_id = 1;
    mask |= 0x1 << rule_id;
    // filter for byte 22 of packet (msb of IP ID) should equal ff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 4) ,  0x00ff0000);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x40 | 0x03); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate VLAN.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x00000081);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // + bytes 16 + 17 (vlan type) should indicate IP.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) ) ,  0x00000008);
    // Was written together with IP ID filter
    // _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x03); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    rule_id = 2;
    mask |= 0x1 << rule_id;
    // ipv6 flow stat
    // filter for byte 16 of packet (part of flow label) should equal 0xff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16)) ,  0x000000ff);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x01); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate IPv6.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x0000dd86);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    rule_id = 3;
    mask |= 0x1 << rule_id;
    // same as 2, with vlan. Type is vlan. Inside vlan, IPv6 with flow label second bits 4-11 equals 0xff
    // filter for byte 20 of packet (part of flow label) should equal 0xff
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 4) ,  0x000000ff);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x10 | 0x03); /* MASK */
    // + bytes 12 + 13 (ether type) should indicate VLAN.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x00000081);
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x30); /* MASK */
    // + bytes 16 + 17 (vlan type) should indicate IP.
    _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) ) ,  0x0000dd86);
    // Was written together with flow label filter
    // _if->pci_reg_write( (E1000_FHFT(rule_id)+(2*16) + 8) , 0x03); /* MASK */
    // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1
    _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | len);

    /* enable rules */
    _if->pci_reg_write(E1000_WUFC, (mask << 16) | (1 << 14) );

    return (0);
}

// clear registers of rules
void CTRexExtendedDriverBase1G::clear_rx_filter_rules(CPhyEthIF * _if) {
    for (int rule_id = 0 ; rule_id < 8; rule_id++) {
        for (int i = 0; i < 0xff; i += 4) {
            _if->pci_reg_write( (E1000_FHFT(rule_id) + i) , 0);
        }
    }
}

int CTRexExtendedDriverBase1G::set_rcv_all(CPhyEthIF * _if, bool set_on) {
    // byte 12 equals 08 - for IPv4 and ARP
    //                86 - For IPv6
    //                81 - For VLAN
    //                88 - For MPLS
    uint8_t eth_types[] = {0x08, 0x86, 0x81, 0x88};
    uint32_t mask = 0;

    clear_rx_filter_rules(_if);

    if (set_on) {
        for (int rule_id = 0; rule_id < sizeof(eth_types); rule_id++) {
            mask |= 0x1 << rule_id;
            // Filter for byte 12 of packet
            _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 4) ,  0x000000 | eth_types[rule_id]);
            _if->pci_reg_write( (E1000_FHFT(rule_id)+(1*16) + 8) , 0x10); /* MASK */
            // FLEX_PRIO[[18:16] = 1, RQUEUE[10:8] = 1, len = 24
            _if->pci_reg_write( (E1000_FHFT(rule_id) + 0xFC) , (1 << 16) | (1 << 8) | 24);
        }
    } else {
        configure_rx_filter_rules(_if);
    }

    return 0;
}

bool CTRexExtendedDriverBase1G::get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats){

    stats->ipackets     +=  _if->pci_reg_read(E1000_GPRC) ;

    stats->ibytes       +=  (_if->pci_reg_read(E1000_GORCL) );
    stats->ibytes       +=  (((uint64_t)_if->pci_reg_read(E1000_GORCH))<<32);


    stats->opackets     +=  _if->pci_reg_read(E1000_GPTC);
    stats->obytes       +=  _if->pci_reg_read(E1000_GOTCL) ;
    stats->obytes       +=  ( (((uint64_t)_if->pci_reg_read(IXGBE_GOTCH))<<32) );

    stats->f_ipackets   +=  0;
    stats->f_ibytes     += 0;


    stats->ierrors      +=  ( _if->pci_reg_read(E1000_RNBC) +
                              _if->pci_reg_read(E1000_CRCERRS) +
                              _if->pci_reg_read(E1000_ALGNERRC ) +
                              _if->pci_reg_read(E1000_SYMERRS ) +
                              _if->pci_reg_read(E1000_RXERRC ) +

                              _if->pci_reg_read(E1000_ROC)+
                              _if->pci_reg_read(E1000_RUC)+
                              _if->pci_reg_read(E1000_RJC) +

                              _if->pci_reg_read(E1000_XONRXC)+
                              _if->pci_reg_read(E1000_XONTXC)+
                              _if->pci_reg_read(E1000_XOFFRXC)+
                              _if->pci_reg_read(E1000_XOFFTXC)+
                              _if->pci_reg_read(E1000_FCRUC)
                              );

    stats->oerrors      +=  0;
    stats->imcasts      =  0;
    stats->rx_nombuf    =  0;
    
    return true;
}

void CTRexExtendedDriverBase1G::clear_extended_stats(CPhyEthIF * _if){
}


#if 0
int CTRexExtendedDriverBase1G::get_rx_stats(CPhyEthIF * _if, uint32_t *pkts, uint32_t *prev_pkts
                                            ,uint32_t *bytes, uint32_t *prev_bytes, int min, int max) {
    repid_t repid = _if->get_repid();
    return g_trex.m_rx.get_rx_stats(repid, pkts, prev_pkts, bytes, prev_bytes, min, max);
}
#endif


