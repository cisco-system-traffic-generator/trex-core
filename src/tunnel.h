/*
 * nvgre.h
 *
 *  Created on: Aug 10, 2017
 *      Author: lxu
 */

#ifndef NVGRE_H_
#define NVGRE_H_

#include <cstdint>
#include <vector>

#include <rte_ether.h>

struct rte_mbuf;
namespace YAML {
class Node;
}

class Tunnel {
public:
	virtual ~Tunnel() {
	}

	virtual int Prepend(rte_mbuf * m) = 0;
	virtual int Adjust(rte_mbuf * m) = 0;

	static int Parse(YAML::Node const&, std::vector<Tunnel *> & tunnels);

	static uint16_t RxCallback(uint8_t port, uint16_t queue,
			struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
			void *user_param);

	static uint16_t TxCallback(uint8_t port, uint16_t queue,
			struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param);

	static void InstallRxCallback(uint8_t port, uint16_t queue);

	static void InstallTxCallback(uint8_t port, uint16_t queue);
};

class NVGRE: public Tunnel {
public:
	NVGRE();

	NVGRE(uint32_t key, uint32_t dst_ip, uint32_t src_ip,
			ether_addr const& overlay_dst_mac,
			ether_addr const& overlay_src_mac);

	~NVGRE() {
	}

	virtual int Prepend(rte_mbuf * m);

	virtual int Adjust(rte_mbuf * m);

	static int Parse(YAML::Node const&, std::vector<Tunnel *> & tunnels);

private:
	uint32_t m_tun_id;
	uint32_t m_tun_dst;
	uint32_t m_tun_src;
	ether_addr m_overlay_src_mac;
	ether_addr m_overlay_dst_mac;
	uint16_t m_ip_ident_seed;
};

#endif /* NVGRE_H_ */
