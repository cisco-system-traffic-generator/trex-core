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

#ifndef _IPV6_HEADER_H_
#define _IPV6_HEADER_H_

#include "PacketHeaderBase.h"

#define IPV6_16b_ADDR_GROUPS 8
#define IPV6_16b_ADDR_GROUPS_MSB 6
#define IPV6_HDR_LEN 40
static uint16_t default_ipv6[8] = { 0xDEAD, 0xBEEF,
                                              0xDEAD, 0xBEEF,
                                              0xDEAD, 0xBEEF,
                                              0xDEAD, 0xBEEF };


class IPv6Header
{

public:
    IPv6Header()
    {
        setDestIpv6(default_ipv6);
        setSourceIpv6(default_ipv6);
        setTrafficClass(0xDD);
    };
    
    IPv6Header  (uint16_t *argSource,
                       uint16_t *argDestinaction,
                       uint8_t  argTrafficClass)
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

    inline  uint8_t   getVersion          ();
    inline  void    setVersion          (uint8_t);
    
    inline  uint8_t   getHeaderLength     (){return (IPV6_HDR_LEN);}

    inline  uint16_t   getTrafficClass              ();
    inline  void    setTrafficClass             (uint16_t);
    
    inline  uint32_t   getFlowLabel     ();
    inline  void    setFlowLabel     (uint32_t);
    
    inline  uint16_t   getPayloadLen     ();
    inline  void    setPayloadLen     (uint16_t);

    inline  uint8_t  getNextHdr      ();
    inline  void    setNextHdr      (uint8_t);

    inline  uint8_t  getHopLimit      ();
    inline  void    setHopLimit      (uint8_t);
                                     
    inline  void    getSourceIpv6         (uint16_t *);
    inline  void    setSourceIpv6         (uint16_t *);
                                        
    inline  void    getDestIpv6           (uint16_t *);
    inline  void    setDestIpv6           (uint16_t *);

public:

    inline  void    updateTrafficClass(uint8_t newclass);
    inline  void    updatePayloadLength(uint16_t newlen);
    inline  void    updateIpv6Src(uint16_t *ipsrc);
    inline  void    updateIpv6Dst(uint16_t *ipdst);
    inline  void    updateMSBIpv6Src(uint16_t *ipsrc);
    inline  void    updateMSBIpv6Dst(uint16_t *ipdst);
    inline  void    updateLSBIpv6Src(uint32_t ipsrc);
    inline  void    updateLSBIpv6Dst(uint32_t ipdst);

    inline  void    swapSrcDest();

////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public: 
    inline  uint8_t*  getPointer          (){return (uint8_t*)this;}
    inline  uint32_t  getSize             (){return  getHeaderLength();}

    void    dump                (FILE*  fd);


public:
    uint32_t     myVer_TrafficClass_FlowLabel;
        
    uint16_t     myPayloadLen;
    uint8_t       myNextHdr;
    uint8_t       myHopLimit;

    uint16_t      mySource[IPV6_16b_ADDR_GROUPS];
    uint16_t      myDestination[IPV6_16b_ADDR_GROUPS];
    uint32_t      myOption[1];
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


#include "IPv6Header.inl"

#endif 

