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

inline  void   VLANHeader::setVlanTag(uint16_t data)
{
    myTag = PKT_HTONS(data);
}

inline  uint16_t VLANHeader::getVlanTag()
{
    return(PKT_NTOHS(myTag));
}

inline  void  VLANHeader::setTagUserPriorty(uint8_t   argUserPriority)
{
    uint16_t  tempTag = PKT_NTOHS(myTag);
    setMaskBit16(tempTag, 0, 2, argUserPriority);
    myTag = PKT_HTONS(tempTag);
}

inline  uint8_t VLANHeader::getTagUserPriorty()
{
    return (uint8_t)(getMaskBit16(PKT_NTOHS(myTag), 0, 2));
}


inline  void  VLANHeader::setTagCFI(bool    isSet)
{
    uint16_t  tempTag = PKT_NTOHS(myTag);
    setMaskBit16(tempTag, 3, 3, isSet? 1 : 0);
    myTag = PKT_HTONS(tempTag);
}

inline  bool  VLANHeader::getTagCFI()
{
    return (getMaskBit16(PKT_NTOHS(myTag), 3, 3) == 1);
}

// This returns host order
inline  uint16_t VLANHeader::getTagID(void)
{
    return  getMaskBit16(PKT_NTOHS(myTag), 4, 15);
}

inline  void    VLANHeader::setTagID(uint16_t argNewTag)
{
    uint16_t  tempTag = PKT_NTOHS(myTag);
    setMaskBit16(tempTag, 4, 15, argNewTag);
    myTag = PKT_HTONS(tempTag);
}

inline void  VLANHeader::incrementTagID(uint16_t inc_value)
{
    uint16_t  tempTag_Host   = PKT_NTOHS(myTag);
    uint16_t  curTagID_Host  = getTagID();
    uint16_t  newTagId_Host  = (0xfff & (curTagID_Host + (0xfff & inc_value))); // addition with 12 LSBits
    setMaskBit16(tempTag_Host, 4, 15, newTagId_Host);
    myTag = PKT_HTONS(tempTag_Host);
}

inline  uint16_t VLANHeader::getNextProtocolNetOrder()
{
    return myNextProtocol;
}

inline  uint16_t VLANHeader::getNextProtocolHostOrder()
{
    return (PKT_NTOHS(myNextProtocol));
}

inline  void    VLANHeader::setNextProtocolFromHostOrder(uint16_t  argNextProtocol)
{
    myNextProtocol = PKT_HTONS(argNextProtocol);
}

inline  void    VLANHeader::setNextProtocolFromNetOrder(uint16_t  argNextProtocol)
{
    myNextProtocol = argNextProtocol;
}

inline void     VLANHeader::setFromPkt          (uint8_t* data)
{
    // set the tag from the data
    myTag = *(uint16_t*)data;
    setNextProtocolFromNetOrder(*((uint16_t*)(data + 2))); // next protocol is after the vlan tag
}

inline uint8_t    VLANHeader::reconstructPkt      (uint8_t* destBuff)
{
    *(uint16_t*)destBuff     = myTag;
    *(uint16_t*)(destBuff+2) = getNextProtocolNetOrder();
    return sizeof(VLANHeader);
}



