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

#include <common/BigEndianBitManipulation.h> 

inline void   TCPHeader::setSourcePort(uint16_t argSourcePort)
{
    mySourcePort = PKT_NTOHS(argSourcePort);
}

inline uint16_t TCPHeader::getSourcePort()
{
    return(PKT_NTOHS(mySourcePort));
}

inline void   TCPHeader::setDestPort(uint16_t argDestinationPort)
{
    myDestinationPort = PKT_NTOHS(argDestinationPort);
}

inline uint16_t TCPHeader::getDestPort()
{
    return (PKT_NTOHS(myDestinationPort));
}

inline void   TCPHeader::setSeqNumber(uint32_t argSeqNum)
{
    mySeqNum = PKT_NTOHL(argSeqNum);
}

inline uint32_t TCPHeader::getSeqNumber()
{
    return (PKT_NTOHL(mySeqNum));
}

inline void TCPHeader::setAckNumber(uint32_t argAckNum)
{
    myAckNum = PKT_NTOHL(argAckNum);
}

inline uint32_t TCPHeader::getAckNumber()
{
    return(PKT_NTOHL(myAckNum));
}

inline void  TCPHeader::setHeaderLength(uint8_t argHeaderLength)
{
    setMaskBit8(myHeaderLength, 0, 3, argHeaderLength >> 2);
}

inline uint8_t TCPHeader::getHeaderLength()
{
    return  getMaskBit8(myHeaderLength, 0, 3) << 2;
}

inline void TCPHeader::setFlag(uint8_t data)
{
    btSetMaskBit8(myFlags,5,0,data);
}

inline  uint8_t TCPHeader::getFlags()
{
    return(myFlags & 0x3f);
}

inline void TCPHeader::setFinFlag(bool toSet)
{
    if(toSet)
    {
        myFlags |=  Flag::FIN;
    }
    else
    {
        myFlags &= ~Flag::FIN;
    }
}

inline bool TCPHeader::getFinFlag()
{
    return(getFlags() & Flag::FIN)?true:false;
}

inline void TCPHeader::setSynFlag(bool toSet)
{
    if(toSet)
    {
        myFlags|=Flag::SYN;
    }
    else
    {
        myFlags&=~Flag::SYN;
    }
}

inline	bool TCPHeader::getSynFlag()
{
    return(getFlags() & Flag::SYN)?true:false;
}

inline void TCPHeader::setResetFlag(bool toSet)
{
    if(toSet)
    {
        myFlags|=Flag::RST;
    }
    else
    {
        myFlags&=~Flag::RST;
    }
}

inline	bool TCPHeader::getResetFlag()
{
    return(getFlags() & Flag::RST)?true:false;
}

inline void TCPHeader::setPushFlag(bool toSet)
{
    if(toSet)
    {
        myFlags|=Flag::PSH;
    }
    else
    {
        myFlags&=~Flag::PSH;
    }
}

inline	bool TCPHeader::getPushFlag()
{
    return(getFlags() & Flag::PSH)?true:false;
}

inline void TCPHeader::setAckFlag(bool toSet)
{
    if(toSet)
    {
        myFlags|=Flag::ACK;
    }
    else
    {
        myFlags&=~Flag::ACK;
    }
}

inline bool TCPHeader::getAckFlag()
{
    return(getFlags() & Flag::ACK)?true:false;
}

inline void TCPHeader::setUrgentFlag(bool toSet)
{
    if(toSet)
    {
        myFlags|=Flag::URG;
    }
    else
    {
        myFlags&=~Flag::URG;
    }
}

inline bool TCPHeader::getUrgentFlag()
{
    return(getFlags() & Flag::URG)?true:false;
}

//---------------------------------------
inline void   TCPHeader::setWindowSize(uint16_t argWindowSize)
{
    myWindowSize = PKT_NTOHS(argWindowSize);
}
inline uint16_t TCPHeader::getWindowSize()
{
    return PKT_NTOHS(myWindowSize);
}

inline void   TCPHeader::setChecksum(uint16_t argChecksum)
{
    myChecksum = PKT_NTOHS(argChecksum);

}

inline	uint16_t TCPHeader::getChecksum()
{
    return PKT_NTOHS(myChecksum);
}

inline void   TCPHeader::setUrgentOffset(uint16_t argUrgentOffset)
{
    myUrgentPtr = argUrgentOffset;
}

inline	uint16_t TCPHeader::getUrgentOffset()
{
    return PKT_NTOHS(myUrgentPtr);
}

inline uint32_t * TCPHeader::getOptionPtr()
{
    return(myOption);
}





