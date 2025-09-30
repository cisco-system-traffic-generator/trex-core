/*
 * Copyright (C) 2022-2022, NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
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

#ifndef SYS_LOGGER_H
#define SYS_LOGGER_H

#include <stdarg.h>
#include <syslog.h>

class SysLogger
{
private:
    bool is_enabled;

public:
    static SysLogger &instance()
    {
        static SysLogger _instance;
        return _instance;
    }

    bool get_enabled()
    {
        return is_enabled;
    }

    void set_enabled(bool enabled)
    {
        is_enabled = enabled;
    }

    void syslog(const char *fmt, ...)
    {
        if (!is_enabled)
            return;

        static bool open_log_done = false;

        if (!open_log_done)
            openlog(NULL, LOG_PID, LOG_USER);

        va_list args;

        va_start(args, fmt);
        vsyslog(LOG_NOTICE, fmt, args);
        va_end(args);
    }

private:
    SysLogger() : is_enabled(false) {}
};

#endif /* SYS_LOGGER_H */
