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

#ifndef _MAC_ADDRESS_H_
#define _MAC_ADDRESS_H_

#include "CPktCmn.h"

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN  6 /**< Length of Ethernet address. */
#endif

class MacAddress
{
public:

    MacAddress()
    {
        set(0xca, 0xfe, 0xde, 0xad, 0xbe, 0xef);
    };

    MacAddress(uint8_t a0,
               uint8_t a1,
               uint8_t a2,
               uint8_t a3,
               uint8_t a4,
               uint8_t a5)
    {
        set(a0,
            a1,
            a2,
            a3,
            a4,
            a5);
    };

	MacAddress(uint8_t macAddr[ETHER_ADDR_LEN])
	{
		set(macAddr[0],
			macAddr[1],
			macAddr[2],
			macAddr[3],
			macAddr[4],
			macAddr[5]	);
	};

    void    set(uint8_t a0,
                uint8_t a1,
                uint8_t a2,
                uint8_t a3,
                uint8_t a4,
                uint8_t a5)
    {
        data[0]=a0;
        data[1]=a1;
        data[2]=a2;
        data[3]=a3;
        data[4]=a4;
        data[5]=a5;
    };

    void set(uint8_t *argPtr) {
        memcpy( data, argPtr, sizeof(data) );
    }

    void set(uint8_t *argPtr,uint8_t val) {
        memcpy( data, argPtr, sizeof(data) );
        data[5]=val;
    }


	bool isInvalidAddress() const
	{
		static MacAddress allZeros(0,0,0,0,0,0);
		static MacAddress cafeDeadBeef;
		return (*this == allZeros || *this == cafeDeadBeef);
	}
	void setIdentifierAsBogusAddr(uint32_t identifier)
	{
		*(uint32_t*)data = identifier;
	}

	uint32_t getIdentifierFromBogusAddr()
	{
		return *(uint32_t*)data;
	}

	bool operator == (const MacAddress& rhs) const
	{
		for(int i = 0; i < ETHER_ADDR_LEN; i++)
		{
			if(data[i] != rhs.data[i])
				return false;
		}

		return true;
	}

	uint8_t*	GetBuffer()
	{
		return data;
	}

	const uint8_t*	GetConstBuffer() const
	{
		return data;
	}
    void dump(FILE *fd) const;

	void copyToArray(uint8_t *arrayToFill) const
	{
        ((uint32_t*)arrayToFill)[0] = ((uint32_t*)data)[0];//Copy first 32bit
		((uint16_t*)arrayToFill)[2] = ((uint16_t*)data)[2];//Copy last 16bit
	}

public:
    uint8_t data[ETHER_ADDR_LEN];
};

#endif //_MAC_ADDRESS_H_
