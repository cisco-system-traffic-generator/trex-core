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
#include <rte_config.h>
#include <dpdk_drv_filter.h>
#include <main_dpdk.h>


#define MAX_PATTERN_NUM 5

static struct rte_flow * filter_tos_flow_to_rq(uint8_t port_id,
                                               uint8_t rx_q,
                                               bool ipv6,
                                               struct rte_flow_error *error){
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];
	struct rte_flow *flow = NULL;
	struct rte_flow_action_queue queue = { .index = rx_q };
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item_eth eth_mask;
	struct rte_flow_item_vlan vlan_spec;
	struct rte_flow_item_vlan vlan_mask;
	struct rte_flow_item_ipv4 ipv4_spec;
	struct rte_flow_item_ipv4 ipv4_mask;

    struct rte_flow_item_ipv6 ipv6_spec;
    struct rte_flow_item_ipv6 ipv6_mask;

	int res;

	memset(pattern, 0, sizeof(pattern));
	memset(action, 0, sizeof(action));

	/*
	 * set the rule attribute.
	 * in this case only ingress packets will be checked.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;

	/*
	 * create the action sequence.
	 * one action only,  move packet to queue
	 */

	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = &queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;

    int pattern_index = 0;
	/*
	 * set the first level of the pattern (eth).
	 * since in this example we just want to get the
	 * ipv4 we set this level to allow all.
	 */
	memset(&eth_spec, 0, sizeof(struct rte_flow_item_eth));
	memset(&eth_mask, 0, sizeof(struct rte_flow_item_eth));
	eth_spec.type = 0;
	eth_mask.type = 0;
	pattern[pattern_index].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[pattern_index].spec = &eth_spec;
	pattern[pattern_index].mask = &eth_mask;

    pattern_index++;
	/*
	 * setting the second level of the pattern (vlan).
	 * since in this example we just want to get the
	 * ipv4 we also set this level to allow all.
	 */
	memset(&vlan_spec, 0, sizeof(struct rte_flow_item_vlan));
	memset(&vlan_mask, 0, sizeof(struct rte_flow_item_vlan));
	pattern[pattern_index].type = RTE_FLOW_ITEM_TYPE_VLAN;
	pattern[pattern_index].spec = &vlan_spec;
	pattern[pattern_index].mask = &vlan_mask;

    pattern_index++;

    if (ipv6){
        memset(&ipv6_spec, 0, sizeof(struct rte_flow_item_ipv6));
        memset(&ipv6_mask, 0, sizeof(struct rte_flow_item_ipv6));
        ipv6_spec.hdr.vtc_flow = PAL_NTOHL(0x1<<20);
        ipv6_mask.hdr.vtc_flow = PAL_NTOHL(0x1<<20);
        pattern[pattern_index].type = RTE_FLOW_ITEM_TYPE_IPV6;
        pattern[pattern_index].spec = &ipv6_spec;
        pattern[pattern_index].mask = &ipv6_mask;

    }else{
        /*
         * setting the third level of the pattern (ip).
         * in this example this is the level we care about
         * so we set it according to the parameters.
         */
        memset(&ipv4_spec, 0, sizeof(struct rte_flow_item_ipv4));
        memset(&ipv4_mask, 0, sizeof(struct rte_flow_item_ipv4));
        ipv4_spec.hdr.type_of_service = 0x1;
        ipv4_mask.hdr.type_of_service = 0x1;
        pattern[pattern_index].type = RTE_FLOW_ITEM_TYPE_IPV4;
        pattern[pattern_index].spec = &ipv4_spec;
        pattern[pattern_index].mask = &ipv4_mask;
    }

    pattern_index++;
	/* the final level must be always type end */
	pattern[pattern_index].type = RTE_FLOW_ITEM_TYPE_END;

	res = rte_flow_validate(port_id, &attr, pattern, action, error);
	if (!res)
		flow = rte_flow_create(port_id, &attr, pattern, action, error);

	return flow;
}

static struct rte_flow * filter_drop_all(uint8_t port_id,
                                         struct rte_flow_error *error){
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];
	struct rte_flow *flow = NULL;
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item_eth eth_mask;

	int res;

	memset(pattern, 0, sizeof(pattern));
	memset(action, 0, sizeof(action));

	/*
	 * set the rule attribute.
	 * in this case only ingress packets will be checked.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;
    //attr.group =1;  /* not supported yet*/

	/*
	 * create the action sequence.
	 * one action only,  move packet to queue
	 */

    action[0].type = RTE_FLOW_ACTION_TYPE_DROP;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;

	/*
	 * set the first level of the pattern (eth).
	 * since in this example we just want to get the
	 * ipv4 we set this level to allow all.
	 */
	memset(&eth_spec, 0, sizeof(struct rte_flow_item_eth));
	memset(&eth_mask, 0, sizeof(struct rte_flow_item_eth));
	eth_spec.type = 0;
	eth_mask.type = 0;
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[0].spec = &eth_spec;
	pattern[0].mask = &eth_mask;

	/* the final level must be always type end */
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

	res = rte_flow_validate(port_id, &attr, pattern, action, error);
	if (!res)
		flow = rte_flow_create(port_id, &attr, pattern, action, error);

	return flow;
}

static struct rte_flow * filter_pass_all_to_rx(uint8_t port_id,
                                               uint8_t rx_q,
                                               struct rte_flow_error *error){
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];
	struct rte_flow *flow = NULL;
    struct rte_flow_action_queue queue = { .index = rx_q };
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item_eth eth_mask;

	int res;

	memset(pattern, 0, sizeof(pattern));
	memset(action, 0, sizeof(action));

	/*
	 * set the rule attribute.
	 * in this case only ingress packets will be checked.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;

	/*
	 * create the action sequence.
	 * one action only,  move packet to queue
	 */

    action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    action[0].conf = &queue;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;

	/*
	 * set the first level of the pattern (eth).
	 * since in this example we just want to get the
	 * ipv4 we set this level to allow all.
	 */
	memset(&eth_spec, 0, sizeof(struct rte_flow_item_eth));
	memset(&eth_mask, 0, sizeof(struct rte_flow_item_eth));
	eth_spec.type = 0;
	eth_mask.type = 0;
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	pattern[0].spec = &eth_spec;
	pattern[0].mask = &eth_mask;

	/* the final level must be always type end */
	pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

	res = rte_flow_validate(port_id, &attr, pattern, action, error);
	if (!res)
		flow = rte_flow_create(port_id, &attr, pattern, action, error);

	return flow;
}

inline void check_dpdk_filter_result(struct rte_flow * flow,
                                     struct rte_flow_error * err){
    if (flow==NULL) {
        printf("Failed to create dpdk flow rule msg: %s\n",err->message);
        exit(1);
    }
}

inline void check_dpdk_filter_result(int res,
                                     struct rte_flow_error * err){
    if (res!=0) {
        printf("Failed to delete dpdk flow rule msg: %s\n",err->message);
        exit(1);
    }
}

void CDpdkFilterPort::clear_filter(struct rte_flow * &  filter){
    struct rte_flow_error error;
    int res;
    assert(filter);
    res=rte_flow_destroy(m_repid,filter,&error);
    check_dpdk_filter_result(res,&error);
    filter=0;
}

void CDpdkFilterPort::set_tos_filter(bool enable){
    struct rte_flow_error error;
    if (enable) {
        m_rx_ipv4_tos = filter_tos_flow_to_rq(m_repid,
                                              m_rx_q,
                                              false,
                                              &error);
        check_dpdk_filter_result(m_rx_ipv4_tos,&error);
        m_rx_ipv6_tos = filter_tos_flow_to_rq(m_repid,
                                              m_rx_q,
                                              true,
                                              &error);
        check_dpdk_filter_result(m_rx_ipv6_tos,&error);
    }else{
        clear_filter(m_rx_ipv4_tos);
        clear_filter(m_rx_ipv6_tos);
    }
}

void CDpdkFilterPort::set_drop_all_filter(bool enable){
    struct rte_flow_error error;
    if (enable){
        m_rx_drop_all = filter_drop_all(m_repid,&error);
        check_dpdk_filter_result(m_rx_drop_all,&error);
    }else{
        clear_filter(m_rx_drop_all);
    }
}

void CDpdkFilterPort::set_pass_all_filter(bool enable){
    struct rte_flow_error error;
    if (enable){
        m_rx_pass_rx_all = filter_pass_all_to_rx(m_repid,m_rx_q,&error);
        check_dpdk_filter_result(m_rx_pass_rx_all,&error);
    }else{
        clear_filter(m_rx_pass_rx_all);
    }
}


void CDpdkFilterPort::_set_mode(dpdk_filter_mode_t mode,
                                bool enable){
    switch (mode) {
    case mfDISABLE:
        break;
    case mfPASS_ALL_RX:
        set_pass_all_filter(enable);
        break;
    case mfDROP_ALL_PASS_TOS:
        set_tos_filter(enable);
        set_drop_all_filter(enable);
        break;
    case mfPASS_TOS:
        set_tos_filter(enable);
        break;
    default:
        assert(0);
    };
}


void CDpdkFilterPort::set_mode(dpdk_filter_mode_t mode){

    if (mode==m_mode) {
        /* nothing to do */
        return;
    }
    _set_mode(m_mode,false); /* disable old mode */
    _set_mode(mode,true);    /* enable new mode */
    m_mode = mode;
}


CDpdkFilterPort::CDpdkFilterPort(){
    m_repid=255;
    m_rx_q = 255;
    m_drop_all=false;
    m_mode = mfDISABLE;
    m_rx_ipv4_tos=0;
    m_rx_ipv6_tos=0;
    m_rx_drop_all=0;
    m_rx_pass_rx_all=0;
}

CDpdkFilterPort::~CDpdkFilterPort(){
    set_mode(mfDISABLE);
}


CDpdkFilterPort * CDpdkFilterManager::get_port(repid_t repid){
    CDpdkFilterPort * lp=m_port[repid];
    if (lp) {
        return(lp);
    }
    lp = new CDpdkFilterPort();
    lp->set_port_id(repid);
    lp->set_drop_all_mode(get_is_tcp_mode()?false:true);
    lp->set_rx_q(MAIN_DPDK_RX_Q);
    m_port[repid]=lp;
    return(lp);
}




