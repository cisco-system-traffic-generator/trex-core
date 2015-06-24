#ifndef PKT_CMN_H
#define PKT_CMN_H
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


                                
#include <common/c_common.h>
#include <common/bitMan.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pal_utl.h"


#define PKT_HTONL(x) (PAL_NTOHL(x))
#define PKT_HTONS(x) (PAL_NTOHS(x))

#define PKT_NTOHL(x) (PAL_NTOHL(x))
#define PKT_NTOHS(x) (PAL_NTOHS(x))


// returns cs in NETWROK order
uint16_t pkt_InetChecksum(uint8_t* data , uint16_t len);

// len MUST be an even number !!!
// len2 can be odd.
// returns cs in NETWROK order
uint16_t pkt_InetChecksum(uint8_t* data , uint16_t len, uint8_t* data2 , uint16_t len2);
    
// this functiion updates an inet-checksum.
// It accepts the checksum field AS IS from the packet, the old byte's value
// and the new byte's value.
// the cs, old and new values must be entered in NETWORK byte order !!!
// the return value is also in NETWORK byte order !!
uint16_t pkt_UpdateInetChecksum(uint16_t csFieldFromPacket, uint16_t oldVal, uint16_t newVal);

// checksum and csToSubtract are two uint16_t cs fields AS THEY APPEAR INSIDE A PACKET !
uint16_t pkt_SubtractInetChecksum(uint16_t checksum, uint16_t csToSubtract);

// checksum and csToAdd are two uint16_t cs fields AS THEY APPEAR INSIDE A PACKET !
uint16_t pkt_AddInetChecksum(uint16_t checksum, uint16_t csToAdd);


struct Tunnels
{
	enum Type 
	{
		// basic tunnels have a bit each. They can be bitwise OR ed.
		// WARNING: We use this number as a Uint8 in some places - don't go over 1 byte !!!

		//Another warning: DO NOT change the values of these symbols, unless you have permission from everyone who
		//uses them. These values are externally exposed through CmdlTunnel interfaces, and therefore JRT relies on these 
		//specific values. (Assi - Jan 2006)
		Empty			= 0x00,
		UNTUNNELED_Marker       = 0x01,
		VLAN			= 0x01, 
		MPLS			= 0x02, 
		L2TP			= 0x04,
		IPinIP			= 0x08,
        GRE			    = 0x10,
		Ethernet		= 0x20,//This is not tunneled. It's an exception, until all these values are changed.
		AnyIP			= 0x40,
        AnyTunneled     = 0x7f,
		TUNNELED_Marker = AnyTunneled,//Any sum of the values written above mustn't reach this value		
		Unrecognized	= 0x80,
        GTP             = 0x81        
	};
};




#endif 

