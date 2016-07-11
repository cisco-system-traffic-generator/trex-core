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

#ifndef _GRE_HEADER_H_
#define _GRE_HEADER_H_

#include "PacketHeaderBase.h"

/**
 * This class encapsulates a GRE header.
 * It has fields that are equivalent to the GRE header fields.
 * The data is saved in network byte order, and therefore the class can be used
 * to create a packet in a buffer and send it over the network.
 */
class GREHeader
{
public:

    struct Protocol
    {
        enum Type
        {
            TRANSPARENT_ETHERNET_BRIDIGING = 0x6558,
        };
    };

////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public:
    //Empty constructor
    GREHeader() :
    _flagsVersion(0) //must initialize it because when doing setFlags or setVersion we mask it
    {}
    //Construct a GREHeader object from a given buffer, ordered in Network byte order
    inline GREHeader(uint8_t* packet);

    inline uint8_t* getPointer() { return (uint8_t*)this; }
    static inline uint32_t getSize() { return (uint32_t)sizeof(GREHeader); }

    inline bool isChecksumEnabled();
    inline bool isKeyEnabled();
    inline bool isSequenceNumberEnabled();
    inline void setFlags(bool checksumEnabled, bool keyEnabled,
        bool sequenceNumberEnable);

    inline uint8_t getVersion();
    inline void setVersion(uint8_t version);

    inline uint16_t getProtocolType();
    inline void setProtocolType(uint16_t protocolType);

    inline uint32_t getKey();
    inline void setKey(uint32_t key);

    void dump(FILE* fd);

public:
    uint16_t _flagsVersion;
    uint16_t _protocolType;
    uint32_t _key;
};

#include "GREHeader.inl"

#endif
