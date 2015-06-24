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

#ifndef _UDP_HEADER_H_
#define _UDP_HEADER_H_

#include "PacketHeaderBase.h"
#include "IPHeader.h"

class UDPHeader
{

public:
    UDPHeader()
	{
		setDestPort(0xDEAD);
		setSourcePort(0xBEEF);
		setChecksum(0);
		setLength(0);
	}

    UDPHeader(uint16_t argSourcePort,
              uint16_t argDestinationPort)
	{
		setDestPort(argDestinationPort);
		setSourcePort(argSourcePort);
	}


    inline  void    setSourcePort(uint16_t data);
    inline  uint16_t  getSourcePort();

    inline  void    setDestPort(uint16_t data);
    inline  uint16_t  getDestPort();

    inline  void    setLength(uint16_t data);
    inline  uint16_t  getLength();

    inline  void    setChecksum(uint16_t data);
    inline  uint16_t  getChecksum();

	inline void     updateCheckSum(IPHeader  *ipHeader);
    inline bool 	isCheckSumOk(IPHeader  *ipHeader);
    inline uint16_t   calcCheckSum(IPHeader  *ipHeader);

	inline  void	swapSrcDest();

////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public: 
    inline	uint8_t*  getPointer          (){return (uint8_t*)this;}
    inline	uint32_t  getSize             (){return 8;}

    inline	uint16_t  getNextProtocol     (){return getDestPort();};
    inline	void    setNextProtocol     (uint16_t argNextProtcool){setDestPort(argNextProtcool);};

    void    dump                (FILE*  fd);


private:
    uint16_t  mySourcePort;
    uint16_t  myDestinationPort;
    uint16_t  myLength;
    uint16_t  myChecksum;
};


#include "UdpHeader.inl"

#endif 

