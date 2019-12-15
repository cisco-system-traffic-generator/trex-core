#include "trex_vlan_filter.h"


/*************
* VlanFilter *
*************/

bool VlanFilter::is_pkt_pass(const uint8_t* pkt) {
    uint8_t m_wanted_flags = m_cfg.m_wanted_flags;

    if ( (m_wanted_flags & FilterFlags::ALL) == FilterFlags::ALL ) {
        return true;
    }

    if ( m_wanted_flags == FilterFlags::None ) {
        return false;
    }

    uint8_t *running_ptr = (uint8_t *)(pkt + 12);
    uint8_t vlans_count = 0;
    bool loop_pkt = true;
    m_pkt_flags = 0; // each bit in m_pkt_flags correspond to the filter flag
    m_total_vlans = 0;
    for (int i = 0; i < 2; i++) {
        if ( m_cfg.m_vlans[i] ) {
            m_total_vlans ++;
        }
    }
   
    while ( loop_pkt ) {
        uint16_t next_header = PAL_NTOHS(*((uint16_t *)running_ptr));

        switch( next_header ) {
            case EthernetHeader::Protocol::VLAN:
            case EthernetHeader::Protocol::QINQ:
            case 0x9100:
            case 0x9200:
            case 0x9300: {
                if ( vlans_count == 2 ) {
                    /* cannot receive more than 2 vlans */
                    return false;  
                }
                /* masking Priority & DEI */
                uint32_t masked_vlan = PAL_NTOHL(*(uint32_t *)running_ptr) & 0xFFFF0FFF;
                if ( masked_vlan != m_cfg.m_vlans[vlans_count++] ) {
                    return false;
                }
                running_ptr += 4;
                break;
            }
            case EthernetHeader::Protocol::IP: {
                IPHeader *ip_header = (IPHeader*)(running_ptr + 2);
                parse_l4(ip_header->getProtocol());
                loop_pkt = false;
                break;
            }
            case EthernetHeader::Protocol::IPv6: {
                IPv6Header *ipv6_header = (IPv6Header *)(running_ptr + 2);
                parse_l4(ipv6_header->getNextHdr());
                loop_pkt = false;
                break;
            }
            default:
                loop_pkt = false;
                break;
        }
    }
    
    if ( m_total_vlans != vlans_count ) {
        return false;
    }

    /* Turn on NO_TCP_UDP only if all others are off */
    m_pkt_flags = !(m_pkt_flags & (~FilterFlags::NO_TCP_UDP)) ? FilterFlags::NO_TCP_UDP : m_pkt_flags;

    return ( m_pkt_flags & m_wanted_flags ) == m_wanted_flags;
}

void VlanFilter::parse_l4(uint8_t protocol) {
    switch ( protocol ) {
        case IPHeader::Protocol::TCP:
        case IPHeader::Protocol::UDP:
            m_pkt_flags |= FilterFlags::TCP_UDP;
            break;
        default:
            break;
    }
}

VlanFilterCFG VlanFilter::get_cfg() {
    return m_cfg;
}

void VlanFilter::set_cfg(VlanFilterCFG &cfg) {
    m_cfg = cfg;
}

void VlanFilter::set_cfg_flag(uint8_t flag) {
    m_cfg.m_wanted_flags = flag;
}

void VlanFilter::mask_to_str(std::string &str) {

    if ( m_cfg.m_wanted_flags == FilterFlags::None ) {
        str = "none";
        return;
    }

    if ( (m_cfg.m_wanted_flags & FilterFlags::ALL) == FilterFlags::ALL ) {
        str = "all";
        return;
    }

    if ( m_cfg.m_wanted_flags & FilterFlags::NO_TCP_UDP ) {
        str += "no_tcp_udp";
    }
    
    if ( m_cfg.m_wanted_flags & FilterFlags::TCP_UDP ) {
        if ( str.size() ) {
            str += " and ";
        }
        str += "tcp_udp";
    }
}