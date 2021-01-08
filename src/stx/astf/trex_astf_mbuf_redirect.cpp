/*
 Bes Dollma
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2020 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "bp_sim.h" // trex_astf_mbuf_redirect.h is included through this.
#include "stt_cp.h"

using namespace std;

MbufCachePerDp::MbufCachePerDp(MbufRedirectCache* redirect_cache, CMbufRing* ring_to_dp) {
    m_cache_size = 0;
    m_ring_to_dp = ring_to_dp;
    m_redirect_cache = redirect_cache;
}

MbufCachePerDp::~MbufCachePerDp() {

}

void MbufCachePerDp::add(rte_mbuf_t* mbuf, bool flush_now) {
    m_cache[m_cache_size++] = mbuf;
    if (flush_now || m_cache_size == MBUF_CACHE_SIZE) {
        flush();
    }
}

void MbufCachePerDp::flush() {
    CFlowTable& ft = m_redirect_cache->m_ctx->m_ft;
    if (m_ring_to_dp->EnqueueBulk(m_cache, m_cache_size)) {
        ft.inc_rss_redirect_tx_cnt(m_cache_size);
    } else {
        ft.inc_rss_redirect_drop_cnt(m_cache_size);
        for (int i = 0; i < m_cache_size; i++) {
            rte_pktmbuf_free(m_cache[i]);
        }
    }
    // cache is clean.
    m_cache_size = 0;
}


MbufRedirectCache::MbufRedirectCache(CTcpPerThreadCtx* ctx,
                                     uint8_t num_dp_cores,
                                     uint8_t thread_id) {
    m_ctx = ctx;
    m_num_dp_cores = num_dp_cores;
    m_thread_id = thread_id;
    m_disable_cache = false;
    CDpToDpMessagingManager* dp_dp = CMsgIns::Ins()->getDpDp();
    m_ring_from_dps = dp_dp->getRingToDp(m_thread_id);
    m_cache = new MbufCachePerDp*[m_num_dp_cores];
    for (int i = 0; i < m_num_dp_cores; i++) {
        m_cache[i] = new MbufCachePerDp(this, dp_dp->getRingToDp(i));
    }
    m_timer = new CAstfTimerFunctorObj();
    assert(m_timer);
    m_timer->m_cb = std::bind(&MbufRedirectCache::on_timer_update, this, m_timer);
    m_ticks = tw_time_msec_to_ticks(FLUSH_PERIOD_MSEC);
}

MbufRedirectCache::~MbufRedirectCache() {
    for (int i = 0; i < m_num_dp_cores; i++) {
        delete m_cache[i];
    }
    delete m_cache;

    if (m_timer) {
        if (m_timer->is_running()) {
            m_ctx->m_timer_w.timer_stop(m_timer);
        }
        delete m_timer;
    }
}

void MbufRedirectCache::on_timer_update(CAstfTimerFunctorObj* timer) {
    flush_mbuf_redirect_cache();
    // start timer again.
    m_ctx->m_timer_w.timer_start(timer, m_ticks);
}

void MbufRedirectCache::redirect_packet(rte_mbuf_t* m, uint8_t dest_thread_id) {
    // validate params
    assert(dest_thread_id < m_num_dp_cores);
    assert(dest_thread_id != m_thread_id);
    assert(m);
    m_cache[dest_thread_id]->add(m, m_disable_cache);
}

uint16_t MbufRedirectCache::retrieve_redirected_packets(rte_mbuf_t** rx_pkts, uint16_t nb_pkts) {
    uint32_t n = std::min(static_cast<uint32_t>(nb_pkts), m_ring_from_dps->Size());
    if (m_ring_from_dps->DequeueBulk(rx_pkts, n)) {
        m_ctx->m_ft.inc_rss_redirect_rx_cnt(n);
        return n;
    }
    return 0;
}

void MbufRedirectCache::flush_and_stop_redirect() {
    /* Stop the timer and then redirect the packets.
       We redirect on purpose only when the timer is running so this function
       is safe to call when the timer is stopped. */
    if (m_timer && m_timer->is_running()) {
        m_disable_cache = true;
        m_ctx->m_timer_w.timer_stop(m_timer);
        flush_mbuf_redirect_cache();
    }
}

void MbufRedirectCache::start_redirect_timer() {
    if (m_timer && m_timer->is_running()) {
        // timer is already running
        return;
    }
    m_disable_cache = false;
    m_ctx->m_timer_w.timer_start(m_timer, m_ticks);
}

void MbufRedirectCache::flush_mbuf_redirect_cache() {
    for (int i = 0; i < m_num_dp_cores; i++) {
        m_cache[i]->flush();
    }
}
