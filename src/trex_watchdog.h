/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

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

#ifndef __TREX_WATCHDOG_H__
#define __TREX_WATCHDOG_H__

#include <string>
#include <vector>
#include <thread>
#include <mutex>

//#include "rte_memory.h"
#include "mbuf.h"
#include "os_time.h"

class TrexWatchDog {
public:
    TrexWatchDog() {
        m_thread  = NULL;
        m_active  = false;
        m_pending = 0;

        register_signal();
    }

    /**
     * registering a monitor happens from another thread 
     * this make sure that start will be able to block until 
     * all threads has registered 
     * 
     * @author imarom (01-Jun-16)
     */
    void mark_pending_monitor(int count = 1);


    /**
     * blocks while monitors are pending registeration
     * 
     * @author imarom (01-Jun-16)
     */
    void block_on_pending(int max_block_time_ms = 200);


    /**
     * add a monitor to the watchdog 
     * this thread will be monitored and if timeout 
     * has passed without calling tick - an exception will be called
     * 
     * @author imarom (31-May-16)
     * 
     * @param name 
     * @param timeout 
     * 
     * @return int 
     */
    int register_monitor(const std::string &name, double timeout_sec);


    /**
     * should be called by each thread on it's handle
     * 
     * @author imarom (31-May-16)
     * 
     * @param handle 
     */
    void tickle(int handle);


    /**
     * start the watchdog
     * 
     */
    void start();


    /**
     * stop the watchdog
     * 
     */
    void stop();


    /* should be cache aligned to avoid false sharing */
    struct monitor_st {
        /* write fields are first */
        volatile bool   tickled;
        dsec_t          ts;

        int             handle;
        double          timeout_sec;
        pthread_t       tid;
        std::string     name;

        /* for for a full cacheline */
        uint8_t       pad[16];
    };


private:
    void register_signal();
    void _main();

    std::vector<monitor_st> m_monitors __rte_cache_aligned;
    std::mutex              m_lock;

    volatile bool           m_active;
    std::thread            *m_thread;
    volatile int            m_pending;

    static bool             g_signal_init;
};

static_assert(sizeof(TrexWatchDog::monitor_st) >= RTE_CACHE_LINE_SIZE, "sizeof(monitor_st) != RTE_CACHE_LINE_SIZE" );

#endif /* __TREX_WATCHDOG_H__ */
