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

#include <sys/ptrace.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include  <stdexcept>


static TrexWatchDog::monitor_st *global_monitor;

const char *get_exe_name();

std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL) {
            result += buffer;
        }
    }
    return result;
}

// This function produces a stack backtrace with demangled function & method names.
__attribute__((noinline))
std::string Backtrace(int skip = 1)
{
    void *callstack[128];
    const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
    char buf[1024];
    int nFrames = backtrace(callstack, nMaxFrames);
    char **symbols = backtrace_symbols(callstack, nFrames);

    std::ostringstream trace_buf;
    for (int i = skip; i < nFrames; i++) {

        Dl_info info;
        if (dladdr(callstack[i], &info) && info.dli_sname) {
            char *demangled = NULL;
            int status = -1;
            if (info.dli_sname[0] == '_')
                demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
            snprintf(buf, sizeof(buf), "%-3d %*p %s + %zd\n",
                     i, int(2 + sizeof(void*) * 2), callstack[i],
                     status == 0 ? demangled :
                     info.dli_sname == 0 ? symbols[i] : info.dli_sname,
                     (char *)callstack[i] - (char *)info.dli_saddr);
            free(demangled);
        } else {
            snprintf(buf, sizeof(buf), "%-3d %*p %s\n",
                     i, int(2 + sizeof(void*) * 2), callstack[i], symbols[i]);
        }
        trace_buf << buf;
    }
    free(symbols);
    if (nFrames == nMaxFrames)
        trace_buf << "[truncated]\n";

    /* add the addr2line info */
    std::stringstream addr2line;

    addr2line << "/usr/bin/addr2line -e " << get_exe_name() << " ";
    for (int i = skip; i < nFrames; i++) {
        addr2line << callstack[i] << " ";
    }

    trace_buf << "\n\n*** addr2line information follows ***\n\n";
    try {
        trace_buf << exec(addr2line.str().c_str());
    } catch (std::runtime_error &e) {
        trace_buf << "\n" << e.what();
    }

    return trace_buf.str();
}

__attribute__((noinline))
static void _callstack_signal_handler(int signr, siginfo_t *info, void *secret) {
    std::stringstream ss;

    double now = now_sec();

    ss << "WATCHDOG: task '" << global_monitor->name << "' has not responded for more than " << (now - global_monitor->ts) << " seconds - timeout is " << global_monitor->timeout_sec << " seconds";

    std::string backtrace = Backtrace();
    ss << "\n\n*** traceback follows ***\n\n" << backtrace << "\n";

    throw std::runtime_error(ss.str());
}


void TrexWatchDog::init(bool enable){
    m_enable =enable;
    if (m_enable) {
        register_signal();
    } 
}


void TrexWatchDog::mark_pending_monitor(int count) {
    if (!m_enable){
        return;
    }

    std::unique_lock<std::mutex> lock(m_lock);
    m_pending += count;
    lock.unlock();
}

void TrexWatchDog::block_on_pending(int max_block_time_ms) {

    if (!m_enable){
        return;
    }

    int timeout_msec = max_block_time_ms;

    std::unique_lock<std::mutex> lock(m_lock);

    while (m_pending > 0) {

        lock.unlock();
        delay(1);
        lock.lock();

        timeout_msec -= 1;
        if (timeout_msec == 0) {
            throw TrexException("WATCHDOG: block on pending monitors timed out");
        }
    }

    /* lock will be released */
}

/**
 * register a monitor 
 * must be called from the relevant thread 
 *  
 * this function is thread safe 
 * 
 * @author imarom (01-Jun-16)
 * 
 * @param name 
 * @param timeout_sec 
 * 
 * @return int 
 */
int TrexWatchDog::register_monitor(const std::string &name, double timeout_sec) {
    if (!m_enable){
        return 0;
    }
    monitor_st monitor;


    /* cannot add monitors while active */
    assert(m_active == false);

    monitor.active       = true;   
    monitor.tid          = pthread_self();
    monitor.name         = name;
    monitor.timeout_sec  = timeout_sec;
    monitor.tickled      = true;
    monitor.ts           = 0;

    /* critical section start */
    std::unique_lock<std::mutex> lock(m_lock);

    /* make sure no double register */
    for (auto &m : m_monitors) {
        if (m.tid == pthread_self()) {
            std::stringstream ss;
            ss << "WATCHDOG: double register detected\n\n" << Backtrace();
            throw TrexException(ss.str());
        }
    }

    monitor.handle = m_monitors.size();
    m_monitors.push_back(monitor);

    assert(m_pending > 0);
    m_pending--;

    /* critical section end */
    lock.unlock();

    return monitor.handle;
}

/**
 * will disable the monitor - it will no longer be watched
 * 
 */
void TrexWatchDog::disable_monitor(int handle) {
    if (!m_enable){
        return ;
    }

    assert(handle < m_monitors.size());

    m_monitors[handle].active = false;
}

/**
 * thread safe function
 * 
 */
void TrexWatchDog::tickle(int handle) {
    if (!m_enable){
        return ;
    }
    assert(handle < m_monitors.size());

    /* not nesscary but write gets cache invalidate for nothing */
    if (m_monitors[handle].tickled) {
        return;
    }

    m_monitors[handle].tickled = true;
}

void TrexWatchDog::register_signal() {
    /* do this once */
    if (g_signal_init) {
        return;
    }

    /* register a handler on SIG ALARM */
    struct sigaction sa;
    memset (&sa, '\0', sizeof(sa));

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = _callstack_signal_handler;

    int rc = sigaction(SIGALRM , &sa, NULL);
    assert(rc == 0);

    g_signal_init = true;
}

void TrexWatchDog::start() {

    if (!m_enable){
        return ;
    }

    block_on_pending();

    /* no pending monitors */
    assert(m_pending == 0);

    m_active = true;
    m_thread = new std::thread(&TrexWatchDog::_main, this);
    if (!m_thread) {
        throw TrexException("unable to create watchdog thread");
    }
}

void TrexWatchDog::stop() {
    if (!m_enable){
        return ;
    }

    m_active = false;

    if (m_thread) {
        m_thread->join();
        delete m_thread;
        m_thread = NULL;
    }
}



/**
 * main loop
 * 
 */
void TrexWatchDog::_main() {

    assert(m_enable==true);

    /* reset all the monitors */
    for (auto &monitor : m_monitors) {
        monitor.tickled = true;
    }

    /* start main loop */
    while (m_active) {

        dsec_t now = now_sec();

        for (auto &monitor : m_monitors) {

            /* skip non active monitors */
            if (!monitor.active) {
                continue;
            }

            /* if its own - turn it off and write down the time */
            if (monitor.tickled) {
                monitor.tickled = false;
                monitor.ts      = now;
                continue;
            }

            /* the bit is off - check the time first */
            if ( (now - monitor.ts) > monitor.timeout_sec ) {
                global_monitor = &monitor;

                pthread_kill(monitor.tid, SIGALRM);

                /* nothing to do more... the other thread will terminate, but if not - we terminate */
                sleep(5);
                printf("\n\n*** WATCHDOG violation detected on task '%s' which have failed to response to the signal ***\n\n", monitor.name.c_str());
                exit(1);
            }

        }

        /* the internal clock - 250 ms */
        delay(250);
    }
}

bool TrexWatchDog::g_signal_init = false;
