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


void abort_gracefully(const std::string &on_stdout,
                      const std::string &on_publisher) __attribute__ ((__noreturn__));


static TrexMonitor *global_monitor;

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

    addr2line << "/usr/bin/addr2line -s -e " << get_exe_name() << " ";
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

    ss << "WATCHDOG: task '" << global_monitor->get_name() << "' has not responded for more than " << global_monitor->get_interval(now) << " seconds - timeout is " << global_monitor->get_timeout_sec() << " seconds";

    std::string backtrace = Backtrace();
    ss << "\n\n*** traceback follows ***\n\n" << backtrace << "\n";

    /* best effort abort - might get stuck in rare cases but does not matter - the watchdog thread will abort eventually */
    abort_gracefully(ss.str(), ss.str());
}


/**
 * hook assert failure function
 */
extern "C" {

    /* thread local storage - to avoid multiple threads calling assert */
    static __thread bool g_in_assert = false;
    
    extern void __assert_fail (const char *__assertion, const char *__file, unsigned int __line, const char *__function) throw () {
        
        /* double assert - make sure no re-entrant calls, simply call abort directly */
        if (g_in_assert) {
            abort();
        }
        
        /* mark it */
        g_in_assert = true;
        
        std::string cause = "assert: " + std::string(__file) + ":" + std::to_string(__line) + " " + std::string(__function) + " Assertion '" + std::string(__assertion) + "' failed.";
        
        std::stringstream ss;
        ss << cause;
        ss << "\n\n*** traceback follows ***\n\n" << Backtrace() << "\n";
        
        
        /* a bit shorter version */
        std::string publish_cause = "assert: " + std::string(__file) + ":" + std::to_string(__line) +  " Assertion '" + std::string(__assertion) + "' failed.";
        
        /* abort with messages */
        abort_gracefully(ss.str(), publish_cause);
    }

}


/**************************************
 * Trex Monitor object
 *************************************/

void TrexMonitor::create(const std::string &name, double timeout_sec) {
    m_tid              = pthread_self();
    m_name             = name;
    m_timeout_sec      = timeout_sec;
    m_base_timeout_sec = timeout_sec;
    m_tickled          = true;
    m_ts               = 0;
    m_io_ref_cnt       = 0;
    
    /* the rare case of m_active_time_sec set out of order with tickled */
    asm volatile("mfence" ::: "memory");
}

/**************************************
 * Trex watchdog
 *************************************/

void TrexWatchDog::init(bool enable){
    m_enable = enable;
    if (m_enable) {
        register_signal();
    } 
}

/**
 * get pointer to monitor of current thread
 * (NULL if no monitor)
 * 
 */
TrexMonitor * TrexWatchDog::get_current_monitor() {

    for (int i = 0; i < m_mon_count; i++) {
        if ( m_monitors[i]->get_tid() == pthread_self() ) {
            return m_monitors[i];
        }
    }

    return NULL;
}


/**
 * register a monitor 
 * this function is thread safe 
 * 
 */
void TrexWatchDog::register_monitor(TrexMonitor *monitor) {
    if (!m_enable){
        return;
    }

    /* critical section start */
    std::unique_lock<std::mutex> lock(m_lock);

    /* sanity - not a must but why not... */
    TrexMonitor * cur_monitor = get_current_monitor();
    if ( cur_monitor != NULL || cur_monitor == monitor ) {
        std::stringstream ss;
        ss << "WATCHDOG: double register detected\n\n" << Backtrace();
        throw TrexException(ss.str());
    }

    /* check capacity */
    if (m_mon_count == MAX_MONITORS) {
        std::stringstream ss;
        ss << "WATCHDOG: too many registered monitors\n\n" << Backtrace();
        throw TrexException(ss.str());
    }

    /* add monitor */
    m_monitors[m_mon_count++] = monitor;

    /* critical section end */
    lock.unlock();

}

void TrexWatchDog::start() {

    if (!m_enable){
        return ;
    }

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
void TrexWatchDog::_main() noexcept {

    pthread_setname_np(pthread_self(), "Trex Watchdog");

    assert(m_enable == true);

    /* start main loop */
    while (m_active) {

        dsec_t now = now_sec();

        /* to be on the safe side - read the count with a lock */
        std::unique_lock<std::mutex> lock(m_lock);
        int count = m_mon_count;
        lock.unlock();

        for (int i = 0; i < count; i++) {
            TrexMonitor *monitor = m_monitors[i];

            /* skip non expired monitors */
            if (!monitor->is_expired(now)) {
                continue;
            }
            
            /* it has expired but it was tickled */
            if (monitor->is_tickled()) {
                monitor->reset(now);
                continue;
            }

            /* crash */
            global_monitor = monitor;

            pthread_kill(monitor->get_tid(), SIGALRM);

            /* nothing to do more... the other thread will terminate, but if not - we terminate */
            sleep(5);
            fprintf(stderr, "\n\n*** WATCHDOG violation detected on task '%s' which have failed to response to the signal ***\n\n", monitor->get_name().c_str());
            abort();
        }

        /* the internal clock - 250 ms */
        delay(250);
    }
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

bool TrexWatchDog::g_signal_init = false;

