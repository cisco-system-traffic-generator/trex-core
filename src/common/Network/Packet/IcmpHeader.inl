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

inline void ICMPHeader::setCode(uint8_t argCode)
{
    myCode = argCode;
}

inline uint8_t ICMPHeader::getCode()
{
    return myCode;
}

inline void   ICMPHeader::setType(uint8_t argType)
{
    myType = argType;
}

inline uint8_t ICMPHeader::getType()
{
    return myType;
}

inline void    ICMPHeader::setSeqNum(uint16_t argSeqNum)
{
    mySeqNum = PKT_NTOHS(argSeqNum);
}

inline uint16_t ICMPHeader::getSeqNum()
{
    return PKT_NTOHS(mySeqNum);
}

inline void    ICMPHeader::setId(uint16_t argId)
{
    myId = PKT_NTOHS(argId);
}

inline uint16_t ICMPHeader::getId()
{
    return PKT_NTOHS(myId);
}

inline void ICMPHeader::setChecksum(uint16_t argNewChecksum)
{
    myChecksum = PKT_NTOHS(argNewChecksum);
}

inline uint16_t ICMPHeader::getChecksum()
{
    return PKT_NTOHS(myChecksum);
}

inline void ICMPHeader::updateCheckSum(uint16_t len)
{
	setChecksum(0);// must be here 

	myChecksum =calcCheckSum(len);
}

inline bool ICMPHeader::isCheckSumOk(uint16_t len)
{
	uint16_t theChecksum= PKT_NTOHS(calcCheckSum(len));

	return(theChecksum == 0);
}

// len is in bytes. Including ICMP header + data.
inline uint16_t ICMPHeader::calcCheckSum(uint16_t len)
{
	uint16_t theChecksum = pkt_InetChecksum((uint8_t*)this, len);

	return(theChecksum);
}
