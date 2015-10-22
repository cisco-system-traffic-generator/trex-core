#ifndef MAC_MAPPING_H_
#define MAC_MAPPING_H_

#define INUSED 0
#define UNUSED 1
typedef struct mac_addr_align_ {
public:
    uint8_t mac[6];
    uint8_t inused;
    uint8_t pad;
} mac_addr_align_t;

typedef struct mac_mapping_ {
    mac_addr_align_t mac;
    uint32_t         ip;
} mac_mapping_t;

class CFlowGenListMac {
public:
    CFlowGenListMac() {
        set_configured(false);
    }

    std::map<uint32_t, mac_addr_align_t> &
    get_mac_info () {
        return m_mac_info;
    }

    bool is_configured() {
        return is_mac_info_configured;
    }

    void set_configured(bool is_conf) {
        is_mac_info_configured = is_conf;
    }

    void clear() {
        set_configured(false);
        m_mac_info.clear();
    }

    uint32_t is_mac_exist(uint32_t ip) {
        if (is_configured()) {
            return m_mac_info.count(ip);
        } else {
            return 0;
        }
    }
    mac_addr_align_t* get_mac_addr_by_ip(uint32_t ip) {
        if (is_mac_exist(ip)!=0) {
            return &(m_mac_info[ip]);
        }
        return NULL;
    }
private:
    bool                                 is_mac_info_configured;
    std::map<uint32_t, mac_addr_align_t> m_mac_info;  /* global mac info loaded form mac_file*/
};

#endif //MAC_MAPPING_H_
