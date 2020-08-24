#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include <iostream>
#include <fstream>
#include <string>

#include "common/c_common.h"
#include "utl_yaml.h"
#include "inet_pton.h"

#include "gtpu.h"

int GTPU::Encap(struct rte_mbuf *pkt)
{
    if (rte_pktmbuf_headroom(pkt) < sizeof(gtpu_header_t)) {
		return -1;
	}

	rte_ether_hdr * eth = rte_pktmbuf_mtod(pkt, rte_ether_hdr *);
	if (eth->ether_type != htons(RTE_ETHER_TYPE_IPV4)) {
		return 0;
	}
    rte_ipv4_hdr * ip = (rte_ipv4_hdr *)(eth + 1);
    
    // check whether the source is a known endpoint ip address
    auto it = m_dteid_by_endpoint.find(ip->src_addr);
    if (it == m_dteid_by_endpoint.end()) {
        return 0;
    }
    uint32_t dteid = it->second;

    ip->hdr_checksum = 0;
    if (ip->next_proto_id == IPPROTO_TCP) {
        rte_tcp_hdr * tcp = (rte_tcp_hdr *)(ip + 1);
        tcp->cksum = 0;
        tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
    }
    ip->hdr_checksum = rte_ipv4_cksum(ip);


    // make space for the ip+udp+gtpu headers
    rte_ether_hdr * new_eth  = (rte_ether_hdr *) rte_pktmbuf_prepend(pkt, sizeof(ip4_gtpu_header_t));

    // copy over the ethernet header
    new_eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);
    rte_ether_addr_copy(&eth->s_addr, &new_eth->s_addr);
	rte_ether_addr_copy(&eth->d_addr, &new_eth->d_addr);

    ip4_gtpu_header_t * ip4_gtpu = (ip4_gtpu_header_t *)(new_eth + 1);
    memcpy(ip4_gtpu, &m_rewrite, sizeof(ip4_gtpu_header_t));

    // ipv4    
    ip4_gtpu->ip4.total_length = htons(pkt->pkt_len - RTE_ETHER_HDR_LEN);

    // udp
    ip4_gtpu->udp.dgram_len = htons(pkt->pkt_len - RTE_ETHER_HDR_LEN - sizeof(rte_ipv4_hdr));

    // gtpu
    ip4_gtpu->gtpu.teid = dteid;
    ip4_gtpu->gtpu.length = htons(pkt->pkt_len - RTE_ETHER_HDR_LEN - sizeof(ip4_gtpu_header_t));

    // Offload the checksum calculation
    pkt->l3_len = sizeof(rte_ipv4_hdr);
    pkt->l4_len = sizeof(rte_udp_hdr);
    pkt->ol_flags &= ~PKT_TX_L4_MASK; // disable l4 checksum
    pkt->ol_flags |= PKT_TX_IP_CKSUM | PKT_TX_IPV4;
    // This also works:
    //pkt->ol_flags &= ~PKT_TX_TCP_CKSUM;
    //pkt->ol_flags |= PKT_TX_IP_CKSUM | PKT_TX_IPV4 | PKT_TX_UDP_CKSUM;

    //pkt->outer_l3_len = sizeof(rte_ipv4_hdr);
    //pkt->outer_l4_len = sizeof(rte_udp_hdr);
    //pkt->ol_flags |= PKT_TX_OUTER_IP_CKSUM | PKT_TX_OUTER_IPV4 | PKT_TX_OUTER_UDP_CKSUM;
    //pkt->tun_type = 
    return 0;
}

int GTPU::Decap(struct rte_mbuf *pkt)
{
    if (rte_pktmbuf_data_len(pkt) < sizeof(ip4_gtpu_header_t)) {
		return 0;
	}

	rte_ether_hdr * eth = rte_pktmbuf_mtod(pkt, rte_ether_hdr *);
	if (eth->ether_type != htons(RTE_ETHER_TYPE_IPV4)) {
		return 0;
	}

    // ip+udp check
    ip4_gtpu_header_t * ip4_gtpu = (ip4_gtpu_header_t *)(eth + 1);
    if (ip4_gtpu->ip4.next_proto_id != IPPROTO_UDP
        || ip4_gtpu->udp.dst_port != htons(GTPU_UDP_PORT)
    )
        return 0;

    // gtpu validation
    if ((ip4_gtpu->gtpu.ver_flags & GTPU_VER_MASK) != GTPU_V1_VER
        || (ip4_gtpu->gtpu.ver_flags & GTPU_PT_BIT) == 0
        || ip4_gtpu->gtpu.type != GTPU_TYPE_GPDU
    )
        return 0;

    // Srip off the ip+udp+gtpu headers
    rte_ether_hdr * new_eth = (rte_ether_hdr *) rte_pktmbuf_adj(pkt, sizeof(ip4_gtpu_header_t));

    // copy over the ethernet header
    new_eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);
    rte_ether_addr_copy(&eth->s_addr, &new_eth->s_addr);
	rte_ether_addr_copy(&eth->d_addr, &new_eth->d_addr);

    return 0;
}

void GTPU::Parse(YAML::Node const& node)
{
    if (!utl_yaml_read_ip_addr(node, "src_address", m_src_addr)) {
		ASSERT_MSG(false, "Can't find 'src_address' for gtpu tunnel");
	}
    if (!utl_yaml_read_ip_addr(node, "dst_address", m_dst_addr)) {
		ASSERT_MSG(false, "Can't find 'dst_address' for gtpu tunnel");
	}
    if (node.FindValue("data_file")) {
		std::string filename;
		node["data_file"] >> filename;
        if (LoadDataFile(filename) < 0)
            ASSERT_MSG(false, "Failed to load data_file for gtpu tunnel");
    } else {
        ASSERT_MSG(false, "Missing data_file name for gtpu_tunnel");
    }
}

void GTPU::Prepare()
{
    memset(&m_rewrite, 0, sizeof(ip4_gtpu_header_t));

    m_rewrite.ip4.version_ihl = 0x45;
    m_rewrite.ip4.time_to_live = 64;
    m_rewrite.ip4.next_proto_id = IPPROTO_UDP;
    m_rewrite.ip4.src_addr = htonl(m_src_addr);
    m_rewrite.ip4.dst_addr = htonl(m_dst_addr);

    m_rewrite.udp.src_port = htons(GTPU_UDP_PORT);
    m_rewrite.udp.dst_port = htons(GTPU_UDP_PORT);

    m_rewrite.gtpu.ver_flags = GTPU_V1_VER | GTPU_PT_BIT;
    m_rewrite.gtpu.type = GTPU_TYPE_GPDU;
}

std::string GTPU::GetCSVToken(std::string& s, size_t & pos)
{
    std::string token;
    size_t start = pos;

    if (pos == std::string::npos)
        return token;

    pos = s.find(',', pos);
    if (pos == std::string::npos) {
        // not found
        token = s.substr(start);
    } else {
        token = s.substr(start, pos-start);
        pos++;
    }
    return token;
}

int GTPU::LoadDataFile(std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return -1;
    }

    std::string line;
    std::string endpoint_addr, src_teid, dst_teid;
    size_t pos;
    while (std::getline(file, line))
    {
        // skip comments
        if (line.front() == '#')
            continue;

        pos = 0;
        endpoint_addr = GetCSVToken(line, pos);
        src_teid = GetCSVToken(line, pos);
        dst_teid = GetCSVToken(line, pos);

        if (endpoint_addr.empty() || src_teid.empty() || dst_teid.empty()) {
            printf("Missing token in line: %s\n", line.c_str());
            return -1;
        }
        if (AddTunnel(endpoint_addr, src_teid, dst_teid))
            return -1;
    }

    file.close();
    return 0;
}

int GTPU::AddTunnel(std::string& endpoint_addr, std::string& src_teid, std::string& dst_teid)
{
    uint32_t ip, steid, dteid;
    if (my_inet_pton4((char *)endpoint_addr.c_str(), (unsigned char *)&ip) == 0) {
        printf("Invalid endpoint IP address: %s\n", endpoint_addr.c_str());
        return -1;
    }
    steid = (uint32_t)std::stoll(src_teid);
    dteid = (uint32_t)std::stoll(dst_teid);
    // ip is already in network byte order
    m_dteid_by_endpoint[ip] = htonl(dteid);
    (void)steid;

    return 0;
}
