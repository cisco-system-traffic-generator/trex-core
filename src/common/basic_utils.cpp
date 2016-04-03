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

bool utl_is_file_exists (const std::string& name) {
    if (FILE *file = fopen(name.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }   
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

