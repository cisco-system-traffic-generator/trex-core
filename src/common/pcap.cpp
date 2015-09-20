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

#include "pcap.h"
#include <errno.h>
#include <string.h>
#include "pal_utl.h"




static uint32_t MAGIC_NUM_FLIP = 0xd4c3b2a1;
static uint32_t MAGIC_NUM_DONT_FLIP = 0xa1b2c3d4;


LibPCapReader::LibPCapReader()
{
	m_is_open = false;
	m_last_time = 0;
	m_is_valid = false;
    m_file_handler = NULL;
	m_is_flip = false;
}

LibPCapReader::~LibPCapReader()
{
	if (m_is_open && m_file_handler) {
        fclose(m_file_handler);
	}
}

void LibPCapReader::Rewind() {
   if (m_is_open && m_file_handler) {
       rewind(m_file_handler);
       this->init();
   }
}

/**
 * open file for read.
 * @param name
 * 
 * @return bool
 */
bool LibPCapReader::Create(char * name, int loops)
{
    this->m_loops = loops;

    if(name == NULL) {
	return false;
    }

    if (m_is_open) {
        return true;
    }
    m_file_handler = CAP_FOPEN_64(name,"rb");
   
    if (m_file_handler == 0) {
        printf(" failed to open cap file %s : errno : %d\n",name, errno);
        return false;
    }

   CAP_FSEEK_64 (m_file_handler, 0, SEEK_END);
   m_file_size = CAP_FTELL_64 (m_file_handler);
   rewind (m_file_handler);
 
   if (init()) {
	   m_is_open = true;
	   return true;
   }

   fclose(m_file_handler);

   return false;
}

/**
 * init the reader.
 * First read the header of the file make sure it is libpacp.
 * If so read the flip value. records are senstive to the local
 * recording machine endianity.
 * 
 * @return bool
 */
bool LibPCapReader::init()
{
	packet_file_header_t header;
	memset(&header,0,sizeof(packet_file_header_t));
    size_t n = fread(&header,1,sizeof(packet_file_header_t),m_file_handler);
	if (n < sizeof(packet_file_header_t)) {
		return false;
	}

	if (header.magic == MAGIC_NUM_FLIP) {
		m_is_flip = true;
	} else if (header.magic == MAGIC_NUM_DONT_FLIP){
		m_is_flip = false;
	} else {
		// capture file in not libpcap format.
		m_is_valid = false;
		return false;
	}

	m_is_valid = true;
	return true;
}

/**
 * flip header values.
 * @param toflip
 */
void LibPCapReader::flip(sf_pkthdr_t * toflip)
{
   toflip->ts.sec  = PAL_NTOHL(toflip->ts.sec);
   toflip->ts.msec = PAL_NTOHL(toflip->ts.msec);
   toflip->len     = PAL_NTOHL(toflip->len);
   toflip->caplen  = PAL_NTOHL(toflip->caplen);
}

bool LibPCapReader::ReadPacket(CCapPktRaw *lpPacket)
{
	if(!m_is_valid || !m_is_open)
		return false;

   sf_pkthdr_t pkt_header;
   memset(&pkt_header,0,sizeof(sf_pkthdr_t));

   if (CAP_FTELL_64(m_file_handler) == m_file_size) {
       /* reached end of file - do we loop ?*/
       if (m_loops > 0) {
           rewind(m_file_handler);
           this->init();
          
       }
   }
   int n = fread(&pkt_header,1,sizeof(sf_pkthdr_t),m_file_handler);
   if (n < sizeof(sf_pkthdr_t)) {
	   return false;
   }

   if (m_is_flip) {
		flip(&pkt_header);
   }
   if (pkt_header.len > READER_MAX_PACKET_SIZE) {
       /* cannot read this packet */
       printf("ERROR packet is too big, bigger than %d \n",READER_MAX_PACKET_SIZE);
       exit(-1);
       return false;
   }

   lpPacket->pkt_len = fread(lpPacket->raw,1,pkt_header.len,m_file_handler);

   lpPacket->time_sec  = pkt_header.ts.sec;
   lpPacket->time_nsec = pkt_header.ts.msec*1000;

   if ( lpPacket->pkt_len < pkt_header.len) {
       lpPacket->pkt_len = 0;
	   return false;
   }

   /* decrease packet limit count */
   if (m_loops > 0) {
       m_loops--;
   }
   lpPacket->pkt_cnt++;
   return true;
}

LibPCapWriter::LibPCapWriter()
{
	m_file_handler = NULL;
	m_timestamp = 0;
    m_is_open = false;
}

LibPCapWriter::~LibPCapWriter()
{
	Close();
}

/**
 * close and release file desc.
*/
void LibPCapWriter::Close()
{
	if (m_is_open) {
		fclose(m_file_handler);
		m_file_handler = NULL;
		m_is_open = false;
	}
}

/**
 * Try to open file for writing.
 * @param name - file nae
 * 
 * @return bool
 */
bool LibPCapWriter::Create(char * name)
{
	if (name == NULL) {
		return false;
	}

	if (m_is_open) {
		return true;
	}

	m_file_handler = CAP_FOPEN_64(name,"wb");
    if (m_file_handler == 0) {
        printf(" ERROR create file \n");
        return(false);
    }
    /* prepare the write counter */
    m_pkt_count = 0;
    return init();
}

/**
 * 
 * Write the libpcap header.
 * 
 * @return bool - true on success
 */
bool LibPCapWriter::init()
{

	// prepare the file header (one time header for each libpcap file)
	// and write it.
	packet_file_header_t header;
    header.magic   = MAGIC_NUM_DONT_FLIP;
    header.version_major  = 0x0002;
    header.version_minor  = 0x0004;
    header.thiszone       = 0;
    header.sigfigs        = 0;
    header.snaplen        = 2000;
	header.linktype       = 1;

    int n = fwrite(&header,1,sizeof(header),m_file_handler);
	if ( n == sizeof(packet_file_header_t )) {
		m_is_open = true;
		return true;
	}
    fclose(m_file_handler);
	return false;
}


/**
 * Write packet to file.
 * Must be called after successfull Create call.
 * @param p
 * @param size
 * 
 * @return bool  - true on success.
 */
bool LibPCapWriter::write_packet(CCapPktRaw * lpPacket)
{
	if (!m_is_open) {
		return false;
	}

	// build the packet libpcap header
    sf_pkthdr_t pkt_header;
	pkt_header.caplen = lpPacket->pkt_len;
	pkt_header.len = lpPacket->pkt_len;
	pkt_header.ts.msec = (lpPacket->time_nsec/1000);
	pkt_header.ts.sec  = lpPacket->time_sec;

	m_timestamp++;

	// write header and then the packet.
	int n = fwrite(&pkt_header,1,sizeof(sf_pkthdr_t),m_file_handler);
    n+= fwrite(lpPacket->raw,1,lpPacket->pkt_len,m_file_handler);

	if (n< ( (int)sizeof(sf_pkthdr_t) + lpPacket->pkt_len)) {
		return false;
	}
    /* advance the counter on success */
    m_pkt_count++;
    return true;
}

uint32_t LibPCapWriter::get_pkt_count() {
    return m_pkt_count;
}

