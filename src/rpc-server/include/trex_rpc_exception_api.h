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

#ifndef __TREX_RPC_EXCEPTION_API_H__
#define __TREX_RPC_EXCEPTION_API_H__

#include <string>
#include <stdexcept>

/**
 * generic exception for RPC errors
 * 
 */
class TrexRpcException : public std::runtime_error 
{
public:
    TrexRpcException() : std::runtime_error("") {

    }
    TrexRpcException(const std::string &what) : std::runtime_error(what) {
    }
};

#endif /* __TREX_RPC_EXCEPTION_API_H__ */
