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

#include "os_time.h"

class WatchDog {
public:
    WatchDog() {
        m_thread = NULL;
        m_active = false;
    }

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

private:
    void _main();

    struct monitor_st {
        int           handle;
        std::string   name;
        double        timeout_sec;
        bool          tickled;
        dsec_t        ts;
    };

    std::vector<monitor_st> m_monitors;


    volatile bool m_active;
    std::thread  *m_thread;
};

#endif /* __TREX_WATCHDOG_H__ */
