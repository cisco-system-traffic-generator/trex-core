#ifndef _GTPU_H_
#define _GTPU_H_

#include <rte_common.h>
#include <string>
#include <map>

#include "tunnel.h"

typedef struct
{
  uint8_t ver_flags;
  uint8_t type;
  uint16_t length;			/* length in octets of the data following the fixed part of the header */
  uint32_t teid;
} gtpu_header_t;

#define GTPU_V1_HDR_LEN   8

#define GTPU_VER_MASK (7<<5)
#define GTPU_PT_BIT   (1<<4)
#define GTPU_E_BIT    (1<<2)
#define GTPU_S_BIT    (1<<1)
#define GTPU_PN_BIT   (1<<0)
#define GTPU_E_S_PN_BIT  (7<<0)

#define GTPU_V1_VER   (1<<5)

#define GTPU_TYPE_GPDU		    255

#define GTPU_UDP_PORT   2152

typedef struct __attribute__((packed))
{
  rte_ipv4_hdr ip4;            /* 20 bytes */
  rte_udp_hdr udp;             /* 8 bytes */
  gtpu_header_t gtpu;	       /* 8 bytes */
} ip4_gtpu_header_t;

class GTPU: public Tunnel {
public:
    GTPU(YAML::Node const& node) {
        Parse(node);
        Prepare();
    }
    ~GTPU() {}

    int Encap(struct rte_mbuf *pkt) override;
    int Decap(struct rte_mbuf *pkt) override;
private:
    uint32_t m_src_addr, m_dst_addr;
    ip4_gtpu_header_t m_rewrite;
    std::map<uint32_t, uint32_t> m_dteid_by_endpoint;

    void Parse(YAML::Node const&);
    void Prepare();
    std::string GetCSVToken(std::string& s, size_t & pos);
    int LoadDataFile(std::string &filename);
    int AddTunnel(std::string& endpoint_addr, std::string& src_teid, std::string& dst_teid);
};

#endif
