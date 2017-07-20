/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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

#ifndef _IPV6_HEADER_H_
#define _IPV6_HEADER_H_

#include "PacketHeaderBase.h"

#define IPV6_16b_ADDR_GROUPS 8
#define IPV6_16b_ADDR_GROUPS_MSB 6
#define IPV6_HDR_LEN 40
static uint16_t default_ipv6[8] = { 0xDEAD, 0xBEEF, 0xDEAD, 0xBEEF,
                                    0xDEAD, 0xBEEF, 0xDEAD, 0xBEEF };

class IPv6Header
{
/*
 *      IPV6 extension headers
 */
    enum ext_headers_e {
        IPPROTO_HOPOPTS   = 0,       /* IPv6 hop-by-hop options      */
        IPPROTO_ROUTING   = 43,      /* IPv6 routing header          */
        IPPROTO_FRAGMENT  = 44,      /* IPv6 fragmentation header    */
        IPPROTO_ENCAP_SEC = 50,      /* IPv6 encapsulation security paylod header   */
        IPPROTO_AUTH      = 51,      /* IPv6 authentication header   */
        IPPROTO_ICMPV6    = 58,      /* ICMPv6                       */
        IPPROTO_NONE      = 59,      /* IPv6 no next header          */
        IPPROTO_DSTOPTS   = 60,      /* IPv6 destination options     */
        IPPROTO_MH        = 135,     /* IPv6 mobility header         */
    };

public:
    IPv6Header()
    {
        setDestIpv6(default_ipv6);
        setSourceIpv6(default_ipv6);
        setTrafficClass(0xDD);
    };

    IPv6Header  (uint16_t *argSource, uint16_t *argDestinaction, uint8_t  argTrafficClass)
    {
        setDestIpv6(argDestinaction);
        setSourceIpv6(argSource);
        setTrafficClass(argTrafficClass);
    };

    enum
    {
        DefaultSize = 40
    };

public:
    inline uint8_t  getVersion();
    inline void     setVersion(uint8_t);
    inline uint8_t  getHeaderLength() {return IPV6_HDR_LEN;}
    inline uint16_t getTrafficClass();
    inline void     setTrafficClass(uint16_t);
    inline uint32_t getFlowLabel();
    inline void     setFlowLabel(uint32_t);
    inline uint16_t getPayloadLen();
    inline void     setPayloadLen(uint16_t);
    inline uint8_t  getNextHdr();
    inline void     setNextHdr(uint8_t);
    inline uint8_t  getHopLimit();
    inline void     setHopLimit(uint8_t);
    inline void     getSourceIpv6(uint16_t *);
    inline void     setSourceIpv6(uint16_t *);
    inline void     getDestIpv6(uint16_t *);
    inline void     setDestIpv6(uint16_t *);
    uint8_t         getl4Proto(uint8_t *pkt, uint16_t pkt_len, uint8_t *&p_l4);
    inline void     updateTrafficClass(uint8_t newclass);
    inline void     updatePayloadLength(uint16_t newlen);
    inline void     updateIpv6Src(uint16_t *ipsrc);
    inline void     updateIpv6Dst(uint16_t *ipdst);
    inline void     updateMSBIpv6Src(uint16_t *ipsrc);
    inline void     updateMSBIpv6Dst(uint16_t *ipdst);
    inline void     updateLSBIpv6Src(uint32_t ipsrc);
    inline void     updateLSBIpv6Dst(uint32_t ipdst);
    inline void     swapSrcDest();

    inline uint32_t getSourceIpv6LSB();
    inline uint32_t getDestIpv6LSB();


////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public:
    inline  uint8_t*  getPointer          (){return (uint8_t*)this;}
    inline  uint32_t  getSize             (){return  getHeaderLength();}

public:
    uint32_t myVer_TrafficClass_FlowLabel;
    uint16_t myPayloadLen;
    uint8_t  myNextHdr;
    uint8_t  myHopLimit;
    uint16_t mySource[IPV6_16b_ADDR_GROUPS];
    uint16_t myDestination[IPV6_16b_ADDR_GROUPS];
    uint32_t myOption[1];
};


class IPv6PseudoHeader
{
public:
  uint32_t m_source_ip;
  uint32_t m_dest_ip;
  uint8_t  m_zero;
  uint8_t  m_protocol;
  uint16_t m_length;

public:
    inline uint8_t* getPointer(){return (uint8_t*)this;}
    inline uint32_t getSize();
    inline uint16_t inetChecksum();
};


/////////////////////////////////////////////
// inline functions implementation
/////////////////////////////////////////////

#include <common/BigEndianBitManipulation.h>

//--------------------------------
inline uint8_t IPv6Header::getVersion()
{
    return getMaskBit32(PKT_NTOHL(myVer_TrafficClass_FlowLabel), 0, 3);
}

inline void  IPv6Header::setVersion(uint8_t argVersion)
{
    uint32_t myVer = PKT_HTONL(myVer_TrafficClass_FlowLabel);
    setMaskBit32(myVer, 0, 3, argVersion);
    myVer_TrafficClass_FlowLabel = PKT_NTOHL(myVer);
}

//--------------------------------
inline uint16_t IPv6Header::getTrafficClass()
{
    return  getMaskBit32(PKT_NTOHL(myVer_TrafficClass_FlowLabel),  4, 11);
}

inline void 
IPv6Header::setTrafficClass(uint16_t argTrafficClass)
{
    uint32_t myTrafficClass = PKT_HTONL(myVer_TrafficClass_FlowLabel);
    setMaskBit32(myTrafficClass, 4, 11, argTrafficClass);
    myVer_TrafficClass_FlowLabel = PKT_NTOHL(myTrafficClass);
}

//--------------------------------
inline uint32_t IPv6Header::getFlowLabel()
{
    return  getMaskBit32(PKT_NTOHL(myVer_TrafficClass_FlowLabel), 12, 31);
}

inline void  IPv6Header::setFlowLabel(uint32_t argFlowLabel)
{
    uint32_t myFlowLabel = PKT_HTONL(myVer_TrafficClass_FlowLabel);
    setMaskBit32(myFlowLabel, 12, 31, argFlowLabel);
    myVer_TrafficClass_FlowLabel = PKT_NTOHL(myFlowLabel);
}

inline uint32_t IPv6Header::getSourceIpv6LSB(){
    return (PKT_NTOHL(*((uint32_t*)&mySource[6])));
}

inline uint32_t IPv6Header::getDestIpv6LSB(){
    return (PKT_NTOHL(*((uint32_t*)&myDestination[6])));
}


//--------------------------------
inline uint16_t IPv6Header::getPayloadLen()
{
    return  PKT_NTOHS(myPayloadLen);
}

inline void  IPv6Header::setPayloadLen(uint16_t argPayloadLen)
{
    myPayloadLen = PKT_HTONS(argPayloadLen);
}

//--------------------------------
inline uint8_t IPv6Header::getNextHdr()
{
    return  myNextHdr;
}

inline void  IPv6Header::setNextHdr(uint8_t argNextHdr)
{
    myNextHdr = argNextHdr;
}

//--------------------------------
inline uint8_t IPv6Header::getHopLimit()
{
    return  myHopLimit;
}

inline void  IPv6Header::setHopLimit(uint8_t argHopLimit)
{
    myHopLimit = argHopLimit;
}

//--------------------------------
inline  void IPv6Header::getSourceIpv6(uint16_t *argSourceAddress)
{
    uint8_t i;
    for (i = 0; i < IPV6_16b_ADDR_GROUPS; i++) {
        argSourceAddress[i] = PKT_NTOHS(mySource[i]);
    }
}

inline void IPv6Header::setSourceIpv6(uint16_t *argSourceAddress)
{
    uint8_t i;
    for (i = 0; i < IPV6_16b_ADDR_GROUPS; i++) {
        mySource[i] = PKT_HTONS(argSourceAddress[i]);
    }
}

//--------------------------------
inline  void IPv6Header::getDestIpv6(uint16_t *argDestAddress)
{
    uint8_t i;
    for (i=0; i<IPV6_16b_ADDR_GROUPS; i++) {
        argDestAddress[i] = PKT_NTOHS(myDestination[i]);
    }
}

inline void IPv6Header::setDestIpv6(uint16_t *argDestAddress)
{
    uint8_t i;
    for (i = 0; i < IPV6_16b_ADDR_GROUPS; i++) {
        myDestination[i] = PKT_HTONS(argDestAddress[i]);
    }
}

//--------------------------------
inline void IPv6Header::updateIpv6Src(uint16_t *ipsrc)
{
    uint8_t i;
    for (i = 0; i < IPV6_16b_ADDR_GROUPS; i++) {
        mySource[i] = PKT_HTONS(ipsrc[i]);
    }
}

inline void IPv6Header::updateIpv6Dst(uint16_t *ipdst)
{
    uint8_t i;
    for (i = 0; i < IPV6_16b_ADDR_GROUPS; i++) {
        myDestination[i] = PKT_HTONS(ipdst[i]);
    }
}

//--------------------------------
inline void IPv6Header::updateMSBIpv6Src(uint16_t *ipsrc)
{
    uint8_t i;
    for (i=0; i<IPV6_16b_ADDR_GROUPS_MSB; i++) {
        mySource[i] = PKT_HTONS(ipsrc[i]);
    }
}

inline void IPv6Header::updateMSBIpv6Dst(uint16_t *ipdst)
{
    uint8_t i;
    for (i = 0; i < IPV6_16b_ADDR_GROUPS_MSB; i++) {
        myDestination[i] = PKT_HTONS(ipdst[i]);
    }
}

//--------------------------------
inline void IPv6Header::updateLSBIpv6Src(uint32_t ipsrc)
{
     uint32_t *lsb = (uint32_t *)&mySource[6];
     *lsb = PKT_HTONL(ipsrc);
}

inline void IPv6Header::updateLSBIpv6Dst(uint32_t ipdst)
{
     uint32_t *lsb = (uint32_t *)&myDestination[6];
     *lsb = PKT_HTONL(ipdst);
}

//--------------------------------
inline void IPv6Header::swapSrcDest()
{
    uint8_t i;
    uint16_t tmp[IPV6_16b_ADDR_GROUPS];
    for (i = 0; i < IPV6_16b_ADDR_GROUPS; i++) {
        tmp[i] = myDestination[i];
        myDestination[i] = mySource[i];
        mySource[i] = tmp[i];
    }
}

#endif
