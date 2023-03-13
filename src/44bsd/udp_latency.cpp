#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

//#include <common/basic_utils.h>
//#include "utl_mbuf.h"

#include <common/Network/Packet/UdpHeader.h>
#include "tcp_var.h"
#include "tcp_socket.h"
#include "dyn_sts.h"


struct latency_header {         // RTP header
    union {
        struct {
            uint16_t version:2;
            uint16_t padding:1;
            uint16_t extension:1;
            uint16_t csrc_count:4;
            uint16_t marker:1;
            uint16_t payload_type:7;
        };
        uint16_t magic;         // latency magic number
    };

    uint16_t seq_num;
    uint32_t timestamp;
    uint32_t ssrc;              // connection id
} __attribute__((__packed__));

#define LATENCY_MAGIC   0x0000


typedef enum { LATENCY_NONE, LATENCY_CLIENT, LATENCY_SERVER } latency_role_t;

struct latency_stats_data {
    uint64_t min_time;
    uint64_t max_time;
    uint64_t measure_cnt;
    uint64_t total_time;
    uint64_t total_pkts;
};

static sts_desc_t latency_stats_desc_data = {
    CMetaDataPerCounter("min_time", "minimum latency time (usec)"),
    CMetaDataPerCounter("max_time", "maximum latency time (usec)"),
    CMetaDataPerCounter("measure_cnt", "number of accumulated min/max time"),
    CMetaDataPerCounter("total_time", "total sum of latency time (usec)"),
    CMetaDataPerCounter("total_pkts", "total received latency packets")
};


struct latency_priv_data {
    latency_role_t role;

    uint16_t magic;
    uint16_t seq_num;
    uint32_t peer_id;

    struct latency_stats_data* stats;
};


static inline struct latency_priv_data* get_priv_data(CEmulApp* app) {
    void* data = app->get_priv_data();
    if (data == nullptr) {
        data = new latency_priv_data();
        app->set_priv_data(data);
    }
    return (struct latency_priv_data*)data;
}


static inline void clear_priv_data(CEmulApp* app) {
    struct latency_priv_data* data = (struct latency_priv_data*)app->get_priv_data();
    if (data) {
        delete data;
        app->set_priv_data(nullptr);
    }
}


static inline struct latency_stats_data* get_stats_data(CEmulApp* app) {
    return (latency_stats_data*)app->get_addon_sts();
}


static inline uint8_t* latencyhdr_in_template(CEmulApp* app) {
    CUdpFlow* flow = app->get_udp_flow();
    return flow->m_template.reserve_addon(sizeof(struct latency_header));
}


static uint32_t latency_time_usec() {
    uint64_t cycles = os_get_hr_tick_64();
    uint64_t sec = os_get_hr_tick_64()/os_get_hr_freq();
    cycles = cycles - sec*os_get_hr_freq();
#define USEC_PER_SEC    1000000L
    return sec*USEC_PER_SEC + (cycles*USEC_PER_SEC)/os_get_hr_freq();
}


static latency_role_t latency_role(struct latency_priv_data* priv, latency_role_t default_role) {
    if (priv->role == LATENCY_NONE) {
        priv->role = default_role;
    }
    return priv->role;
}


static void update_latency_header(struct latency_priv_data *priv, struct latency_header* latencyhdr) {
    latencyhdr->magic = htons(priv->magic);
    latencyhdr->seq_num = htons(priv->seq_num);
    latencyhdr->timestamp = htonl(latency_time_usec());
    latencyhdr->ssrc = htonl(priv->peer_id);

    priv->seq_num += 1;
}


static void update_latency_stats(struct latency_priv_data *priv, struct latency_header* latencyhdr) {
    uint32_t curr_timestamp = latency_time_usec();
    uint32_t time_diff = curr_timestamp - ntohl(latencyhdr->timestamp);

    struct latency_stats_data& stats = *priv->stats;

    stats.total_pkts += 1;
    stats.total_time += time_diff;

    if (time_diff > stats.max_time) {
        stats.max_time = time_diff;
    }
    if ((time_diff && stats.min_time == 0) || (time_diff < stats.min_time)) {
        stats.min_time = time_diff;
    }
    stats.measure_cnt = true;
}


#define SSRC_ID_BASE    0x12345678

uint32_t new_ssrc_id() {
    static uint32_t ssrc_id_gen = SSRC_ID_BASE;
    return ssrc_id_gen++;
}


class CEmulLatency: public CEmulAddon {

public:
    CEmulLatency() : CEmulAddon("latency", false, IPHeader::Protocol::UDP) {}

    uint64_t new_connection_id(uint32_t profile_id, uint32_t template_idx) override {
        return new_ssrc_id();
    }

    bool get_connection_id(uint8_t *data, uint16_t len, uint64_t& conn_id) override {
        struct latency_header *latencyhdr = (struct latency_header*)data;
        if (len < sizeof(struct latency_header) || latencyhdr->magic != ntohs(LATENCY_MAGIC)) {
            return false;
        }
        /* get connection id from data */
        conn_id = ntohl(latencyhdr->ssrc);
        return true;
    }

    void setup_connection(CEmulApp* app) override {
        struct latency_priv_data *priv = get_priv_data(app);

        priv->role = LATENCY_NONE;

        priv->magic = LATENCY_MAGIC;
        priv->seq_num = 0;
        priv->peer_id = app->get_peer_id();

        priv->stats = (struct latency_stats_data*)app->get_addon_sts();
    }

    void close_connection(CEmulApp* app) override {
        clear_priv_data(app);
    }

    void send_data(CEmulApp* app, CMbufBuffer* buf) override {
        struct latency_priv_data *priv = get_priv_data(app);
        latency_role_t role = latency_role(priv, LATENCY_CLIENT);

        if (role == LATENCY_CLIENT) {
            uint8_t *latencyhdr = latencyhdr_in_template(app);
            update_latency_header(priv, (struct latency_header*)latencyhdr);
        }

        CUdpFlow* flow = app->get_udp_flow();

        flow->send_pkt(buf);
        flow->flush_tx();
    }

    void recv_data(CEmulApp* app, struct rte_mbuf* m) override {
        struct latency_priv_data *priv = get_priv_data(app);
        latency_role_t role = latency_role(priv, LATENCY_SERVER);

        update_latency_stats(priv, rte_pktmbuf_mtod(m, struct latency_header*));

        if (role == LATENCY_SERVER) {
            uint8_t *latencyhdr = latencyhdr_in_template(app);
            memcpy(latencyhdr, rte_pktmbuf_mtod(m, void*), sizeof(struct latency_header));
        }

        app->on_bh_rx_pkts(0, m);
    }

    sts_desc_t* get_stats_desc() const override {
        return &latency_stats_desc_data;
    }
};


REGISTER_EMUL_ADDON(CEmulLatency);
