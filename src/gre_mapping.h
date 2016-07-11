#ifndef GRE_MAPPING_H_
#define GRE_MAPPING_H_

typedef struct gre_mapping_ {
    uint8_t gw_mac[6];
    uint8_t client_mac[6];
    uint32_t src_ip;
    uint32_t key;
} gre_mapping_t;

class CFlowGenListGre {
public:
    CFlowGenListGre() {
        set_configured(false);
        last_index = 0;
    }

    std::vector<gre_mapping_t> &get_gre_info () {
        return m_gre_info;
    }

    bool is_configured() {
        return is_gre_info_configured;
    }

    void set_configured(bool is_conf) {
        is_gre_info_configured = is_conf;
    }

    void clear() {
        set_configured(false);
        m_gre_info.clear();
    }

    gre_mapping_t get_gre_info(uint32_t idx) {
      return m_gre_info[idx];
    }

    uint32_t get_gre_next_index() {
      if (last_index >= m_gre_info.size())
        last_index = 0;
      return last_index++;
    }

    void set_gre_dst_ip(uint32_t ip) {
        dst_ip = ip;
    }

    uint32_t get_gre_dst_ip() {
        return dst_ip;
    }

private:
    uint32_t last_index;
    bool is_gre_info_configured;
    uint32_t dst_ip;
    std::vector<gre_mapping_t> m_gre_info;  /* global gre info loaded form gre_file*/
};

#endif //GRE_MAPPING_H_
