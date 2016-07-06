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
#ifndef _TCP_HEADER_H_
#define _TCP_HEADER_H_

#include "PacketHeaderBase.h"
#define TCP_HEADER_LEN 20
                 
class TCPHeader
{

public:
    enum TCPHeader_enum_t
    {
        TCP_INVALID_PORT = 0
    };

    TCPHeader(){}

    TCPHeader(uint16_t    argSourcePort,
              uint16_t    argDestinationPort,
              uint8_t     argFlags,
              uint32_t    argSeqNum,
              uint32_t    argAckNum);

    struct Flag
    {
        enum Type
        {
            FIN =          0x01,
            SYN =          0x02,
            RST =          0x04,
            PSH =          0x08,
            ACK =          0x10,
            URG =          0x20
        };
    };

    void    setSourcePort   (uint16_t);
    uint16_t  getSourcePort   ();

    void    setDestPort     (uint16_t);
    uint16_t  getDestPort     ();

    void    setSeqNumber    (uint32_t);
    uint32_t  getSeqNumber    ();

    void    setAckNumber    (uint32_t);
    uint32_t  getAckNumber    ();

    //this is factor 4
    void    setHeaderLength (uint8_t);
    uint8_t   getHeaderLength ();

    void    setFlag         (uint8_t);
    uint8_t   getFlags        ();

    void    setFinFlag      (bool);
    bool    getFinFlag      ();

    void    setSynFlag      (bool);
    bool    getSynFlag      ();

    void    setResetFlag    (bool);
    bool    getResetFlag    ();

    void    setPushFlag     (bool);
    bool    getPushFlag     ();

    void    setAckFlag      (bool);
    bool    getAckFlag      ();

    void    setUrgentFlag   (bool);
    bool    getUrgentFlag   ();

    void    setWindowSize   (uint16_t);
    uint16_t  getWindowSize   ();

    void    setChecksum     (uint16_t);
    uint16_t  getChecksum     ();

    void    setUrgentOffset (uint16_t);
    uint16_t  getUrgentOffset ();

    uint32_t* getOptionPtr    ();


////////////////////////////////////////////////////////////////////////////////////////
// Common Header Interface
////////////////////////////////////////////////////////////////////////////////////////

public: 
    uint8_t*  getPointer      (){return (uint8_t*)this;}
    uint32_t  getSize         (){return  getHeaderLength();}

    uint16_t  getNextProtocol ();
    void    setNextProtocol (uint16_t);

    void    dump            (FILE*  fd);

private:
    uint16_t mySourcePort;
    uint16_t myDestinationPort;
    uint32_t mySeqNum;
    uint32_t myAckNum;
    uint8_t  myHeaderLength;
    uint8_t  myFlags;
    uint16_t myWindowSize;
    uint16_t myChecksum;
    uint16_t myUrgentPtr;
    uint32_t myOption[1];
};

#include "TcpHeader.inl"

#endif //_TCP_HEADER_H_
