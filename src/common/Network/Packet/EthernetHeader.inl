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

inline EthernetHeader::EthernetHeader(uint8_t* packet)
{
	*this = *((EthernetHeader*)packet);
}
inline void   EthernetHeader::setNextProtocol(uint16_t argProtocol)
{
    myProtocol = PKT_HTONS(argProtocol);
}

inline uint16_t EthernetHeader::getNextProtocol()
{
    return( PKT_HTONS(myProtocol));
}

inline uint16_t EthernetHeader::getVlanProtocol()
{
    return( PKT_HTONS(myVlanProtocol));
}

inline uint16_t EthernetHeader::getVlanTag()
{
    return( PKT_HTONS(myVlanTag));
}

inline uint16_t EthernetHeader::getQinQProtocol()
{
    return( PKT_HTONS(myQinQProtocol));
}

inline uint16_t EthernetHeader::getQinQTag()
{
    return( PKT_HTONS(myQinQTag));
}
