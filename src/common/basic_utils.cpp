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
#include "basic_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <sys/resource.h>

#include "pal_utl.h"

int my_inet_pton4(const char *src, unsigned char *dst);

bool utl_is_file_exists (const std::string& name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }   
}

void utl_k12_pkt_format(FILE* fp,void  * src,  unsigned int size) {
    unsigned int i;
    fprintf(fp,"\n");
    fprintf(fp,"+---------+---------------+----------+\n");
    fprintf(fp,"00:00:00,000,000   ETHER \n");
    fprintf(fp,"|0   |");
    for ( i=0; i<size;i++ ) {
        fprintf(fp,"%02x|",((unsigned char *)src)[i]);
    }
    fprintf(fp,"\n");;
}


void utl_DumpBuffer(FILE* fp,void  * src,  unsigned int size,int offset) {
    unsigned int i;
    for ( i=0; i<size;i++ ) {
        if ( (!(i%16)) && (i!=0) ) fprintf(fp,"]\n");
        if ( !(i%4) && !(i%16) ) fprintf(fp,"[");
        if ( !(i%4) &&  (i%16) ) fprintf(fp,"] [");
        fprintf(fp,"%02x",((unsigned char *)src)[i + offset]);
        if ( (i+1)%4 ) fprintf(fp," ");
    }
    for ( ;i%4;i++ )
        fprintf(fp,"  ");
    fprintf(fp,"]");

    fprintf(fp,"\n");;
}


void utl_DumpChar(FILE* fd, 
                  void  * src,  
                  unsigned int eln, 
                  unsigned int width 
                  ){
    int size=eln*width;
    unsigned char * p=(unsigned char *)src;
    int i;
    fprintf(fd," - ");
    for (i=0; i<size;i++) {
        if ( isprint(*p) ) {
          fprintf(fd,"%c",*p);
        }else{
            fprintf(fd,"%c",'.');
        }
        p++;
    }
}

void utl_DumpBufferLine(FILE* fd, 
                        void  * src,  
                        int     offset,
                        unsigned int eln, 
                        unsigned int width ,
                        unsigned int mask){
    unsigned char * p=(unsigned char *)src;
    uint32 addr;
    
    if ( mask & SHOW_BUFFER_ADDR_EN){
        addr=offset;
        fprintf(fd,"%08x: ",(int)addr);
    }
    int i;
    for (i=0; i<(int)eln; i++) {
        switch (width) {
            case 1:
                fprintf(fd,"%02x ",*p);
                p++;
                break;
            case 2:
                fprintf(fd,"%04x ",*((uint16 *)p));
                p+=2;
                break;
            case 4:
                fprintf(fd,"%08x ",*((int *)p));
                p+=4;
                break;
            case 8:
                fprintf(fd,"%08x",*((int *)p));
                fprintf(fd,"%08x ",*((int *)(p+4)));
                p+=8;
                break;
        }
    }
    if (mask & SHOW_BUFFER_CHAR) {
       utl_DumpChar(fd, src,eln,width);
    }
    fprintf(fd,"\n");
}

void utl_DumpBuffer2(FILE* fd,
                     void  * src,  
                     unsigned int size, //buffer size
                     unsigned int width ,
                     unsigned int width_line ,
                     unsigned int mask
                     ) {
    if (!( (width==1) || (width==2) || (width==4) || (width==8) )){
        width=1;
    }

    int nlen=(size)/(width_line );
    if ( ( (size % width_line))!=0 ) {
        nlen++;
    }
    int i;
    char *p=(char *)src;
    int offset=0;

    if (mask & SHOW_BUFFER_ADDR_EN){
        if (mask & SHOW_BUFFER_ADDR) {
            offset=(int)((uintptr_t)p);
        }else{
            offset=0;
        }
    }
    unsigned int eln_w;  
    int len_exist=size;
    
    for (i=0; i<nlen; i++) {
      if ( len_exist > (int)(width_line /width) ){  
        eln_w=width_line /width;
      }else{
        eln_w=(len_exist+width-1)/width;
      }
      utl_DumpBufferLine(fd, p,offset,eln_w,width,mask);
      p+=width_line;
      offset+=width_line;
      len_exist-=  width_line;
    }
}                              


void TestDump(void){

    char buffer[100];
    int i;
    for (i=0;i<100;i++) {
        buffer[i]=0x61+i;
    }


    utl_DumpBuffer2(stdout,buffer,31,1,4,SHOW_BUFFER_ADDR_EN |SHOW_BUFFER_CHAR);
}

std::string
utl_macaddr_to_str(const uint8_t *mac) {
    std::string tmp;
    utl_macaddr_to_str(mac, tmp);
    return tmp;
}

void utl_macaddr_to_str(const uint8_t *macaddr, std::string &output) {
    
    for (int i = 0; i < 6; i++) {
        char formatted[4];

        if (i == 0) {
            snprintf(formatted, sizeof(formatted), "%02x", macaddr[i]);
        } else {
            snprintf(formatted, sizeof(formatted), ":%02x", macaddr[i]);
        }

        output += formatted;
    }

}

std::string utl_macaddr_to_str(const uint8_t *macaddr) {
    std::string tmp;
    utl_macaddr_to_str(macaddr, tmp);
    
    return tmp;
}

bool utl_str_to_macaddr(const std::string &s, uint8_t *mac) {
    int last = -1;
    int rc = sscanf(s.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx%n",
                    mac + 0, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5,
                    &last);

    if ( (rc != 6) || (s.size() != last) ) {
        return false;
    }
    
    return true;
}

/**
 * generate a random connection handler
 * 
 */
std::string 
utl_generate_random_str(unsigned int &seed, int len) {
    std::stringstream ss;

    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    /* generate 8 bytes of random handler */
    for (int i = 0; i < len; ++i) {
        ss << alphanum[rand_r(&seed) % (sizeof(alphanum) - 1)];
    }

    return (ss.str());
}

/**
 * define the coredump size 
 * allowed when crashing 
 * 
 * @param size - -1 means unlimited
 * @param map_huge_pages - should the core map the huge TLB 
 *                       pages
 */
void utl_set_coredump_size(long size, bool map_huge_pages) {
    int mask;
    struct rlimit core_limits;
    
    if (size == -1) {
        core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
    } else {
        core_limits.rlim_cur = core_limits.rlim_max = size;
    }

    setrlimit(RLIMIT_CORE, &core_limits);

    /* set core dump mask */
    FILE *fp = fopen("/proc/self/coredump_filter", "wb");
    if (!fp) {
        printf("failed to open /proc/self/coredump_filter\n");
        exit(-1);
    }

    /* huge pages is the 5th bit */
    if (map_huge_pages) {
        mask = 0x33;
    } else {
        mask = 0x13;
    }
    
    fprintf(fp, "%08x\n", mask);
    fclose(fp);
}

uint32_t utl_ipv4_to_uint32(const char *ipv4_str, uint32_t &ipv4_num) {
    
    uint32_t tmp;
    
    int rc = my_inet_pton4(ipv4_str, (unsigned char *)&tmp);
    if (!rc) {
        return (0);
    }
    
    ipv4_num = PAL_NTOHL(tmp);
    
    return (1);
}
   
std::string utl_uint32_to_ipv4(uint32_t ipv4_addr) {
    std::stringstream ss;
    ss << ((ipv4_addr >> 24) & 0xff) << "." << ((ipv4_addr >> 16) & 0xff) << "." << ((ipv4_addr >> 8) & 0xff) << "." << (ipv4_addr & 0xff);
    return ss.str();
}
