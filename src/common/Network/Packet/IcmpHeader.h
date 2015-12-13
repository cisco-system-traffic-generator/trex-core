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

#ifndef _ICMP_HEADER_H_
#define _ICMP_HEADER_H_

#include "PacketHeaderBase.h"
#include "IPHeader.h"

class ICMPHeader
{

public:
    ICMPHeader()
	{
		setCode(0);
		setType(0);
		setSeqNum(0xDEAD);
		setId(0xBEEF);
		setChecksum(0);
	}

    ICMPHeader(uint8_t  argType,
	       uint8_t  argCode,
	       uint16_t argId,
	       uint16_t argSeqNum)
	{
		setType(argType);
		setCode(argCode);
		setId(argId);
		setSeqNum(argSeqNum);
	}


    inline  void setCode(uint8_t data);
    inline  uint8_t getCode();

    inline  void setType(uint8_t data);
    inline  uint8_t getType();

    inline  void setSeqNum(uint16_t data);
    inline  uint16_t getSeqNum();

    inline  void setId(uint16_t data);
    inline  uint16_t getId();

    inline  void setChecksum(uint16_t data);
    inline  uint16_t getChecksum();

    inline void updateCheckSum(uint16_t len);
    inline bool isCheckSumOk(uint16_t len);
    inline uint16_t calcCheckSum(uint16_t len);


////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public: 
    inline	uint8_t*  getPointer          (){return (uint8_t*)this;}
    inline	uint32_t  getSize             (){return 8;}

    void    dump                (FILE*  fd);

private:
    uint8_t   myType;
    uint8_t   myCode;
    uint16_t  myChecksum;
    uint16_t  myId;
    uint16_t  mySeqNum;
};


#include "IcmpHeader.inl"

#endif 
