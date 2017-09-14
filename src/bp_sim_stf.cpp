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

#include"bp_sim.h"


/*
 * This function will insert an IP option header containing metadata for the
 * rx-check feature.
 *
 * An mbuf is created to hold the new option header plus the portion of the
 * packet after the base IP header (includes any IP options header that might
 * exist).  This mbuf is then linked into the existing mbufs (becoming the
 * second mbuf).
 *
 * Note that the rxcheck option header is inserted as the first option header,
 * and any existing IP option headers are placed after it.
 */

void CFlowPktInfo::do_generate_new_mbuf_rxcheck(rte_mbuf_t * m,
                                 CGenNode * node,
                                 bool single_port){

    /* retrieve size of rx-check header, must be multiple of 8 */
    uint16_t opt_len =  RX_CHECK_LEN;
    uint16_t old_ipv4_header_len =  0;
    assert( (opt_len % 8) == 0 );

    m->l3_len += RX_CHECK_LEN;

    /* determine starting move location */
    char *mp1 = rte_pktmbuf_mtod(m, char*);
    uint16_t mp1_offset = m_pkt_indication.getFastIpOffsetFast();
    if (unlikely (m_pkt_indication.is_ipv6()) ) {
        mp1_offset += IPv6Header::DefaultSize;
    }else{
        mp1_offset += IPHeader::DefaultSize;
    }
    char *move_from = mp1 + mp1_offset;

    /* determine size of new mbuf required */
    uint16_t move_len = m->data_len - mp1_offset;
    uint16_t new_mbuf_size = move_len + opt_len;
    uint16_t mp2_offset = opt_len;

    /* obtain a new mbuf */
    rte_mbuf_t * new_mbuf = CGlobalInfo::pktmbuf_alloc(node->get_socket_id(), new_mbuf_size);
    assert(new_mbuf);
    char * mp2 = rte_pktmbuf_append(new_mbuf, new_mbuf_size);
    char * move_to = mp2 + mp2_offset;

    /* move part of packet from first mbuf to new mbuf */
    memmove(move_to, move_from, move_len);

    /* trim first mbuf and set pointer to option header*/
    CRx_check_header *rxhdr;
    uint16_t buf_adjust = move_len;
    rxhdr = (CRx_check_header *)mp2;
    m->data_len -= buf_adjust;

    /* insert rx-check data as an IPv4 option header or IPv6 extension header*/
    CFlowPktInfo *  lp=node->m_pkt_info;
    CPacketDescriptor   * desc=&lp->m_pkt_indication.m_desc;

    /* set option type and update ip header length */
    IPHeader * ipv4=(IPHeader *)(mp1 + 14);
    if (unlikely (m_pkt_indication.is_ipv6()) ) {
        IPv6Header * ipv6=(IPv6Header *)(mp1 + 14);
        uint8_t save_header= ipv6->getNextHdr();
        ipv6->setNextHdr(RX_CHECK_V6_OPT_TYPE);
         ipv6->setHopLimit(TTL_RESERVE_DUPLICATE);
        ipv6->setTrafficClass(ipv6->getTrafficClass() | TOS_GO_TO_CPU);
        ipv6->setPayloadLen( ipv6->getPayloadLen() +
                                  sizeof(CRx_check_header));
        rxhdr->m_option_type = save_header;
        rxhdr->m_option_len = RX_CHECK_V6_OPT_LEN;
    }else{
        old_ipv4_header_len = ipv4->getHeaderLength();
        ipv4->setHeaderLength(old_ipv4_header_len+opt_len);
        ipv4->setTotalLength(ipv4->getTotalLength()+opt_len);
        ipv4->setTimeToLive(TTL_RESERVE_DUPLICATE);
        ipv4->setTOS(ipv4->getTOS() | TOS_GO_TO_CPU);

        rxhdr->m_option_type = RX_CHECK_V4_OPT_TYPE;
        rxhdr->m_option_len = RX_CHECK_V4_OPT_LEN;
    }

    /* fill in the rx-check metadata in the options header */
    if ( CGlobalInfo::m_options.is_rxcheck_const_ts() ){
        /* Runtime flag to use a constant value for the timestamp field. */
        /* This is used by simulation to provide consistency across runs. */
        rxhdr->m_time_stamp = 0xB3B2B1B0;
    }else{
        rxhdr->m_time_stamp = os_get_hr_tick_32();
    }
    rxhdr->m_magic      = RX_CHECK_MAGIC;
    rxhdr->m_flow_id     = node->m_flow_id | ( ( (uint64_t)(desc->getFlowId() & 0xf))<<52 ) ; // include thread_id, node->flow_id, sub_flow in case of multi-flow template
    rxhdr->m_flags       =  0;
    rxhdr->m_aging_sec   =  desc->GetMaxFlowTimeout();
    rxhdr->m_template_id    = (uint8_t)desc->getId();

    /* add the flow packets goes to the same port */
    if (single_port) {
        rxhdr->m_pkt_id     = desc->getFlowPktNum();
        rxhdr->m_flow_size  = desc->GetMaxPktsPerFlow();

    }else{
        rxhdr->m_pkt_id     = desc->GetDirInfo()->GetPktNum();
        rxhdr->m_flow_size  = desc->GetDirInfo()->GetMaxPkts();
        /* set dir */
        rxhdr->set_dir(desc->IsInitSide()?1:0);
        rxhdr->set_both_dir(desc->IsBiDirectionalFlow()?1:0);
    }


    /* update checksum for IPv4, split across 2 mbufs */
    if (likely ( ! m_pkt_indication.is_ipv6()) ) {
        if (CGlobalInfo::m_options.preview.getChecksumOffloadEnable()) {
            ipv4->myChecksum = 0;
            char * tcp_udp=move_to+old_ipv4_header_len-IPHeader::DefaultSize;
            /* update TCP/UDP checksum */
            if ( m_pkt_indication.m_desc.IsTcp() ) {
                TCPHeader * tcp = (TCPHeader *)(tcp_udp);
                update_tcp_cs(tcp,ipv4);
            }else {
                if ( m_pkt_indication.m_desc.IsUdp() ){
                    UDPHeader * udp =(UDPHeader *)(tcp_udp);
                    update_udp_cs(udp,ipv4);
                }else{
                }
            }

        } else {
            ipv4->updateCheckSum2((uint8_t *)ipv4, IPHeader::DefaultSize, (uint8_t *)rxhdr, old_ipv4_header_len+opt_len -IPHeader::DefaultSize);
        }
    }


    /* link new mbuf */
    new_mbuf->next = m->next;
    new_mbuf->nb_segs++;
    m->next = new_mbuf;
    m->nb_segs++;
    m->pkt_len += opt_len;
}
