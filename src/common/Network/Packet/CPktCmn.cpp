#include "CPktCmn.h"
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



#define TouchCacheLine(a)

uint16_t pkt_InetChecksum(uint8_t* data , 
                        uint16_t len, uint8_t* data2 , uint16_t len2){

    TouchCacheLine(data2);
    TouchCacheLine(data2+32);
    TouchCacheLine(data2+64);

    int sum = 0;
    while(len>1){
        TouchCacheLine(data+96); //three lines ahead !
        sum += PKT_NTOHS(*((uint16_t*)data));
        data += 2;
        len -= 2;
    }

    while(len2>1){
        TouchCacheLine(data2+96); //three lines ahead !
        sum += PKT_NTOHS(*((uint16_t*)data2));
        data2 += 2;
        len2 -= 2;
    }

    if(len2){
        sum += (PKT_NTOHS(*((uint16_t*)data2)) & 0xff00);
    }

    while(sum >> 16){
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return PKT_NTOHS((uint16_t)(~sum));
}

uint16_t pkt_InetChecksum(uint8_t* data , uint16_t len){

    int sum = 0;
    while(len>1){
        TouchCacheLine(data+96); //three line ahead !
        sum += PKT_NTOHS(*((uint16_t*)data));
        data += 2;
        len -= 2;
    }

    if(len){
        sum += (PKT_NTOHS(*((uint16_t*)data)) & 0xff00);
    }

    while(sum >> 16){
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return PKT_NTOHS((uint16_t)(~sum));
}

uint16_t pkt_UpdateInetChecksum(uint16_t csFieldFromPacket, uint16_t oldVal, uint16_t newVal){
    uint32_t newCS;
    newCS = (uint16_t)(~PKT_NTOHS(csFieldFromPacket));
    newCS += (uint16_t)(~PKT_NTOHS(oldVal));
    newCS += (uint16_t)PKT_NTOHS(newVal);
    while(newCS >> 16){
        newCS = (newCS & 0xffff) + (newCS >> 16);
    }
    return PKT_NTOHS((uint16_t)(~newCS));
}

uint16_t pkt_SubtractInetChecksum(uint16_t checksum, uint16_t csToSubtract){
    uint32_t newCS;
    newCS = (uint16_t)(~PKT_NTOHS(checksum));

    // since the cs is already in ~ format in the packet, there is no need
    // to negate it for subtraction in 1's complement.
    newCS += (uint16_t)PKT_NTOHS(csToSubtract);

    while(newCS >> 16){
        newCS = (newCS & 0xffff) + (newCS >> 16);
    }
    return PKT_NTOHS((uint16_t)(~newCS));
}

uint16_t pkt_AddInetChecksum(uint16_t checksum, uint16_t csToAdd){
    uint32_t newCS;
    newCS = (uint16_t)(~PKT_NTOHS(checksum));

    // since the cs is already in ~ format in the packet, there is a need
    // to negate it for addition in 1's complement.
    newCS += (uint16_t)PKT_NTOHS(~csToAdd);

    while(newCS >> 16){
        newCS = (newCS & 0xffff) + (newCS >> 16);
    }
    return PKT_NTOHS((uint16_t)(~newCS));
}


extern "C" void pkt_ChecksumTest(){

    uint16_t cs;
    uint8_t data[5] = {0xcd,0x7a,0x55,0x55,0xa1};

    cs = pkt_InetChecksum((uint8_t*)data,5);
    printf("CS = 0x%04x: ",cs);
    if(cs == 0x2F3C){
        printf("CS func with odd len OK.\n");
    }else{
        printf("ERROR: CS func produced wrong value with odd number of bytes.\n");
    }
    
    cs = pkt_InetChecksum((uint8_t*)data,4);
    printf("CS = 0x%04x: ",cs);
    if(cs == 0x2FDD){
        printf("CS func with even len OK.\n");
    }else{
        printf("ERROR: CS func produced wrong value with even number of bytes.\n");
    }

    cs = pkt_UpdateInetChecksum(cs,0x5555,0x8532);
    printf("CS = 0x%04x: ",cs);
    if(cs == 0x0000){
        printf("Update CS func OK.\n");
    }else{
        printf("ERROR: Update CS func produced wrong value.\n");
    }   

}
