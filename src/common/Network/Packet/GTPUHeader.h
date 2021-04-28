#ifndef _GTPU_H_
#define _GTPU_H_

#include <common/BigEndianBitManipulation.h>

/**
 *      Bits
 * Octets   8   7   6   5   4   3   2   1
 * 1                  Version   PT  (*) E   S   PN
 * 2        Message Type
 * 3        Length (1st Octet)
 * 4        Length (2nd Octet)
 * 5        Tunnel Endpoint Identifier (1st Octet)
 * 6        Tunnel Endpoint Identifier (2nd Octet)
 * 7        Tunnel `Endpoint Identifier (3rd Octet)
 * 8        Tunnel Endpoint Identifier (4th Octet)
 * 9        Sequence Number (1st Octet)1) 4)
 * 10       Sequence Number (2nd Octet)1) 4)
 * 11       N-PDU Number2) 4)
 * 12       Next Extension Header Type3) 4)
**/

#define GTPU_V1_HDR_LEN   8

#define GTPU_VER_MASK (7<<5)
#define GTPU_PT_BIT   (1<<4)
#define GTPU_E_BIT    (1<<2)
#define GTPU_S_BIT    (1<<1)
#define GTPU_PN_BIT   (1<<0)
#define GTPU_E_S_PN_BIT  (7<<0)

#define GTPU_V1_VER   (1<<5)

#define GTPU_PT_GTP    (1<<4)
#define GTPU_TYPE_GTPU  255


class GTPUHeader {
    uint8_t ver_flags;
    uint8_t type;
    uint16_t length;
    uint32_t teid;

public:

    uint8_t getVerFlags(){
        return this->ver_flags;
    }


    void setVerFlags(uint8_t flags){
        this->ver_flags = flags;
    }


    uint8_t getType(){
        return this->type;
    }


    void setType(uint8_t type){
        this->type = type;
    }


    uint16_t getLength(){
        return PKT_NTOHS(this->length);
    }


    void setLength(uint16_t length){
        this->length = PKT_NTOHS(length);
    }


    uint32_t getTeid(){
        return PKT_HTONL(this->teid);
    }


    void setTeid(uint32_t teid){
        this->teid = PKT_HTONL(teid);
    }
};

#endif