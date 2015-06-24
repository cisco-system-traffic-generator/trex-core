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

#include "VLANHeader.h"


/*   
                              VLAN Header Fields
                              ------------------
                                    
    0       2   3   4                                                            15
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    |Priority| CFI |                Tag                                             |
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    |                              Type                                          |
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/


void    VLANHeader::dump             (FILE*  fd)
{
    fprintf(fd, "\nVLAN Header");
    fprintf(fd, "\nTag %d (0x%.2X), Pri %d, CFI - %d, Next protocol - %d (0x%.2X)",
            getTagID(), getTagID(), getTagUserPriorty(), getTagCFI(), getNextProtocolHostOrder(), getNextProtocolHostOrder());
    fprintf(fd, "\n");
}

uint8_t VLANHeader::reconstructFromBuffer(uint8_t* destBuffer, uint8_t* srcBuffer)
{
	uint8_t type = srcBuffer[0];
	uint8_t size = srcBuffer[1];	
	if((type != Tunnels::VLAN) || (size != sizeof(VLANHeader)))
	{
		// DBG_Error2(PACKET_DBG_TUNNEL_RECONSTRUCTION_ERROR,Tunnels::VLAN,size);
		return 0;
	}	
	memcpy(destBuffer,srcBuffer+2,sizeof(VLANHeader));
	return size;
}

uint8_t VLANHeader::fillReconstructionBuffer(uint8_t* destBuffer, uint8_t* srcBuffer)
{
	destBuffer[0] = (uint8_t)Tunnels::VLAN;
	destBuffer[1] = sizeof(VLANHeader);
	memcpy(destBuffer+2,srcBuffer,sizeof(VLANHeader));
	return sizeof(VLANHeader)+2;	
}

uint8_t VLANHeader::fillReconstructionBuffer(uint8_t* destBuffer, VLANHeader& vHeader)
{
	destBuffer[0] = (uint8_t)Tunnels::VLAN;
	destBuffer[1] = sizeof(VLANHeader);
    vHeader.reconstructPkt(destBuffer+2);
	return sizeof(VLANHeader)+2;	
}

#if 0
Status VLANHeader::parseAsText(uint8_t* srcBuffer, TextCollectorInterface &tc)
{
	uint8_t type = srcBuffer[0];
	uint8_t size = srcBuffer[1];	
	if((type != Tunnels::VLAN) //The buffer doesn't contain valid information
	   || (size < sizeof(VLANHeader))) //The buffer isn't big enough
	{
        return FAIL;
	}
	tc << "VLAN: ";
	uint32_t offset = 0;
	while (offset < size)
	{
		VLANHeader vlanHeader;
		vlanHeader.setFromPkt(&srcBuffer[2+offset]);
		
		tc.printf("Tag %d (0x%.2X), Pri %d, CFI - %d, Next protocol - %d (0x%.2X)\n",
            vlanHeader.getTagID(), vlanHeader.getTagID(), vlanHeader.getTagUserPriorty(), vlanHeader.getTagCFI(), vlanHeader.getNextProtocolHostOrder(), vlanHeader.getNextProtocolHostOrder());
		offset += sizeof(VLANHeader);
	}

	return SUCCESS;
}
#endif






