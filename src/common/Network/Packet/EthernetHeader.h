/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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

#ifndef _ETHERNET_HEADER_H_
#define _ETHERNET_HEADER_H_

#include "PacketHeaderBase.h"
#include "MacAddress.h"

#define ETH_HDR_LEN 14

/**
 * This class encapsulates an ethernet header.
 * It has fields that are equivalent to the ethernet header fields.
 * The data is saved in network byte order, and therefore the class can be used to create a packet in a buffer
 * and send it over the network.
 */ 
class EthernetHeader
{
public:

    struct Protocol
    {
        enum    Type
        {
            IP              = 0x0800,
            VLAN            = 0x8100,
            ARP             = 0x0806,
            IPv6            = 0x86DD,
            MPLS_Unicast    = 0x8847,
            MPLS_Multicast  = 0x8848,
            PPP             = 0x880b,
            PPPoED          = 0x8863,
            PPPoES          = 0x8864,
            QINQ            = 0x88A8
        };
    };

	
////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public: 
	//Empty constructor
	EthernetHeader() :
        myProtocol(0)
	{}
	//Construct an EthernetHeader object from a given buffer, ordered in Network byte order
	inline EthernetHeader(uint8_t* packet);

    inline  uint8_t*  getPointer          (){return (uint8_t*)this;}
    inline  uint32_t  getSize () {
        return ( (getNextProtocol() == Protocol::VLAN) ? 18 : 14);
    }
    
    // Get dest MAC pointer
    MacAddress *getDestMacP()             { return &myDestination; }

    // Get source MAC pointer
    MacAddress *getSrcMacP()              { return &mySource; }

    //Returns the next protocol, in host byte order
    inline  uint16_t  getNextProtocol     ();

    //Set the next protocol value. The argument is receieved in host byte order.
    inline  void    setNextProtocol     (uint16_t);

    // Retrieve VLAN fields for tag and protocol information
    inline  uint16_t getVlanTag ();
    inline  uint16_t getVlanProtocol ();
    inline  uint16_t getQinQTag ();
    inline  uint16_t getQinQProtocol ();
    void    dump                (FILE*  fd);


public:
    MacAddress          myDestination;
    MacAddress          mySource;
private:
    uint16_t              myProtocol;
    uint16_t              myVlanTag;
    uint16_t              myVlanProtocol;
    uint16_t            myQinQTag;
    uint16_t            myQinQProtocol;
};

#include "EthernetHeader.inl"

#endif //_ETHERNET_HEADER_H_

