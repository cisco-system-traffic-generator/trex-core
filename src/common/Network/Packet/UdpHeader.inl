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

inline void   UDPHeader::setSourcePort(uint16_t argSourcePort)
{
    mySourcePort = PKT_NTOHS(argSourcePort);
}

uint16_t UDPHeader::getSourcePort()
{
    return PKT_NTOHS(mySourcePort);
}

inline void    UDPHeader::setDestPort(uint16_t argDestPort)
{
    myDestinationPort = PKT_NTOHS(argDestPort);
}

uint16_t UDPHeader::getDestPort()
{
    return PKT_NTOHS(myDestinationPort);
}

inline void  UDPHeader::setLength(uint16_t argLength)
{
    myLength = PKT_NTOHS(argLength);
}

uint16_t UDPHeader::getLength()
{
    return PKT_NTOHS(myLength);
}

inline void    UDPHeader::setChecksum(uint16_t argNewChecksum)
{
    myChecksum = PKT_NTOHS(argNewChecksum);
}

uint16_t UDPHeader::getChecksum()
{
    return PKT_NTOHS(myChecksum);
}

void UDPHeader::updateCheckSum(IPHeader  *ipHeader)
{
	setChecksum(0);// must be here 

	myChecksum =calcCheckSum(ipHeader);
}

bool UDPHeader::isCheckSumOk(IPHeader  *ipHeader)
{
	uint16_t theChecksum= PKT_NTOHS(calcCheckSum(ipHeader));

	return(theChecksum == 0);
}

uint16_t UDPHeader::calcCheckSum(IPHeader  *ipHeader)
{
	IPPseudoHeader pseudo;

    uint16_t length= ipHeader->getTotalLength() - ipHeader->getHeaderLength();

	pseudo.m_source_ip = PKT_NTOHL(ipHeader->getSourceIp());

	pseudo.m_dest_ip   = PKT_NTOHL(ipHeader->getDestIp());

	pseudo.m_zero      = 0;

	pseudo.m_protocol  = ipHeader->getProtocol();

	pseudo.m_length    = PKT_NTOHS(length);

	uint16_t theChecksum = pkt_InetChecksum((uint8_t*)this,length);

	theChecksum = pkt_AddInetChecksum(theChecksum,pseudo.inetChecksum());

	return(theChecksum);
}

void UDPHeader::swapSrcDest()
{
	uint16_t tmp = myDestinationPort;
	myDestinationPort = mySourcePort;
	mySourcePort = tmp;
}



