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

inline GREHeader::GREHeader(uint8_t* packet)
{
    *this = *((GREHeader*)packet);
}

inline bool GREHeader::isChecksumEnabled()
{
    return getMaskBit16(PKT_NTOHS(_flagsVersion), 0, 0);
}
inline bool GREHeader::isKeyEnabled()
{
    return getMaskBit16(PKT_NTOHS(_flagsVersion), 2, 2);
}
inline bool GREHeader::isSequenceNumberEnabled()
{
    return getMaskBit16(PKT_NTOHS(_flagsVersion), 3, 3);
}
inline void GREHeader::setFlags(bool checksumEnabled, bool keyEnabled,
    bool sequenceNumberEnable)
{
    uint16_t flagsVersionWord = (PKT_NTOHS(_flagsVersion));
    setMaskBit16(flagsVersionWord, 0, 0, checksumEnabled ? 1 : 0);
    setMaskBit16(flagsVersionWord, 2, 2, keyEnabled ? 1 : 0);
    setMaskBit16(flagsVersionWord, 3, 3, sequenceNumberEnable ? 1 : 0);
    _flagsVersion = (PKT_HTONS(flagsVersionWord));
}

inline uint8_t GREHeader::getVersion()
{
    return getMaskBit16(PKT_NTOHS(_flagsVersion), 13, 15);
}

inline void GREHeader::setVersion(uint8_t version)
{
    uint16_t flagsVersionWord = (PKT_NTOHS(_flagsVersion));
    setMaskBit16(flagsVersionWord, 13, 15, version);
    _flagsVersion = (PKT_HTONS(flagsVersionWord));
}

inline uint16_t GREHeader::getProtocolType()
{
    return (PKT_NTOHS(_protocolType));
}
inline void GREHeader::setProtocolType(uint16_t protocolType)
{
    _protocolType = (PKT_HTONS(protocolType));
}

inline uint32_t GREHeader::getKey()
{
    return (PKT_NTOHL(_key));
}
inline void GREHeader::setKey(uint32_t key)
{
    _key = (PKT_HTONL(key));
}
