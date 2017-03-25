/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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
/*
This file is for testing devices implementing NAT.
For each flow, we need to learn the NAT translation in order to send server responses.
Algorithm is as described below:
We send first packet from client, and look at it on other side to see how it was changed. Then we configure
the server flow with the appropriate change. To keep track of which flow the packet belongs to, we attach to the
first packet of each flow, flow id and thread id (thread handling the flow).
Information attaching method can be chosen be using --learn-mode option.
The information is attached either in special IP option, or in ACK number of first SYN packet if protocol is TCP
and in IP_ID of first flow packet if it is UDP.
*/

#include <stdint.h>
#include <common/basic_utils.h>
#include "nat_check.h"
#include "bp_sim.h"

void CGenNodeNatInfo::dump(FILE *fd){

    fprintf(fd," msg_type :  %d \n",m_msg_type);
    int i;
    for (i=0; i<m_cnt; i++) {
        CNatFlowInfo * lp=&m_data[i];
        fprintf (fd," id:%d , external ip:%08x , ex_port: %04x , TCP seq:%x fid: %d \n"
                 , i, lp->m_external_ip, lp->m_external_port, lp->m_tcp_seq, lp->m_fid);
    }
}

void CGenNodeNatInfo::init(){
    m_msg_type= CGenNodeMsgBase::NAT_FIRST;
    m_pad=0;
    m_cnt=0;
}

void CNatStats::reset(){
    m_total_rx=0;
    m_total_msg=0;
    m_err_no_valid_thread_id=0;
    m_err_no_valid_proto=0;
    m_err_queue_full=0;
}


CNatPerThreadInfo * CNatRxManager::get_thread_info(uint8_t thread_id){
    if (thread_id<m_max_threads) {
        return (&m_per_thread[thread_id]);
    }
    m_stats.m_err_no_valid_thread_id++;
    return (0);
}


bool CNatRxManager::Create(){

    m_max_threads = CMsgIns::Ins()->get_num_threads() ;
    m_per_thread = new CNatPerThreadInfo[m_max_threads];
    CMessagingManager * lpm=CMsgIns::Ins()->getRxDp();

    int i;
    for (i=0; i<m_max_threads; i++) {
        m_per_thread[i].m_ring=lpm->getRingCpToDp(i);
        assert(m_per_thread[i].m_ring);
    }



    return (true);
}

void CNatRxManager::Delete(){
    if (m_per_thread) {
        delete m_per_thread;
        m_per_thread=0;
    }
}

void delay(int msec);



/* check this every 1msec */
void CNatRxManager::handle_aging(){
    int i;
    dsec_t now=now_sec();
    for (i=0; i<m_max_threads; i++) {
        CNatPerThreadInfo * thread_info=get_thread_info( i );
        if ( thread_info->m_cur_nat_msg ){
            if ( now - thread_info->m_last_time > MAX_TIME_MSG_IN_QUEUE_SEC ){
                flush_node(thread_info);
            }
        }
    }
}

void CNatRxManager::flush_node(CNatPerThreadInfo * thread_info){
        // try send
        int cnt=0;
        while (true) {
            if ( thread_info->m_ring->Enqueue((CGenNode*)thread_info->m_cur_nat_msg) == 0 ){
                #ifdef NAT_TRACE_
                printf("send message \n");
                #endif
                break;
            }
            m_stats.m_err_queue_full++;
            delay(1);
            cnt++;
            if (cnt>10) {
                printf(" ERROR  queue from rx->dp is stuck, somthing is wrong here \n");
                exit(1);
            }
        }
        /* msg will be free by sink */
        thread_info->m_cur_nat_msg=0;
}

void CNatRxManager::get_info_from_tcp_ack(uint32_t tcp_ack, uint32_t &fid, uint8_t &thread_info) {
    thread_info = (uint8_t) tcp_ack;
    fid = (tcp_ack >> 8) & NAT_FLOW_ID_MASK_TCP_ACK;
}

void CNatRxManager::get_info_from_ip_id(uint16_t ip_id, uint32_t &fid, uint8_t &thread_info) {
    thread_info = ((uint8_t) ip_id) & 0x3f;
    fid = (ip_id >> 6) & NAT_FLOW_ID_MASK_IP_ID;
}

/*
 * We handle NAT info. Extracting IP src/dst and protocol from the packet.
 * Adding the information to a msg to be sent to the thread handling this flow.
 * Thread and flow info is extracted from IP packet. Either from IP option or TCP ACK.
 * Parameters:
 *   option - pointer to our proprietary NAT info IP option.
 *      If it is NULL, the NAT info is in the TCP ACK number
 *   ipv4 - pointer to ipv4 header to extract info from.
 *   is_first - Is this the first packet of the flow or second. To handle firewalls that do
 *              TCP seq randomization on the server->client side, we also look at the second
 *              packet of the flow (SYN+ACK), and extract its seq num.
 */
void CNatRxManager::handle_packet_ipv4(CNatOption *option, IPHeader *ipv4, bool is_first) {
    CNatPerThreadInfo * thread_info;
    uint32_t fid=0;
    uint32_t tcp_seq;


    uint32_t ext_ip   = ipv4->getSourceIp();
    uint8_t proto     = ipv4->getProtocol();
    TCPHeader *tcp = (TCPHeader *) (((char *)ipv4)+ ipv4->getHeaderLength());
    UDPHeader *udp = (UDPHeader *) (((char *)ipv4)+ ipv4->getHeaderLength());;
    uint16_t ext_port;

    switch(proto) {
    case IPPROTO_TCP:
        ext_port = tcp->getSourcePort();
        tcp_seq = tcp->getSeqNumber();
        break;
    case IPPROTO_UDP:
        ext_port = udp->getSourcePort();
        tcp_seq = 0;
        break;
    default:
        m_stats.m_err_no_valid_proto++;
        return;
    }

    if (option) {
        thread_info = get_thread_info(option->get_thread_id());
        fid = option->get_fid();
    } else {
        uint8_t thread_id;

        if (is_first) {
            if (proto == IPPROTO_TCP) {
                uint32_t tcp_ack = tcp->getAckNumber();
                get_info_from_tcp_ack(tcp_ack, fid, thread_id);
                thread_info = get_thread_info(thread_id);
                if (CGlobalInfo::is_learn_mode(CParserOption::LEARN_MODE_TCP_ACK)) {
                    uint32_t dst_ip = ipv4->getDestIp();
                    uint16_t dst_port = tcp->getDestPort();
                    uint64_t map_key = (dst_ip << 16) + dst_port;
                    double time_stamp = now_sec();
                    m_ft.insert(map_key, tcp_ack, time_stamp);
                    m_ft.clear_old(time_stamp - 10);
                }
            } else {
                uint16_t ip_id = ipv4->getId();
                get_info_from_ip_id(ip_id, fid, thread_id);
                thread_info = get_thread_info(thread_id);
            }
        } else {
            uint32_t val;
            // server->client packet. IP/port reversed in regard to first SYN packet
            uint64_t map_key = (ext_ip << 16) + ext_port;
            if (m_ft.erase(map_key, val)) {
                get_info_from_tcp_ack(val, fid, thread_id);
                thread_info = get_thread_info(thread_id);
            } else {
                // flow was not found in the table
                thread_info = 0;
            }
        }
    }


    if (unlikely(!thread_info)) {
        return;
    }

#ifdef NAT_TRACE_
    printf("rx msg ext ip: %08x ext port: %04x TCP Seq: %08x flow_id : %d (%s) \n", ext_ip, ext_port, tcp_seq, fid
           , is_first ? "first":"second");
#endif

    CGenNodeNatInfo * node=thread_info->m_cur_nat_msg;
    if ( !node ){
        node = (CGenNodeNatInfo * )CGlobalInfo::create_node();
        assert(node);
        node->init();
        thread_info->m_cur_nat_msg = node;
        thread_info->m_last_time   = now_sec();
    }
    /* get message */
    CNatFlowInfo * msg=node->get_next_msg();

    /* fill the message */
    if (is_first) {
        msg->m_external_ip   = ext_ip;
        msg->m_external_port = ext_port;
    } else {
        msg->m_external_port = TCPHeader::TCP_INVALID_PORT;
    }
    msg->m_tcp_seq = tcp_seq;
    msg->m_fid           = fid;
    msg->m_pad           = 0xee;

    if ( node->is_full() ){
        flush_node(thread_info);
    }
}


#define MYDP(f) if (f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)f)
#define MYDP_A(f)     fprintf(fd," %-40s: %llu \n",#f, (unsigned long long)f)



void CNatStats::Dump(FILE *fd){
   MYDP(m_total_rx);
   MYDP(m_total_msg);
   MYDP(m_err_no_valid_thread_id);
   MYDP(m_err_no_valid_proto);
   MYDP(m_err_queue_full);
}


void CNatRxManager::Dump(FILE *fd){
    m_stats.Dump(fd);
    m_ft.dump(fd);
}

void CNatRxManager::DumpShort(FILE *fd){
    fprintf(fd,"nat check msgs: %lu,  errors: %lu  \n",m_stats.m_total_msg,m_stats.get_errs() );
}

void CNatOption::dump(FILE *fd) {
    fprintf(fd,"  op        : %x \n",get_option_type());
    fprintf(fd,"  ol        : %x \n",get_option_len());
    fprintf(fd,"  thread_id : %x \n",get_thread_id());
    fprintf(fd,"  magic     : %x \n",get_magic());
    fprintf(fd,"  fid       : %x \n",get_fid());
    utl_DumpBuffer(stdout,(void *)&u.m_data[0],8,0);
}
