#include <rte_ethdev.h>
#include <rte_ip.h>

#include "common/c_common.h"
#include "utl_yaml.h"
#include "trex_global.h"

#include "gtpu.h"
#include "tunnel.h"

void Tunnel::InstallRxCallback(uint16_t port_id, uint16_t queue)
{
	Tunnel *t =	CGlobalInfo::m_options.m_mac_addr[port_id].m_tunnel;
	if (!t) {
		return;
	}

	if (NULL == rte_eth_add_rx_callback(port_id, queue, Tunnel::RxCallback, (void *)t)) {
		printf("failed to install RX callback at %u:%u.\n", port_id, queue);
		return;
	}
	printf("installed RX callback at %u:%u.\n", port_id, queue);
}

void Tunnel::InstallTxCallback(uint16_t port_id, uint16_t queue)
{
	Tunnel *t =	CGlobalInfo::m_options.m_mac_addr[port_id].m_tunnel;
	if (!t) {
		return;
	}

	if (NULL == rte_eth_add_tx_callback(port_id, queue, Tunnel::TxCallback, (void *)t)) {
		printf("failed to install TX callback at %u:%u.\n", port_id, queue);
		return;
	}
	printf("installed TX callback at %u:%u.\n", port_id, queue);
}

uint16_t Tunnel::RxCallback(uint16_t port_id, uint16_t queue,
		struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
		void *user_param)
{
	//Tunnel *t =	CGlobalInfo::m_options.m_mac_addr[port_id].m_tunnel;
	Tunnel *t =	(Tunnel *)user_param;
	if (!t) {
		return nb_pkts;
	}
    for (uint16_t i = 0; i < nb_pkts; i++) {
        rte_mbuf * pkt = pkts[i];
        t->Decap(pkt);
    }
    return nb_pkts;
}

uint16_t Tunnel::TxCallback(uint16_t port_id, uint16_t queue,
		struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param)
{
	//Tunnel *t =	CGlobalInfo::m_options.m_mac_addr[port_id].m_tunnel;
	Tunnel *t =	(Tunnel *)user_param;
	if (!t) {
		return nb_pkts;
	}
    for (uint16_t i = 0; i < nb_pkts; i++) {
        rte_mbuf * pkt = pkts[i];
        t->Encap(pkt);
    }
    return nb_pkts;
}

Tunnel* Tunnel::Parse(YAML::Node const& node)
{
    if (node.FindValue("type")) {
		std::string type;
		node["type"] >> type;
		if (type == "gtpu") {
			return static_cast<Tunnel *>(new GTPU(node));
		} else {
			ASSERT_MSG(false, "Unknown tunnel type.");
		}
	}
	return NULL;
}

