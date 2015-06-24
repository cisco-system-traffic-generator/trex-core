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

#ifndef _IP_HEADER_H_
#define _IP_HEADER_H_

#include "PacketHeaderBase.h"

#define IPV4_HDR_LEN 20

class IPHeader
{

public:
    IPHeader()
    {
		myVer_HeaderLength = 0;  //must initialize it cause when doing setLenght or setVersion we mask it (found in purify)
        setDestIp(0xDEADBEEF);
        setSourceIp(0xBEEFDEAD);
        setProtocol(0xDD);
    };
    
    IPHeader          (uint32_t argSource,
                       uint32_t argDestinaction,
                       uint8_t  argProtocol)
    {
		myVer_HeaderLength = 0;  //must initialize it cause when doing setLenght or setVersion we mask it
        setDestIp(argDestinaction);
        setSourceIp(argSource);
        setProtocol(argProtocol);
    };


    struct Protocol
    {
        enum Type 
        {
            IP     	= 0x04 , 
            TCP     = 0x06 , 
            UDP     = 0x11 , 
            ICMP    = 0x01 , 
            IGMP    = 0x02,
            ESP     = 0x32,
            AH      = 0x33,
            GRE     = 0x2F,
            IPV6_ICMP = 0x3A,
            IPV6_NONXT = 0x3B,
        };
        //get the IP-Protocol name by protocol number
        static char * interpretIpProtocolName(uint8_t argType);
    };

    enum
    {
        DefaultSize = 20
    };


public:

    inline  uint8_t   getVersion          ();
    inline  void    setVersion          (uint8_t);
    
    /**
     * Return the header length in bytes
     * 
     * @return 
     */
    inline  uint8_t   getHeaderLength     ();

    /**
     * Receives the header length in bytes and sets it
     * appropriately in the packet
     */
    inline  void    setHeaderLength     (uint8_t);

    inline  uint8_t   getTOS              ();
    inline  void    setTOS              (uint8_t);

    inline  uint16_t  getTotalLength      ();
    inline  void    setTotalLength      (uint16_t);

    inline  uint16_t  getId               ();
    inline  void    setId               (uint16_t);

    inline  uint16_t  getFragmentOffset   ();// return the result in bytes
    inline  bool    isFirstFragment     ();
    inline  bool    isMiddleFragment    ();
    inline  bool    isFragmented        ();
    inline  bool    isMoreFragments     ();
    inline  bool    isDontFragment      ();
    inline  bool    isLastFragment      ();
    inline  bool    isNotFirstFragment  ();
    inline  void    setFragment         (uint16_t         argOffset, 
                                         bool           argMoreFrag,
                                         bool           argDontFrag);

    inline  uint8_t   getTimeToLive       (void);
    inline  void    setTimeToLive       (uint8_t);
                                        
    inline  uint8_t   getProtocol         (void);
    inline  void    setProtocol         (uint8_t);
                                        
    inline  uint32_t  getSourceIp         ();
    inline  void    setSourceIp         (uint32_t);
                                        
    inline  uint32_t  getDestIp           ();
    inline  void    setDestIp           (uint32_t);
                                        
    bool            isMulticast         ();
    bool            isBroadcast         ();
                                        
    inline  uint16_t  getChecksum         ();
    bool            isChecksumOK        ();

public:

    inline  void    updateTos           (uint8_t newTos);
	inline	void	updateTotalLength	(uint16_t newlen);
    inline  void    updateIpSrc(uint32_t ipsrc);
    inline  void    updateIpDst(uint32_t ipsrc);



    inline  void    updateCheckSum      ();
    inline  void    updateCheckSum2(uint8_t* data1, uint16_t len1, uint8_t* data2 , uint16_t len2);

	inline 	void	swapSrcDest			();

////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public: 
    inline  uint8_t*  getPointer          (){return (uint8_t*)this;}
    inline  uint32_t  getSize             (){return  getHeaderLength();}

    inline  uint8_t  getNextProtocol     ();

    void    dump                (FILE*  fd);

    inline  uint8_t*  getOption          (){ return ((uint8_t*)&myOption[0]); }
    inline  uint8_t   getOptionLen()    { return ( getHeaderLength() - DefaultSize ); }


public:
    uint8_t       myVer_HeaderLength;
    uint8_t       myTos;
    uint16_t      myLength;
        
    uint16_t      myId;
    uint16_t      myFrag;
        
    uint8_t       myTTL;
    uint8_t       myProtocol;
    uint16_t      myChecksum;

    uint32_t      mySource;
    uint32_t      myDestination;
    uint32_t      myOption[1];
};


class IPPseudoHeader
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


#include "IPHeader.inl"

#endif 

