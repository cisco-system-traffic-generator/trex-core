/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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

#include <trex_stateless_dp_core.h>
#include <stdio.h>
#include <unistd.h>
#include <trex_stateless.h>

#include <bp_sim.h>

#ifndef TREX_RPC_MOCK_SERVER

// DPDK c++ issue 
#define UINT8_MAX 255
#define UINT16_MAX 0xFFFF
// DPDK c++ issue 
#endif

#include <rte_ethdev.h>
#include "mbuf.h"

/**
 * TEST
 * 
 */
static const uint8_t udp_pkt[]={ 
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x00,0x00,0x00,0x01,0x00,0x00,
    0x08,0x00,

    0x45,0x00,0x00,0x81,
    0xaf,0x7e,0x00,0x00,
    0x12,0x11,0xd9,0x23,
    0x01,0x01,0x01,0x01,
    0x3d,0xad,0x72,0x1b,

    0x11,0x11,
    0x11,0x11,

    0x00,0x6d,
	0x00,0x00,

    0x64,0x31,0x3a,0x61,
    0x64,0x32,0x3a,0x69,0x64,
    0x32,0x30,0x3a,0xd0,0x0e,
    0xa1,0x4b,0x7b,0xbd,0xbd,
    0x16,0xc6,0xdb,0xc4,0xbb,0x43,
    0xf9,0x4b,0x51,0x68,0x33,0x72,
    0x20,0x39,0x3a,0x69,0x6e,0x66,0x6f,
    0x5f,0x68,0x61,0x73,0x68,0x32,0x30,0x3a,0xee,0xc6,0xa3,
    0xd3,0x13,0xa8,0x43,0x06,0x03,0xd8,0x9e,0x3f,0x67,0x6f,
    0xe7,0x0a,0xfd,0x18,0x13,0x8d,0x65,0x31,0x3a,0x71,0x39,
    0x3a,0x67,0x65,0x74,0x5f,0x70,0x65,0x65,0x72,0x73,0x31,
    0x3a,0x74,0x38,0x3a,0x3d,0xeb,0x0c,0xbf,0x0d,0x6a,0x0d,
    0xa5,0x31,0x3a,0x79,0x31,0x3a,0x71,0x65,0x87,0xa6,0x7d,
    0xe7
};

static int
test_inject_pkt(uint8_t *pkt, uint32_t pkt_size) {

    #ifndef TREX_RPC_MOCK_SERVER
        rte_mempool_t * mp= CGlobalInfo::m_mem_pool[0].m_big_mbuf_pool ;
    #else
        rte_mempool_t * mp = NULL;
    #endif

    rte_mbuf_t *m = rte_pktmbuf_alloc(mp);
    if ( unlikely(m==0) )  {
        printf("ERROR no packets \n");
        return (-1);
    }
    char *p = rte_pktmbuf_append(m, pkt_size);
    assert(p);
    /* set pkt data */
    memcpy(p,pkt,pkt_size);

    rte_mbuf_t *tx_pkts[32];
    tx_pkts[0] = m;
    uint8_t nb_pkts = 1;
    uint16_t ret = rte_eth_tx_burst(0, 0, tx_pkts, nb_pkts);
    (void)ret;
    rte_pktmbuf_free(m);

    return (0);
}

static int 
test_inject_udp_pkt(){
    return (test_inject_pkt((uint8_t*)udp_pkt,sizeof(udp_pkt)));
}

void
TrexStatelessDpCore::test_inject_dummy_pkt() {
    test_inject_udp_pkt();
}

/***************************
 * DP core
 * 
 **************************/
TrexStatelessDpCore::TrexStatelessDpCore(uint8_t core_id) : m_core_id(core_id) {
}

/**
 * main function for DP core
 * 
 */
void
TrexStatelessDpCore::run() {
    printf("\nOn DP core %d\n", m_core_id);
    while (true) {
        test_inject_dummy_pkt();
        rte_pause();
    }
}

