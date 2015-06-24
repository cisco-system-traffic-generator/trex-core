/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

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
#ifndef PAL_UTL_H
#define PAL_UTL_H

#include <stdint.h>

#define PAL_MSB(x) (((x) >> 8) & 0xff)   /* most signif byte of 2-byte integer */
#define PAL_LSB(x) ((x) & 0xff)          /* least signif byte of 2-byte integer*/
#define PAL_MSW(x) (((x) >> 16) & 0xffff) /* most signif word of 2-word integer */
#define PAL_LSW(x) ((x) & 0xffff)        /* least signif byte of 2-word integer*/

/* swap the MSW with the LSW of a 32 bit integer */
#define PAL_WORDSWAP(x) (PAL_MSW(x) | (PAL_LSW(x) << 16))

#define PAL_LLSB(x)        ((x) & 0xff)        /* 32bit word byte/word swap macros */
#define PAL_LNLSB(x)       (((x) >> 8) & 0xff)
#define PAL_LNMSB(x)       (((x) >> 16) & 0xff)
#define PAL_LMSB(x)        (((x) >> 24) & 0xff)
#define PAL_LONGSWAP(x)    ((PAL_LLSB(x) << 24) | \
                        (PAL_LNLSB(x) << 16)|  \
                        (PAL_LNMSB(x) << 8) |  \
                        (PAL_LMSB(x)))

#define PAL_NTOHL(x)     ((uint32_t)(PAL_LONGSWAP((uint32_t)x)))
#define PAL_NTOHS(x)     ((uint16_t)(((PAL_MSB((uint16_t)x))) | (PAL_LSB((uint16_t)x) << 8)))
 
#define PAL_HTONS(x)     (PAL_NTOHS(x))
#define PAL_HTONL(x)     (PAL_NTOHL(x))

static inline uint64_t pal_ntohl64(uint64_t x){
    uint32_t high=(uint32_t)((x&0xffffffff00000000ULL)>>32);
    uint32_t low=(uint32_t)((x&0xffffffff));
    uint64_t res=((uint64_t)(PAL_LONGSWAP(low)))<<32 | PAL_LONGSWAP(high);
    return (res);
}

#define PAL_NTOHLL(x)     (pal_ntohl64(x))


#define unlikely(a) (a)
#define likely(a)  (a)

static inline void rte_pause (void)
{
}



#endif


