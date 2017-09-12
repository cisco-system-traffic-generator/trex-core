/*
 * nvgre.cpp
 *
 *  Created on: Aug 10, 2017
 *      Author: lxu
 */

#include <rte_byteorder.h>
#include <rte_ip.h>
#include <rte_random.h>
#include <rte_mbuf.h>
#include <rte_hash_crc.h>

#include "tunnel.h"

#include "bp_sim.h"
#include "utl_yaml.h"

#define GRE_FLAGS_VERSION 0x2000
#define ETH_TYPE_TEB 0x6558

struct gre_hdr {
	uint16_t flags_version;  //0x2000
	uint16_t proto;	//ETH_P_TEB 0x6558
	uint32_t key;
}__attribute__((__packed__));

struct Encapsulation {
	ether_hdr eth;
	ipv4_hdr ip;
	gre_hdr gre;
}__attribute__((__packed__));

static const uint8_t IPV4_HDR_VERSION_IHL = 0x45;
static const uint8_t IPV4_HDR_TTL = 64;
static constexpr uint16_t kEtherTypeBeIPv4 = __builtin_bswap16(
		(ETHER_TYPE_IPv4));

static const uint16_t kBeGreFlagsVersion = rte_cpu_to_be_16(GRE_FLAGS_VERSION);
static const uint16_t kBeEthTypeTEB = rte_cpu_to_be_16(ETH_TYPE_TEB);

static inline uint8_t ipv4_header_length(struct ipv4_hdr const* ipv4_hdr) {
	return (ipv4_hdr->version_ihl & IPV4_HDR_IHL_MASK) * IPV4_IHL_MULTIPLIER;
}

static inline void const* ipv4_body(struct ipv4_hdr const* ipv4_hdr) {
	return (void const*) ((uint8_t const*) ipv4_hdr
			+ ipv4_header_length(ipv4_hdr));
}

NVGRE::NVGRE() :
		m_tun_id(0), m_tun_dst(0), m_tun_src(0), m_overlay_src_mac(), m_overlay_dst_mac(), m_ip_ident_seed(
				0) {
	m_ip_ident_seed = rte_rand() & 0xFFFF;
}

NVGRE::NVGRE(uint32_t key, uint32_t dst, uint32_t src,
		ether_addr const& overlay_dst_mac, ether_addr const& overlay_src_mac) :
		m_tun_id(key), m_tun_dst(dst), m_tun_src(src), m_overlay_src_mac(
				overlay_src_mac), m_overlay_dst_mac(overlay_dst_mac), m_ip_ident_seed(
				0) {
	m_ip_ident_seed = rte_rand() & 0xFFFF;
}

int NVGRE::Prepend(rte_mbuf * pkt) {
	if (rte_pktmbuf_headroom(pkt) < sizeof(struct Encapsulation)) {
		return -1;
	}

	ether_hdr * overlay_eth = rte_pktmbuf_mtod(pkt, ether_hdr *);
	if (overlay_eth->ether_type != kEtherTypeBeIPv4) {
		return 0;
	}

	struct Encapsulation * encap = (struct Encapsulation *) rte_pktmbuf_prepend(
			pkt, sizeof(struct Encapsulation));
	memset(encap, 0, sizeof(*encap));

	struct ether_hdr * outer_eth = &encap->eth;
	outer_eth->ether_type = kEtherTypeBeIPv4;
	ether_addr_copy(&overlay_eth->s_addr, &outer_eth->s_addr);
	ether_addr_copy(&overlay_eth->d_addr, &outer_eth->d_addr);

	struct ipv4_hdr * outer_ipv4 = &encap->ip;
	outer_ipv4->version_ihl = IPV4_HDR_VERSION_IHL;
	outer_ipv4->type_of_service = 0;
	outer_ipv4->total_length = rte_cpu_to_be_16(pkt->pkt_len - ETHER_HDR_LEN);
	outer_ipv4->fragment_offset = 0;
	outer_ipv4->time_to_live = IPV4_HDR_TTL;
	outer_ipv4->next_proto_id = IPPROTO_GRE;
	outer_ipv4->hdr_checksum = 0;
	outer_ipv4->src_addr = m_tun_src;
	outer_ipv4->dst_addr = m_tun_dst;

	m_ip_ident_seed += 27;   // bump by a prime number
	outer_ipv4->packet_id = rte_cpu_to_be_16(m_ip_ident_seed);

	gre_hdr * gre = &encap->gre;
	gre->flags_version = kBeGreFlagsVersion;
	gre->proto = kBeEthTypeTEB;
	gre->key = rte_cpu_to_be_32(m_tun_id);

	ether_addr_copy(&m_overlay_src_mac, &overlay_eth->s_addr);
	ether_addr_copy(&m_overlay_dst_mac, &overlay_eth->d_addr);

	pkt->l3_len = sizeof(struct ipv4_hdr);
	pkt->ol_flags |= PKT_TX_IP_CKSUM | PKT_TX_IPV4;
	return 0;
}

int NVGRE::Adjust(rte_mbuf * pkt) {
	if (rte_pktmbuf_data_len(pkt) <= sizeof(struct Encapsulation)) {
		return 0;
	}

	ether_hdr const* eth = rte_pktmbuf_mtod(pkt, ether_hdr const*);
	if (eth->ether_type != kEtherTypeBeIPv4) {
		return 0;
	}

	struct ipv4_hdr const* ipv4 = (struct ipv4_hdr const*) (eth + 1);
	if (ipv4->next_proto_id != IPPROTO_GRE) {
		return 0;
	}

	gre_hdr const* gre = (gre_hdr const*) ipv4_body(ipv4);
	if (gre->proto != kBeEthTypeTEB
			|| gre->flags_version != kBeGreFlagsVersion) {
		return 0;
	}

	rte_pktmbuf_adj(pkt, sizeof(struct Encapsulation));
	return 0;
}

uint16_t Tunnel::RxCallback(uint8_t port, uint16_t queue,
		struct rte_mbuf *pkts[], uint16_t nb_pkts, uint16_t max_pkts,
		void *user_param) {
	if (0 == nb_pkts)
		return nb_pkts;

	std::vector<Tunnel *> & t =
			CGlobalInfo::m_options.m_mac_addr[port].m_tunnels;
	if (t.empty()) {
		return nb_pkts;
	}

	for (uint16_t i = 0; i < nb_pkts; i++) {
		rte_mbuf * m = pkts[i];
		t[0]->Adjust(m);
	}
	return nb_pkts;
}

uint16_t Tunnel::TxCallback(uint8_t port, uint16_t queue,
		struct rte_mbuf *pkts[], uint16_t nb_pkts, void *user_param) {
	std::vector<Tunnel *> & t =
			CGlobalInfo::m_options.m_mac_addr[port].m_tunnels;
	if (t.empty()) {
		return nb_pkts;
	}

	for (uint16_t i = 0; i < nb_pkts; i++) {
		rte_mbuf * m = pkts[i];
		ether_hdr * eth = rte_pktmbuf_mtod(m, ether_hdr *);
		ipv4_hdr * ip = (ipv4_hdr *) (eth + 1);
		uint32_t hash = rte_hash_crc_4byte(ip->src_addr, 0);
		hash = rte_hash_crc_4byte(ip->dst_addr, hash);
		Tunnel * tunnel = t[hash % t.size()];
		tunnel->Prepend(m);
	}
	return nb_pkts;
}

void Tunnel::InstallRxCallback(uint8_t port, uint16_t queue) {
	std::vector<Tunnel *> & t =
			CGlobalInfo::m_options.m_mac_addr[port].m_tunnels;
	if (t.empty()) {
		return;
	}

	if (NULL == rte_eth_add_rx_callback(port, queue, Tunnel::RxCallback, NULL)) {
		printf("failed to install RX callback at %u:%u.", port, queue);
		return;
	}
	printf("installed RX callback at %u:%u.", port, queue);
}

void Tunnel::InstallTxCallback(uint8_t port, uint16_t queue) {
	std::vector<Tunnel *> & t =
			CGlobalInfo::m_options.m_mac_addr[port].m_tunnels;
	if (t.empty()) {
		return;
	}

	if (NULL == rte_eth_add_tx_callback(port, queue, Tunnel::TxCallback, NULL)) {
		printf("failed to install TX callback at %u:%u.", port, queue);
		return;
	}
	printf("installed TX callback at %u:%u.", port, queue);
}

inline int ParseMAC(YAML::Node const& node, uint8_t mac[6]) {
	if (node.Type() == YAML::NodeType::Sequence) { // [1,2,3,4,5,6]
		ASSERT_MSG(node.size() == 6, "Array of MAC should have 6 elements.");
		uint32_t fi;
		for (unsigned i = 0; i < node.size(); i++) {
			node[i] >> fi;
			mac[i] = fi;
		}
	} else if (node.Type() == YAML::NodeType::Scalar) { // "12:34:56:78:9a:bc"
		std::string mac_str;
		node >> mac_str;
		std::vector<uint8_t> bytes;
		bool res = mac2vect(mac_str, bytes);
		ASSERT_MSG(res && bytes.size() == 6,
				"String of dest MAC should be in format '12:34:56:78:9a:bc'.");
		memcpy(mac, &bytes[0], 6);
	} else {
		return -1;
	}
	return 0;
}

extern int my_inet_pton4(const char *src, unsigned char *dst);
inline bool ParseIPv4(YAML::Node const& node, uint32_t * ip) {
	std::string ip_str;
	node >> ip_str;
	return my_inet_pton4((char *) ip_str.c_str(), (unsigned char *) ip);
}

int Tunnel::Parse(YAML::Node const& node, std::vector<Tunnel *> & tunnels) {
	if (node.FindValue("type")) {
		std::string type;
		node["type"] >> type;
		if (type == "nvgre") {
			NVGRE::Parse(node, tunnels);
		} else {
			ASSERT_MSG(false, "Unknown tunnel type. Support 'nvgre'.");
		}
	}
	return 0;
}

int NVGRE::Parse(YAML::Node const& node, std::vector<Tunnel *> & tunnels) {
	ether_addr dl_src, dl_dst;
	uint32_t tun_id, tun_dst, tun_src;

	std::string mac_str;
	if (node.FindValue("dl_src")) {
		ParseMAC(node["dl_src"], dl_src.addr_bytes);
	} else {
		ASSERT_MSG(false, "Can't find 'dl_src' in nvgre tunnel.");
	}

	if (node.FindValue("dl_dst")) {
		ParseMAC(node["dl_dst"], dl_dst.addr_bytes);
	} else {
		ASSERT_MSG(false, "Can't find 'dl_dst' in nvgre tunnel.");
	}

	if (!utl_yaml_read_uint32(node, "tun_id", tun_id)) {
		ASSERT_MSG(false, "Can't find 'tun_id' in nvgre tunnel.");
	}

	if (!utl_yaml_read_ip_addr(node, "tun_dst", tun_dst)) {
		ASSERT_MSG(false, "Can't find 'tun_dst' in nvgre tunnel.");
	}

	if (!node.FindValue("tun_src")) {
		ASSERT_MSG(false, "Can't find 'tun_src' in nvgre tunnel.");
	}
	YAML::Node const& tun_src_node = node["tun_src"];
	switch (tun_src_node.Type()) {
	case YAML::NodeType::Sequence:
		for (unsigned i = 0; i < tun_src_node.size(); i++) {
			ASSERT_MSG(ParseIPv4(tun_src_node[i], &tun_src),
					"Can't parse 'tun_src' in nvgre tunnel.");
			tunnels.push_back(
					new NVGRE(tun_id, PKT_HTONL(tun_dst), tun_src, dl_dst,
							dl_src));
		}
		break;
	case YAML::NodeType::Scalar:
		ASSERT_MSG(ParseIPv4(tun_src_node, &tun_src),
				"Can't parse 'tun_src' in nvgre tunnel.")
		tunnels.push_back(
				new NVGRE(tun_id, PKT_HTONL(tun_dst), tun_src, dl_dst, dl_src));
		break;
	default:
		ASSERT_MSG(false, "Wrong type for 'tun_src'")
	}
	return 0;
}
