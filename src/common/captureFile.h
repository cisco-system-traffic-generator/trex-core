#ifndef __CAPTURE_FILE_H__
#define __CAPTURE_FILE_H__
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



#include "c_common.h"
#include <stdio.h>
#include "bitMan.h"
#include <math.h>
#include <stdlib.h>
#include <string>
#include <iostream>

#ifdef WIN32
#pragma warning(disable:4786)
#endif


typedef enum capture_type {
	LIBPCAP,
	ERF,
	LAST_TYPE
} capture_type_e;

#define MAX_PKT_SIZE (9*1024+22) /* 9k IP +14+4 FCS +some spare */

#define READER_MAX_PACKET_SIZE MAX_PKT_SIZE

class CAlignMalloc {
public:
    CAlignMalloc(){
        m_p=0;
    }
    void * malloc(uint16_t size,uint8_t align);
    void free();
public:
    char * m_p;
};

static inline uintptr_t my_utl_align_up(uintptr_t num,uint16_t round){
    if ((num & ((round-1)) )==0) {
        //the number align
        return(num);
    }
    return( (num+round) & (~(round-1)) );
}




class CPktNsecTimeStamp {
public:
    
    #define _NSEC_TO_SEC 1000000000.0
    CPktNsecTimeStamp(){
        m_time_sec  =0;
        m_time_nsec =0;
    }

    CPktNsecTimeStamp(uint32_t sec,uint32_t nsec){
        m_time_sec =sec;
        m_time_nsec =nsec;
    }
    CPktNsecTimeStamp(double nsec){
        m_time_sec = (uint32_t)floor (nsec);
        nsec -= m_time_sec;
        m_time_nsec = (uint32_t)floor(nsec*_NSEC_TO_SEC);
    }

    double getNsec() const {
         return ((double)m_time_sec +(double)m_time_nsec/(_NSEC_TO_SEC));
    }

    double diff(const CPktNsecTimeStamp & obj){
        return (abs(getNsec() - obj.getNsec() ) );
    }

    void Dump(FILE *fd);
public:
    uint32_t     m_time_sec;
    uint32_t     m_time_nsec;
};



class CCapPktRaw {

public:
	CCapPktRaw();
    CCapPktRaw(int size);
    CCapPktRaw(CCapPktRaw  *obj);
	virtual ~CCapPktRaw();

    uint32_t     time_sec;
    uint32_t     time_nsec;
    char       * raw;
    uint64_t	 pkt_cnt;

    uint16_t     pkt_len;
private:
    uint16_t     flags;
    CAlignMalloc  m_handle;
public:
    double get_time(void) {
        CPktNsecTimeStamp t1(time_sec,time_nsec);
        return ( t1.getNsec());
    }
    void set_new_time(double new_time){
        CPktNsecTimeStamp t1(new_time);
        time_sec =t1.m_time_sec;
        time_nsec=t1.m_time_nsec;
    }

    /* enlarge the packet */
    char * append(uint16_t len);

    void CloneShalow(CCapPktRaw  *obj);
    
    void setInterface(uint8_t _if){
        btSetMaskBit16(flags,10,8,_if);
    }
    
    uint8_t getInterface(){
        return ((uint8_t)btGetMaskBit16(flags,10,8));
    }
    
    void setDoNotFree(bool do_not_free){
        btSetMaskBit16(flags,0,0,do_not_free?1:0);
    }

    bool getDoNotFree(){
        return ( ( btGetMaskBit16(flags,0,0) ? true:false) );
    }

    bool Compare(CCapPktRaw * obj,int dump,double dsec);

    
public:
	inline uint16_t getTotalLen(void) {
		return (pkt_len);
	}
	void Dump(FILE *fd,int verbose);
};
 
/**
 * Interface for capture file reader.
 * 
 */
class CCapReaderBase
{
public:

    virtual ~CCapReaderBase(){}

    virtual bool ReadPacket(CCapPktRaw * lpPacket)=0;

    
    /* by default all reader reads one packet
       and gives the feature one packet
    */
    virtual uint32_t  get_last_pkt_count() {return 1;}

    /* method for rewind the reader
      abstract and optional 
    */
    virtual void Rewind() {};
	/**
     * open file for reading.
     */
	virtual bool Create(char * name, int loops = 0) = 0;

    virtual capture_type_e get_type() = 0;

protected:
    int 		m_loops;
    uint64_t 	m_file_size;
};

/**
 * Factory for creating reader inteface of some of the supported
 * formats.
 *
 */
class CCapReaderFactory {
public:
	/**
     * The function will try to create the matching reader for the
     * file format (libpcap,ngsniffer...etc). Since there is no real
     * connection (stile repository) between file suffix and its
     * type we just try one bye one,
     * @param name - cature file name
     * @param loops - number of loops for the same capture. use 0
     *                for one time transmition
     * @param err - IO stream to print error
     *  
     * @return CCapReaderBase* - pointer to new instance (allocated
     *         by the function). the user should release the
     *         instance once it has no use any more.
	 */
	static CCapReaderBase * CreateReader(char * name, int loops = 0, std::ostream &err = std::cout);


private:
	static CCapReaderBase * CreateReaderInstace(capture_type_e type);
};

/**
 * Interface for capture file writer.
 *
 */
class CFileWriterBase {

public:

	virtual ~CFileWriterBase(){};
	virtual bool Create(char * name) = 0;
    virtual bool write_packet(CCapPktRaw * lpPacket)=0;

};


/**
 * Factory for creating capture file interface of some of the
 * supported formats.
 *
 */
class CCapWriterFactory {
public:
	
	/**
     * The factory function will create the matching reader instance
     * according to the type.
     * 
     * @param type - the foramt
     * @param name - new file name
	 * 
     * @return CCapWriter* - return pointer to the writer instance
     *         or NULL if failed from some reason (or unsupported
     *         format).Instance user
     *         should relase memory when instance not needed
     *         anymore.
	 */
	static CFileWriterBase * CreateWriter(capture_type_e type ,char * name);

private:

    static CFileWriterBase * createWriterInsance(capture_type_e type );
};


#if WIN32

#define CAP_FOPEN_64 fopen
#define CAP_FSEEK_64 fseek
#define CAP_FTELL_64 ftell

#else

#define CAP_FOPEN_64 fopen64
#define CAP_FSEEK_64 fseeko64
#define CAP_FTELL_64 ftello64

#endif


class CErfCmp
 {
public:
    CErfCmp(){
        dump=false;
        d_sec=0.001;
    }
    bool compare(std::string f1, std::string f2 );

    bool cpy(std::string src,std::string dst);
public:
    bool dump;
    double d_sec;
};



#endif
