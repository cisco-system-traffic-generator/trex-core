#ifndef GTPU_TUNNEL_H_
#define GTPU_TUNNEL_H_

#include <cstdint>
#include <vector>

#include <mbuf.h>
#include "utl_yaml.h"
#include "inet_pton.h"
#include <common/Network/Packet/CPktCmn.h>
#include "tunnel.h"


class GTPU: public Tunnel {
public:
	GTPU();

    GTPU( uint32_t teid, uint32_t src_ip, uint32_t dst_ip) ;

    GTPU(uint32_t teid, uint8_t src_ipv6[8], uint8_t dst_ipv6[8]);

	~GTPU() {
	}


	virtual int Prepend(rte_mbuf * m, u_int16_t);

    int Prepend_ipv4_tunnel(rte_mbuf * pkt, u_int8_t l3_offset, u_int16_t);

    int Prepend_ipv6_tunnel(rte_mbuf * pkt, u_int8_t l3_offset, u_int16_t);

    int Adjust_ipv4_tunnel( rte_mbuf * pkt, struct rte_ether_hdr const *eth);

    int Adjust_ipv6_tunnel( rte_mbuf * pkt, struct rte_ether_hdr const *eth);

	virtual int Adjust(rte_mbuf * m, u_int8_t);

    static int Parse(YAML::Node const&, std::vector<Tunnel *> & tunnels);

    static void  parse_ipv6(const YAML::Node &node, const std::string &name, unsigned char *ip_num) {
    try {
        std::string ip_str;

        node[name] >> ip_str;
        int rc = my_inet_pton6((char *)ip_str.c_str(), ip_num);
        if (!rc) {
            printf("Invalid IPv6 address: " );
        }
        // we want host order
        for (int i = 0; i < 8; i++) {
            ((uint16_t *) ip_num)[i] = PKT_NTOHS(((uint16_t *) ip_num)[i]);
        }
        return;

    } catch (const YAML::InvalidScalar &ex) {
        printf("Expecting valid IPv6 address for field '");

    } catch (const YAML::KeyNotFound &ex) {
        printf("Can not locate mandatory field '");
    }
   }


private:
	uint32_t teid;
    uint32_t dst_ip;
    uint32_t src_ip;
    uint32_t key;
    uint8_t *m_outer_hdr;
    bool m_ipv6_set;
};

#endif /* GTPU_TUNNEL_H_*/
