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

#include "trex_watchdog.h"
#include "trex_exception.h"

#include <assert.h>
#include <unistd.h>
#include <sstream>

int WatchDog::register_monitor(const std::string &name, double timeout_sec) {
    monitor_st monitor;

    monitor.name         = name;
    monitor.timeout_sec  = timeout_sec;
    monitor.tickled      = true;
    monitor.ts           = 0;

    monitor.handle = m_monitors.size();
    m_monitors.push_back(monitor);

    return monitor.handle;
}

void WatchDog::tickle(int handle) {
    assert(handle < m_monitors.size());

    /* not nesscary but write gets cache invalidate for nothing */
    if (m_monitors[handle].tickled) {
        return;
    }

    m_monitors[handle].tickled = true;
}

void WatchDog::start() {
    m_active = true;
    m_thread = new std::thread(&WatchDog::_main, this);
    if (!m_thread) {
        throw TrexException("unable to create watchdog thread");
    }
}

void WatchDog::stop() {
    m_thread->join();
    delete m_thread;
}

/**
 * main loop
 * 
 */
void WatchDog::_main() {
    while (m_active) {

        dsec_t now = now_sec();

        for (auto &monitor : m_monitors) {
            /* if its own - turn it off and write down the time */
            if (monitor.tickled) {
                monitor.tickled = false;
                monitor.ts      = now;
                continue;
            }

            /* the bit is off - check the time first */
            if ( (now - monitor.ts) > monitor.timeout_sec ) {
                std::stringstream ss;
                ss << "WATCHDOG: task '" << monitor.name << "' has not responded for more than " << (now - monitor.ts) << " seconds - timeout is " << monitor.timeout_sec << " seconds";
                throw TrexException(ss.str());
                assert(0);
            }

        }

        sleep(1);
    }
}

