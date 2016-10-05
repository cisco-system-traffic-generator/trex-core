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
    for (i=0; i<IPV6_16b_ADDR_GROUPS; i++) {
        argSourceAddress[i] = PKT_NTOHS(mySource[i]);
    }
}

inline void IPv6Header::setSourceIpv6(uint16_t *argSourceAddress)
{
    uint8_t i;
    for (i=0; i<IPV6_16b_ADDR_GROUPS; i++) {
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
    for (i=0; i<IPV6_16b_ADDR_GROUPS; i++) {
        myDestination[i] = PKT_HTONS(argDestAddress[i]);
    }
}

//--------------------------------
inline void IPv6Header::updateIpv6Src(uint16_t *ipsrc)
{
    uint8_t i;
    for (i=0; i<IPV6_16b_ADDR_GROUPS; i++) {
        mySource[i] = PKT_HTONS(ipsrc[i]);
    }
}

inline void IPv6Header::updateIpv6Dst(uint16_t *ipdst)
{
    uint8_t i;
    for (i=0; i<IPV6_16b_ADDR_GROUPS; i++) {
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
    for (i=0; i<IPV6_16b_ADDR_GROUPS_MSB; i++) {
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
    for (i=0; i<IPV6_16b_ADDR_GROUPS; i++) {
        tmp[i] = myDestination[i];
        myDestination[i] = mySource[i];
        mySource[i] = tmp[i];
    }
}
