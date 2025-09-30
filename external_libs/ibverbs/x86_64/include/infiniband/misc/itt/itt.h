/*
 * Copyright (c) 2022-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef ITT_H
#define ITT_H

#undef ENABLE_ITT

#ifdef ENABLE_ITT
#define INTEL_ITTNOTIFY_API_PRIVATE
#include "ittnotify.h"

#include <string>
#include <thread>

class ITTTracer
{
public:
    static ITTTracer &instance() {
        static ITTTracer tracer;
        return tracer;
    }

    void enable(const char *base_path) {
        std::string trace_path = std::string(base_path) + "itt";

        setenv("INTEL_LIBITTNOTIFY64", "libIntelSEAPI64.so", 0);
        setenv("INTEL_SEA_SAVE_TO", trace_path.c_str(), 0);
    }

private:
    ITTTracer() {}
};

class ITTTask
{
private:
    __itt_domain *p_domain;
    __itt_id id;

public:
    ITTTask(__itt_domain* p_domain, __itt_string_handle* p_handle)
        : p_domain(p_domain), id(__itt_null)
    {
        if (!this->p_domain || !this->p_domain->flags)
            return;
        this->id = __itt_id_make(this->p_domain, reinterpret_cast<unsigned long long>(p_handle));
        __itt_task_begin(this->p_domain, this->id, __itt_null, p_handle);
    }

    ~ITTTask()
    {
        if (!this->p_domain || !this->p_domain->flags)
            return;
        __itt_task_end(this->p_domain);
    }
};

extern __itt_domain *itt_domain;

#define ITT_ADD_LINE(name) ITT_JOIN(name, __LINE__)
#define ITT_ENSURE_STATIC(static_var) while(!(static_var)) std::this_thread::yield()

#define ITT_TRACE_FUNCTION                                                                                    \
    static __itt_string_handle* ITT_ADD_LINE(__itt_handle) = __itt_string_handle_create(__PRETTY_FUNCTION__); \
    ITT_ENSURE_STATIC(ITT_ADD_LINE(__itt_handle));                                                            \
    ITTTask ITT_ADD_LINE(__itt_task)(itt_domain, ITT_ADD_LINE(__itt_handle))
#else
#define ITT_TRACE_FUNCTION
#endif

#endif /* ITT_H */
