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

#ifndef _MPLS_HEADER_H_
#define _MPLS_HEADER_H_

#include "PacketHeaderBase.h"
#include "EthernetHeader.h"
#include <common/BigEndianBitManipulation.h>

#define MPLS_HDR_LEN 4

/* Reference: RFC 5462, RFC 3032
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                Label                  | TC  |S|       TTL     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *	Label:  Label Value, 20 bits
 *	TC:     Traffic Class field, 3 bits
 *	S:      Bottom of Stack, 1 bit
 *	TTL:    Time to Live, 8 bits
 */

class MPLSHeader
{
    uint32_t myTag;

public:
    MPLSHeader()
    {
        myTag = 0;
    }

    uint32_t getTag()
    {
        return (PKT_NTOHL(this->myTag));
    }

    void setTag(uint32_t)
    {
        this->myTag = PKT_HTONL(this->myTag);
    }

    uint32_t getLabel()
    {
        return getMaskBit32(PKT_NTOHL(this->myTag), 0, 19);
    }

    void setLabel(uint32_t label)
    {
        uint32_t tempTag = PKT_NTOHL(this->myTag);
        setMaskBit32(tempTag, 0, 19, label);
        this->myTag = PKT_HTONL(tempTag);
    }

    uint8_t getTc()
    {
        return getMaskBit32(PKT_NTOHL(this->myTag), 20, 22);
    }

    void setTc(uint8_t tc)
    {
        uint32_t tempTag = PKT_NTOHL(this->myTag);
        setMaskBit32(tempTag, 20, 22, tc);
        this->myTag = PKT_HTONL(tempTag);
    }

    bool getBottomOfStack()
    {
        // return this->s;
        return (getMaskBit32(PKT_NTOHL(this->myTag), 23, 23) == 1);
    }

    void setBottomOfStack(bool s)
    {
        uint32_t tempTag = PKT_NTOHL(this->myTag);
        setMaskBit32(tempTag, 23, 23, s ? 1 : 0);
        this->myTag = PKT_HTONL(tempTag);
    }

    uint8_t getTtl()
    {
        return getMaskBit32(PKT_NTOHL(this->myTag), 24, 31);
    }

    void setTtl(uint8_t ttl)
    {
        uint32_t tempTag = PKT_NTOHL(this->myTag);
        setMaskBit32(tempTag, 24, 31, ttl);
        this->myTag = PKT_HTONL(tempTag);
    }

    uint8_t *getPointer()
    {
        return (uint8_t *)this;
    }
};

#endif // _MPLS_HEADER_H_