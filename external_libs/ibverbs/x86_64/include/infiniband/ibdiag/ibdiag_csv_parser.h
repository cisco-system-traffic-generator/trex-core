/*
 * Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#pragma once

#include <type_traits>
#include <limits>

//
// Parser Templates
//
template<typename T, typename V>
    bool Validate(const char *text, const char *end, T &field, V value)
    {
        if (value > std::numeric_limits<T>::max() || value < std::numeric_limits<T>::min())
            return false;

        while (*end && std::isspace(*end))
            ++end;

        if (*end)
            return false;

        field = static_cast<T>(value);

        return true;
    }

template<typename T, typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value, bool>::type = true>
    bool ParseType(const char *text, T &field, uint8_t base)
    {
        char *  end     = nullptr;
        int64_t value   = std::strtoll(text, &end, base);

        return Validate(text, end, field, value);
    }

template<typename T, typename std::enable_if<std::is_unsigned<T>::value, bool>::type = true>
    bool ParseType(const char *text, T &field, uint8_t base)
    {
        char *      end     = nullptr;
        uint64_t    value   = std::strtoull(text, &end, base);

        return Validate(text, end, field, value);
    }


template<typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
    bool ParseType(const char *text, T &field, uint8_t reserved = 0)
    {
        field = text;
        return true;
    }

template<typename T, typename std::enable_if<std::is_floating_point<T>::value, bool>::type = true>
    bool ParseType(const char *text, T &field, uint8_t reserved = 0)
    {
        char *      end     = nullptr;
        long double value   = std::strtold(text, &end);

        return Validate(text, end, field, value);
    }

static bool isNA(const char *text)
{
    const char * p = text;

    if (p == nullptr)
        return false;

    while(*p && isspace(*p))
        ++p;

    if (*p != 'N' && *p != 'n')
        return false;

    if (*++p == '/')
        ++p;

    if (*p != 'A' && *p != 'a')
        return false;

    ++p;

    while (*p && isspace(*p))
        ++p;

    return !*p;
}

template<typename T, typename V = T>
    bool Parse(const char *text, T &field, bool * NA = nullptr, V def = V(), uint8_t base = 0)
    {
        const char * p  = text;
        field           = static_cast<T>(def);

        if (p == nullptr)
            return false;

        while(*p && isspace(*p))
            ++p;

        if (NA)
            if (isNA(p))
                return *NA = true;

        ParseType(p, field, base);
        return true;
    }


