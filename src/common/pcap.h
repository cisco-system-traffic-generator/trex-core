#ifndef __TREX_LIBPCAP_H__
#define __TREX_LIBPCAP_H__ 

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
#include <stdio.h>

typedef struct pcaptime {
   uint32_t sec;
   uint32_t msec;
} pcaptime_t;

typedef struct packet_file_header 
{
	uint32_t magic;             
	uint16_t version_major;   
	uint16_t version_minor;   
	uint32_t thiszone;          
	uint32_t sigfigs;           
	uint32_t snaplen;           
	uint32_t linktype;          
}packet_file_header_t ;

typedef struct sf_pkthdr {
	pcaptime_t  ts;         
	uint32_t            caplen;     
	uint32_t            len;        
} sf_pkthdr_t;

/**
 * Implements the CCAPReaderBase interface.
 *
 */
class LibPCapReader : public CCapReaderBase
{
public:
    LibPCapReader();

    virtual ~LibPCapReader();

	/**
     * open file for reading.
     * (can be called once).
	 * @param name
	 * 
	 * @return bool
	 */
    bool Create(char * name, int loops = 0);    

	/**
     * When called after open will return true only if
     * capture file is libpcap format.
	 * 
	 * @return bool
	 */
    bool isValid() { return m_is_valid; }

	/**
     * Fill the structure with the new packet.
	 * @param lpPacket
	 * 
     * @return bool - return true if packet were read and false
     *         otherwise (reached eof)
	 */
    virtual bool ReadPacket(CCapPktRaw *lpPacket); 
    virtual void Rewind();


    virtual capture_type_e get_type() {
        return LIBPCAP;
    }

private:
	LibPCapReader(LibPCapReader &);
	

	bool init();
    void flip(sf_pkthdr_t * tofilp);
	bool m_is_open;
	uint64_t m_last_time;
	bool m_is_valid;
    FILE * m_file_handler;
    bool m_is_flip;

};

/**
 * Libpcap file format writer.
 * Implements CFileWrirerBase interface
 */
class LibPCapWriter: public CFileWriterBase 
{

public:

	LibPCapWriter();
	virtual ~LibPCapWriter();

	/**
     * Open file for writing. Rewrite from scratch (no append).
     * @param name - the file name
	 * 
     * @return bool - return true if File was open successfully.
	 */
	bool Create(char * name);

	/**
     * Write packet to file (must be called only after successfull
     * Create).
     * 
     * @param p  - buffer pointer
     * @param size - buffer length
     * 
     * @return true on success.
	 */
	virtual bool write_packet(CCapPktRaw * lpPacket);
    /**
     * 
     * returns the count of packets so far written
     * 
     * @return uint32_t 
     */
    uint32_t get_pkt_count();

    /**
     * Close file and flush all.
	*/
	void Close();

    /**
     * flush all packets to disk
     * 
     * @author imarom (11/24/2016)
     */
    void flush_to_disk();
    
private:

	bool init();
	FILE * m_file_handler;
	uint64_t m_timestamp;
    bool m_is_open;
    uint32_t m_pkt_count;
};

#endif
