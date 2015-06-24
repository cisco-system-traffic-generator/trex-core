#ifndef _PACKET_HEADER_BASE_H_
#define _PACKET_HEADER_BASE_H_
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


#include "CPktCmn.h"

/**
 * This class should be the base class for all packet headers in the system.
 * Its target is to obligate all the headers to implement some common interface.
 * e.g. Providing the pointer for the header, its size, dumping itself etc.
 * Since that all header are being casted over some memory because
 * of performance orientation, we are not using the pure virtual feature
 * of C++. Thus this obligation is enforced at link time since there will be
 * no implmentation in the base and if the interface will be used in some derived
 * that didn't implement it, it will fail at link time.
 */
class PacketHeaderBase
{
public:
    uint8_t*  getPointer          (){return (uint8_t*)this;};
    uint32_t  getSize             ();

    uint16_t  getNextProtocol     ();
    void    setNextProtocol     (uint16_t);

    void    dump                (FILE*  fd);
};

enum
{
    TCPDefaultHeaderSize = 20,//TCP 
    UDPDefaultHeaderSize = 8  //UDP
};

#endif //_PACKET_HEADER_BASE_H_
