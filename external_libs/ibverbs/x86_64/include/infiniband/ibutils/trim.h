/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

inline static std::string &
    ltrim(std::string & s, const std::string & chars = "\t\n\v\f\r ") {
        return s.erase(0, s.find_first_not_of(chars));
    }

inline static std::string &
    rtrim(std::string & s, const std::string & chars = "\t\n\v\f\r ") {
        return s.erase(s.find_last_not_of(chars) + 1);
    }

inline static std::string &
    trim(std::string & s, const std::string & chars = "\t\n\v\f\r ") {
        return ltrim(rtrim(s, chars), chars);
    }

inline static std::string
    ltrim(const std::string & s, const std::string & chars = "\t\n\v\f\r ") {
	size_t first = s.find_first_not_of(chars);
        return first == std::string::npos ? "" : std::string(s, first, 0);
    }

inline static std::string
    rtrim(const std::string & s, const std::string & chars = "\t\n\v\f\r "){
        return std::string(s, 0, s.find_last_not_of(chars) + 1);
    }

inline static std::string
    trim(const std::string & s, const std::string & chars = "\t\n\v\f\r ") {
	size_t first = s.find_first_not_of(chars);
        return first == std::string::npos ? "" :
		std::string(s, first, s.find_last_not_of(chars) - first + 1);
}
