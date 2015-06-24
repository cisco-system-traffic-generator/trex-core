#ifndef BIG_ENDIAN_BIT_MAN_H
#define BIG_ENDIAN_BIT_MAN_H
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

#include "bitMan.h"

inline void setMaskBit8(uint8_t & a, 
                        int           startbit,
                        int           stopbit,
                        uint8_t newVal )
{
    btSetMaskBitBigE8(a, startbit, stopbit, newVal);
}


inline void setMaskBit16(uint16_t & a, 
                         int           startbit,
                         int           stopbit,
                         uint16_t newVal )
{
    btSetMaskBitBigE16(a, startbit, stopbit, newVal);
}


inline void setMaskBit32(uint32_t   & a, 
                         int           startbit,
                         int           stopbit,
                         uint32_t newVal )
{
    btSetMaskBitBigE32(a, startbit, stopbit, newVal);
}




inline unsigned char getMaskBit8(uint8_t   a, 
                                 int             startbit,
                                 int             stopbit) {
     return btGetMaskBitBigE8(a,startbit,stopbit);
}


inline unsigned short getMaskBit16(uint16_t a, 
                                  int            startbit,
                                  int            stopbit) {
     return btGetMaskBitBigE16(a,startbit,stopbit);
}


inline unsigned long   getMaskBit32(uint32_t   a, 
                                 int             startbit,
                                 int             stopbit) {
     return btGetMaskBitBigE32(a,startbit,stopbit);
}


#endif // BIG_ENDIAN_BIT_MAN_H


