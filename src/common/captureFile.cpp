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
#include "captureFile.h"
#include "pcap.h"
#include "erf.h"
#include "basic_utils.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>


void CPktNsecTimeStamp::Dump(FILE *fd){
    fprintf(fd,"%.6f [%x:%x]",getNsec(),m_time_sec,m_time_nsec );
}


void * CAlignMalloc::malloc(uint16_t size,uint8_t align){
    assert(m_p==0);
    m_p=(char *)::malloc(size+align);
    uintptr_t v =(uintptr_t)m_p;
    char *p=(char *)my_utl_align_up( v,align);
    return  (void *)p;
}

void CAlignMalloc::free(){
    ::free(m_p);
    m_p=0;
}

#define PKT_ALIGN 128


CCapPktRaw::CCapPktRaw(CCapPktRaw  *obj){
    flags =0;
    pkt_cnt=obj->pkt_cnt;
    pkt_len=obj->pkt_len;
    time_sec=obj->time_sec;
    time_nsec=obj->time_nsec;
    assert ((pkt_len>0) && (pkt_len<MAX_PKT_SIZE) );
    raw = (char *)m_handle.malloc(pkt_len,PKT_ALIGN);
    // copy the packet 
    memcpy(raw,obj->raw,pkt_len);
}


CCapPktRaw::CCapPktRaw(int size){
    pkt_cnt=0;
    flags =0;
    pkt_len=size;
    time_sec=0;
    time_nsec=0;
    if (size==0) {
        raw =0;
    }else{
        raw = (char *)m_handle.malloc(size,PKT_ALIGN);
        memset(raw,0xee,size);
    }

}

CCapPktRaw::CCapPktRaw(){
    flags =0;
	pkt_cnt=0;
	pkt_len=0;
	time_sec=0;
	time_nsec=0;
	raw = (char *)m_handle.malloc((uint16_t)MAX_PKT_SIZE,PKT_ALIGN);
    memset(raw,0xee,MAX_PKT_SIZE);
}

CCapPktRaw::~CCapPktRaw(){
	if (raw && (getDoNotFree()==false) ) {
		m_handle.free();
	}
}

char * CCapPktRaw::append(uint16_t len){
    CAlignMalloc h;
    char * p;
    char * new_raw = (char *)h.malloc(pkt_len+len,PKT_ALIGN);
    memcpy(new_raw,raw,pkt_len);
    m_handle.free();
    raw=new_raw;
    p= raw+pkt_len;
    pkt_len+=len;
    m_handle.m_p =h.m_p;
    /* new pointer */
    return(p);
}


void CCapPktRaw::CloneShalow(CCapPktRaw  *obj){
    pkt_len=obj->pkt_len;
    raw = obj->raw;
    setDoNotFree(true);
}

void CCapPktRaw::Dump(FILE *fd,int verbose){
	fprintf(fd," =>pkt (%p) %llu , len %d, time [%x:%x] \n",raw, (unsigned long long)pkt_cnt,pkt_len,time_sec,time_nsec);
	if (verbose) {
		utl_DumpBuffer(fd,raw,pkt_len,0);
	}
}


bool CCapPktRaw::Compare(CCapPktRaw * obj,int dump,double dsec){

    if (pkt_len != obj->pkt_len) {
        if ( dump ){
            printf(" ERROR len is not eq \n");
        }
        return (false);
    }

    if ( getInterface() != obj->getInterface() ){
        printf(" ERROR original packet from if=%d and cur packet from if=%d \n",getInterface(),obj->getInterface());
        return (false);
    }

    CPktNsecTimeStamp t1(time_sec,time_nsec);
    CPktNsecTimeStamp t2(obj->time_sec,obj->time_nsec);
    if ( t1.diff(t2) > dsec ){
        if ( dump ){
            printf(" ERROR diff of 1 msec in time  \n");
        }
        return (false);
    }

    if ( memcmp(raw,obj->raw,pkt_len) == 0 ){
        return (true);
    }else{
        if ( dump ){
            fprintf(stdout," ERROR buffer not the same \n");
            fprintf(stdout," B1 \n");
            fprintf(stdout," ---------------\n");
            utl_DumpBuffer(stdout,raw,pkt_len,0);
            fprintf(stdout," B2 \n");
            fprintf(stdout," ---------------\n");
        
            utl_DumpBuffer(stdout,obj->raw,obj->pkt_len,0);
        }
        return (false);
    }
}

#define  CPY_BUFSIZE 1024

bool CErfCmp::cpy(std::string src,std::string dst){

    char mybuf[CPY_BUFSIZE] ;
    FILE *ifd = NULL;
    FILE *ofd = NULL;
    ifd = fopen( src.c_str(), "rb" );
    ofd = fopen( dst.c_str(), "w+");
    assert(ifd!=NULL);
    assert(ofd!=NULL);

    int n;
    while ( true){
        n = fread(mybuf, sizeof(char), CPY_BUFSIZE ,ifd);
        if (n>0) {
            fwrite(mybuf, sizeof(char),n,ofd);
        }else{
            break;
        }
    }

    fclose(ifd);
    fclose(ofd);
    return true;
}


bool CErfCmp::compare(std::string f1, std::string f2 ){

    if ( dump ){
        printf(" compare %s %s \n",f1.c_str(),f2.c_str());
    }
    bool res=true;
    CCapReaderBase * lp1=CCapReaderFactory::CreateReader((char *)f1.c_str(),0);
    if (lp1 == 0) {
        if ( dump ){
            printf(" ERROR file %s does not exits or not supported \n",(char *)f1.c_str());
        }
        return (false);
    }

    CCapReaderBase * lp2=CCapReaderFactory::CreateReader((char *)f2.c_str(),0);
    if (lp2 == 0) {
        delete lp1;
        if ( dump ){
            printf(" ERROR file %s does not exits or not supported \n",(char *)f2.c_str());
        }
        return (false);
    }

    CCapPktRaw raw_packet1;
    bool has_pkt1;
    CCapPktRaw raw_packet2;
    bool has_pkt2;

    int pkt_cnt=1;
    while ( true ) {
        /* read packet */
         has_pkt1 = lp1->ReadPacket(&raw_packet1) ;
         has_pkt2 = lp2->ReadPacket(&raw_packet2) ;

         /* one has finished */
         if ( !has_pkt1  || !has_pkt2 ) {
             if (has_pkt1 != has_pkt2 ) {
                 if ( dump ){
                     printf(" ERROR not the same number of packets  \n");
                 }
                 res=false;
             }
             break;
         }
        if (!raw_packet1.Compare(&raw_packet2,true,d_sec)  ){
            res=false;
            printf(" ERROR in pkt %d \n",pkt_cnt);
            break;
        }


        pkt_cnt++;
    }

    delete lp1;
    delete lp2;
    return (res);
}

/**
 * try to create type by type
 * @param name
 * 
 * @return CCapReaderBase*
 */
CCapReaderBase * CCapReaderFactory::CreateReader(char * name, int loops, std::ostream &err)
{
    assert(name);

    /* make sure we have a file */
    FILE * f = CAP_FOPEN_64(name, "rb");
    if (f == NULL) {
        if (errno == ENOENT) {
            err << "CAP file '" << name << "' not found";
        } else {
            err << "failed to open CAP file '" << name << "' with errno " << errno;
        }
        return NULL;
    }
    // close the file
    fclose(f);

    for (capture_type_e i = LIBPCAP ; i<LAST_TYPE ; i = (capture_type_e(i+1)) )
	{
		CCapReaderBase * next = CCapReaderFactory::CreateReaderInstace(i);
		if (next == NULL || next->Create(name,loops)) {
			return next;
		}
		delete next;
	}

    err << "unsupported CAP format (not PCAP or ERF): " << name << "\n";

	return NULL;
}

CCapReaderBase * CCapReaderFactory::CreateReaderInstace(capture_type_e type)
{
	switch(type)
	{
	case  ERF:
		return new CErfFileReader();
	case LIBPCAP:
		return new LibPCapReader();
	default:
		printf("Got unsupported file type\n");
		return NULL;
	}

}


	
/**
 * The factory function will create the matching reader instance
 * according to the type.
 * 
 * @param type - the foramt
 * @param name - new file name
 * 
 * @return CCapWriter* - return pointer to the writer instance
 *         or NULL if failed from some reason. Instance user
 *         should relase memory when instance not needed
 *         anymore.
 */
CFileWriterBase  * CCapWriterFactory::CreateWriter(capture_type_e type ,char * name)
{
	if (name == NULL) {
		return NULL;
	}

	CFileWriterBase  * toRet = CCapWriterFactory::createWriterInsance(type);
		
	if (toRet) {
		if (!toRet->Create(name)) {
            delete toRet;
			toRet = NULL;
		}
	}

	return toRet;
}

/**
 * Create instance for writer if type is supported.
 * @param type
 * 
 * @return CFileWriterBase*
 */
CFileWriterBase  * CCapWriterFactory::createWriterInsance(capture_type_e type )
{
	switch(type) {
	case LIBPCAP:
		return new LibPCapWriter();
	case ERF:
		return new CErfFileWriter();
		// other is not supported yet.
	default:
		return NULL;
	}
}

