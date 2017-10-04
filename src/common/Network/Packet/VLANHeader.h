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

#ifndef _VLAN_HEADER_H_
#define _VLAN_HEADER_H_
    
#include "PacketHeaderBase.h"
#include "EthernetHeader.h"


/*   
                              VLAN Header Fields
                              ------------------
                                    
    0       2   3   4                                                            15
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    |Priority| CFI |                                                             |
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    |                              Type                                          |
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/**
 * This class encapsulates a VLAN header.
 * It has fields that are equivalent to the VLAN header fields.
 * The data is saved in network byte order, and therefore the class can be used to create a packet in a buffer
 * and send it over the network.
 */ 
class VLANHeader
{


////////////////////////////////////////////////////////////////////////////////////////
// Field Manipulation
////////////////////////////////////////////////////////////////////////////////////////
public:

    // Sets the complete tag field without sub fields manipulation, host byte order
    void    setVlanTag          (uint16_t data);
    uint16_t  getVlanTag          ();

    void    setTagUserPriorty   (uint8_t data);
    uint8_t   getTagUserPriorty   ();
    
    bool    getTagCFI           ();
    void    setTagCFI           (bool);

    uint16_t  getTagID            ();
    void    setTagID            (uint16_t);

    void    incrementTagID(uint16_t inc_value);

    void    setFromPkt          (uint8_t* data);

    uint8_t   reconstructPkt      (uint8_t* destBuff);


////////////////////////////////////////////////////////////////////////////////////////
// Common Interface
////////////////////////////////////////////////////////////////////////////////////////

public: 
    uint8_t*  getPointer          (){return (uint8_t*)this;}
    uint32_t  getSize      (){return (uint32_t)sizeof(VLANHeader);}

    uint16_t  getNextProtocolNetOrder    ();
	uint16_t  getNextProtocolHostOrder   ();
    void    setNextProtocolFromNetOrder(uint16_t);
	void    setNextProtocolFromHostOrder(uint16_t);

    void    dump                (FILE*  fd);

	static uint16_t bytesToSkip(uint8_t* base)
	{
		return sizeof(VLANHeader);
	}

	static uint8_t reconstructFromBuffer(uint8_t* destBuffer, uint8_t* srcBuffer);

	static uint8_t fillReconstructionBuffer(uint8_t* destBuffer, uint8_t* srcBuffer);

    static uint8_t fillReconstructionBuffer(uint8_t* destBuffer, VLANHeader& vHeader);

    
    static uint32_t getMaxVlanTag()  { return (1<<12) - 1 ; }

public:
    uint16_t      myTag;
    uint16_t      myNextProtocol;
};

#include "VLANHeader.inl"

#endif //_VLAN_HEADER_H_

