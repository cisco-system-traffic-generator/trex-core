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


inline void  IPHeader::setVersion(uint8_t argVersion)
{
    setMaskBit8(myVer_HeaderLength, 0, 3, argVersion);
}

inline uint8_t IPHeader::getVersion()
{
    return  getMaskBit8(myVer_HeaderLength, 0, 3);
}

inline uint8_t IPHeader::getHeaderLength()
{
    return (getMaskBit8(myVer_HeaderLength, 4, 7) << 2);
}

inline void  IPHeader::setHeaderLength(uint8_t    argLength)
{
    setMaskBit8(myVer_HeaderLength, 4, 7, (argLength>>2));
}

inline uint8_t IPHeader::getNextProtocol()
{
	return myProtocol;
}

//--------------------------------

inline void  IPHeader::setTOS(uint8_t argTOS)
{
    myTos = argTOS;
}

inline uint8_t IPHeader::getTOS()
{
    return myTos;
}

//--------------------------------
// length of ip packet 
inline void IPHeader::setTotalLength(uint16_t argLength)
{
    myLength = PKT_NTOHS(argLength);
}

inline  uint16_t  IPHeader::getTotalLength()
{
    return(PKT_NTOHS(myLength));
}

//--------------------------------
inline  uint16_t  IPHeader::getId()
{
    return(PKT_NTOHS(myId));
}

inline void IPHeader::setId(uint16_t argID)
{
    myId = PKT_NTOHS(argID);
}

//--------------------------------

uint16_t IPHeader::getFragmentOffset()
{
    uint16_t  theFrag = (PKT_NTOHS(myFrag));
    return ((theFrag & 0x1FFF) << 3);
    //return (getMaskBit16(theFrag, 3, 15) << 3);//The field is in 8 byte units
}

bool    IPHeader::isMoreFragments()
{
    uint16_t  theFrag = (PKT_NTOHS(myFrag));
    if(theFrag & 0x2000)
    //if(getMaskBit16(theFrag, 2, 2) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool    IPHeader::isDontFragment()
{
    uint16_t  theFrag = (PKT_NTOHS(myFrag));
    if(theFrag & 0x4000)
    //if(getMaskBit16(theFrag, 1, 1) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

inline void IPHeader::setFragment      (uint16_t    argOffset , 
                                        bool      argMoreFrag,
                                        bool      argDontFrag)
{
    uint16_t  theFragmentWord = 0;
    setMaskBit16(theFragmentWord, 3, 15, argOffset);
    setMaskBit16(theFragmentWord, 1, 1,  argDontFrag ? 1 : 0);
    setMaskBit16(theFragmentWord, 2, 2,  argMoreFrag ? 1 : 0);
    myFrag = (PKT_NTOHS(theFragmentWord));
}

inline bool IPHeader::isFirstFragment()
{
    return((getFragmentOffset() == 0) && (isMoreFragments() == true));
}

inline bool IPHeader::isMiddleFragment()
{
    return((getFragmentOffset() != 0) && (isMoreFragments() == true));
}

inline bool IPHeader::isFragmented()
{
    return((getFragmentOffset() != 0) || (isMoreFragments() == true));
}

inline bool IPHeader::isLastFragment()
{
    return((getFragmentOffset() != 0) && (isMoreFragments() == false));
}

//return true if this is fragment but not first 
inline bool IPHeader::isNotFirstFragment()
{
    return((isFragmented() == true)  && (isFirstFragment() == false));

}


//--------------------------------
inline uint8_t IPHeader::getTimeToLive()
{
    return(myTTL);
}
inline void IPHeader::setTimeToLive(uint8_t argTTL)
{
    myTTL = argTTL;
}

//--------------------------------
inline uint8_t IPHeader::getProtocol()
{
    return (myProtocol);
}

inline void IPHeader::setProtocol(uint8_t argProtocol)
{
    myProtocol = argProtocol;
}
//--------------------------------


inline  uint32_t IPHeader::getSourceIp()
{
    return (PKT_NTOHL(mySource));
}

inline void IPHeader::setSourceIp(uint32_t argSourceAddress)
{
    mySource = PKT_NTOHL(argSourceAddress);
}

//--------------------------------
inline  uint32_t IPHeader::getDestIp()
{
    return (PKT_NTOHL(myDestination));
}

inline void IPHeader::setDestIp(uint32_t argDestAddress)
{
    myDestination = PKT_NTOHL(argDestAddress);
}

//--------------------------------
inline bool IPHeader::isMulticast()
{
    return((getDestIp() & 0xf0) == 0xe0);
}

inline bool IPHeader::isBroadcast()
{
    return(getDestIp() == 0xffffffff);
}
//--------------------------------

inline uint16_t IPHeader::getChecksum()
{
    return PKT_NTOHS(myChecksum);
}

inline bool IPHeader::isChecksumOK()
{
    uint16_t theChecksum = pkt_InetChecksum(getPointer(),(uint16_t)getSize() );
    return(theChecksum == 0);
}


inline void IPHeader::updateTos(uint8_t newTos)
{
    uint16_t oldWord = *((uint16_t *)(getPointer()));
    myTos = newTos;
    uint16_t newWord = *((uint16_t *)(getPointer()));
    myChecksum =   pkt_UpdateInetChecksum(myChecksum,oldWord,newWord);
}



    
inline  void    IPHeader::updateIpDst(uint32_t ipsrc){
    uint32_t old      = myDestination;
    uint32_t _new_src = PKT_NTOHL(ipsrc);

    myChecksum =   pkt_UpdateInetChecksum(myChecksum,(old&0xffff0000)>>16,((_new_src&0xffff0000)>>16));
    myChecksum =   pkt_UpdateInetChecksum(myChecksum,(old&0xffff),(_new_src&0xffff));
    myDestination=_new_src;
}

inline  void   IPHeader::updateIpSrc(uint32_t ipsrc){
    uint32_t old      = mySource;
    uint32_t _new_src = PKT_NTOHL(ipsrc);

    myChecksum =   pkt_UpdateInetChecksum(myChecksum,(old&0xffff0000)>>16,((_new_src&0xffff0000)>>16));
    myChecksum =   pkt_UpdateInetChecksum(myChecksum,(old&0xffff),(_new_src&0xffff));
    mySource=_new_src;
}


inline void IPHeader::updateTotalLength(uint16_t newlen)
{
    uint16_t oldLen = myLength;
    myLength = PKT_NTOHS(newlen);
    myChecksum =   pkt_UpdateInetChecksum(myChecksum,oldLen,myLength);
}

// updating checksum after changing old val to new
inline void IPHeader::updateCheckSum(uint16_t old_val, uint16_t new_val)
{
    myChecksum =   pkt_UpdateInetChecksum(myChecksum, old_val, new_val);
}

inline void IPHeader::updateCheckSum()
{
    myChecksum = 0;
    myChecksum = pkt_InetChecksum(getPointer(), (uint16_t)getSize());
}

inline void IPHeader::updateCheckSum2(uint8_t* data1, uint16_t len1, uint8_t* data2 , uint16_t len2)
{
    myChecksum = 0;
    myChecksum = pkt_InetChecksum(data1, len1, data2, len2);
}

inline void IPHeader::swapSrcDest()
{
	uint32_t tmp = myDestination;
	myDestination = mySource;
	mySource = tmp;
}

inline uint32_t IPPseudoHeader::getSize()
{
	return sizeof(IPPseudoHeader);
}
inline uint16_t IPPseudoHeader::inetChecksum()
{
    return(pkt_InetChecksum(getPointer(), (uint16_t)getSize()));
}



