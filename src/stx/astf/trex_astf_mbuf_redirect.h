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

#include <stdint.h>
#include "msg_manager.h"
#include "mbuf.h"


// forward declarations
class CAstfTimerFunctorObj;
class CTcpPerThreadCtx;
class MbufRedirectCache;

/**
 * MbufCachePerDp holds a cache of a specific DP to a specific DP.
 * Each DP can redirect packets to each DP. The mbuf ring to the destination
 * DP is received as a parameter. The ring is shared, multiple producers and
 * single consumers.
 */
class MbufCachePerDp {

public:
    /** Ctor
     * @param mbuf_redirect_cache
     *    Redirect cache object of this core. Holds multiple MbufCachePerDp, per destination DP.
     *    Pointer to the object that holds this.
     * @param ring_to_dp
     *   Ring to data plane that we need to redirect packets to.
     */
    MbufCachePerDp(MbufRedirectCache* mbuf_redirect_cache, CMbufRing* ring_to_dp);

    ~MbufCachePerDp();

    /**
     * Add a new mbuf to the cache. If the cache is full post adding, we flush it.
     *
     * @param mbuf
     *   mbuf to add to cache
     * @param flush_now
     *   Should flush the cache immediately.
     *   - True: Flush the cache immediately to the ring.
     *   - False: Do not flush the cache immediately to the ring.
     */
    void add(rte_mbuf_t* mbuf, bool flush_now);

    /**
     * Flush the cache to the ring of the relevant DP.
     */
    void flush();

private:

    MbufRedirectCache*          m_redirect_cache;           // General redirect cache for all DP.
    CMbufRing*                  m_ring_to_dp;               // Ring to DP for which we are caching.
    int                         m_cache_size;               // Cache size at the moment.
    static const uint16_t       MBUF_CACHE_SIZE = 64;       // Mbuf cache table size per DP.
    rte_mbuf_t*                 m_cache[MBUF_CACHE_SIZE];   // Cache object, an array of mbuf pointers of fixed size.
};


/**
 * MbufRedirectCache is the object relevant to redirect packets to the relevant DPs
 * in ASTF software RSS mode. Each AstfDpCore holds a MbufRedirectCache. The thread_id
 * of the aforementioned AstfDpCore is received as a paramater.
 * This object is created only in the relevant TRex mode.
 */
class MbufRedirectCache {

    friend class MbufCachePerDp; // So that the MbufCachePerDp can update the counters.

public:
    /** Ctor
     * @param ctx
     *   Client Context.
     * @param num_dp_cores
     *   General number of data plane cores. This is not per dual, but in total.
     * @param thread_id
     *   Thread id number of the AstfDpCore that holds this object.
     */
    MbufRedirectCache(CTcpPerThreadCtx* ctx, uint8_t num_dp_cores, uint8_t thread_id);

    ~MbufRedirectCache();

    /**
     * on_timer_update is called periodically each FLUSH_PERIOD_MSEC msec. Upon each call, it flushes
     * the cache to other cores. This must remain public in order to be binded as a callback.
     *
     * @param timer
     *   Timer object.
     */
    void on_timer_update(CAstfTimerFunctorObj* timer);

    /**
     * redirect_packet receives a packet that needs to be redirected to some other data plane core.
     * This packet needs to be redirected to the core that handles the flow in software, meaning it needs
     * to be RSSed in software, due the lack of RSS in NIC hardware.
     * The function stores the packet in a cache for the destination DP.
     * In case the cache is full, the function flushes the cache to the destination DP's mbuf ring.
     * The cache is also flushed periodically.
     *
     * @param m
     *   Packet mbuf
     * @param dest_thread_id
     *   Thread ID of the DP core who holds the flow information for the packet.
     */
    void redirect_packet(rte_mbuf_t* m, uint8_t dest_thread_id);

    /**
     * retrieve_redirected_packets reads the smaller between the number of redirected packets and *nb_pkts*
     * into the *rx_pkts* mbuf array.
     *
     * @param rx_pkts
     *   The address of an array of pointers to *rte_mbuf* structures that
     *   must be large enough to store *nb_pkts* pointers in it.
     * @param nb_pkts
     *   The maximum number of packets to retrieve.
     * @return
     *   Number of packets retrieved.
     */
    uint16_t retrieve_redirected_packets(rte_mbuf_t** rx_pkts, uint16_t nb_pkts);

    /**
     * flush_and_stop_redirect flushes the mbuf redirect cache and then stops the timer.
     * This way the timer doesn't interfere with the TRex watchdog.
     */
    void flush_and_stop_redirect();

    /**
     * start_redirect_timer starts the timer which is called each FLUSH_PERIOD_MSEC to
     * redirect the mbufs to the rings of the respective DPs.
     * This timer is not started automatically upon creation, it is started only
     * when traffic starts transmitting.
     */
     void start_redirect_timer();

private:

    /**
     * Redirect the mbufs we have been caching.
     */
    void flush_mbuf_redirect_cache();

    MbufCachePerDp**            m_cache;                    // Cache table of mbuf to redirect to other cores in case of software rss.
    CMbufRing*                  m_ring_from_dps;            // Packets redirected to this core from other DP cores.
    CTcpPerThreadCtx*           m_ctx;                      // TCP per thread context.
    CAstfTimerFunctorObj*       m_timer;                    // Timer object to flush.
    uint32_t                    m_ticks;                    // Ticks between two timer calls.
    const uint16_t              FLUSH_PERIOD_MSEC = 1;      // Period between two successive flushes.
    uint8_t                     m_num_dp_cores;             // Number of DP cores.
    uint8_t                     m_thread_id;                // Thread ID of the DP core that created this object.
    bool                        m_disable_cache;            // Disables caching. Packets will be automatically enqueued.

};