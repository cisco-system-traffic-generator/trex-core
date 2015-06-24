/*
*
* Copyright (c) 2003 Endace Technology Ltd, Hamilton, New Zealand.
* All rights reserved.
*
* This software and documentation has been developed by Endace Technology Ltd.
* along with the DAG PCI network capture cards. For further information please
* visit http://www.endace.com/.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  1. Redistributions of source code must retain the above copyright notice,
*  this list of conditions and the following disclaimer.
*
*  2. Redistributions in binary form must reproduce the above copyright
*  notice, this list of conditions and the following disclaimer in the
*  documentation and/or other materials provided with the distribution.
*
*  3. The name of Endace Technology Ltd may not be used to endorse or promote
*  products derived from this software without specific prior written
*  permission.
*
* THIS SOFTWARE IS PROVIDED BY ENDACE TECHNOLOGY LTD ``AS IS'' AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
* EVENT SHALL ENDACE TECHNOLOGY LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* $Id: erf.c 15544 2005-08-26 19:40:46Z guy $
*/

/*****                         
 * NAME
 *   
 *   
 * AUTHOR
 *   taken from SCE
 *   
 * COPYRIGHT
 *   Copyright (c) 2004-2011 by cisco Systems, Inc.
 *   All rights reserved.
 *   
 * DESCRIPTION
 *   
 ****/

/* 
 * erf - Endace ERF (Extensible Record Format)
 *
 * See
 *
 *	http://www.endace.com/support/EndaceRecordFormat.pdf
 */



#include <stdlib.h>
#include <string.h>
#include "erf.h"
#include "basic_utils.h"
#include "pal_utl.h"


#define MAX_ERF_PACKET   READER_MAX_PACKET_SIZE 


extern long file_seek(void *stream, long offset, int whence, int *err);
#define file_read fread
#define file_write fwrite
#define file_close fclose
extern int file_error(FILE *fh);
#define file_getc fgetc
#define file_gets fgets
#define file_eof feof


long file_seek(FILE *stream, long offset, int whence, int *err)
{
	long ret;

	ret = CAP_FSEEK_64(stream, offset, whence);
	if (ret == -1)
		*err = file_error(stream);
	return ret;
}


int file_error(FILE *fh)
{
	if (ferror(fh))
		return -1;
	else
		return 0;
}


int erf_open(wtap *wth, int *err)
{
	guint32 i;
	int common_type = 0;
	erf_timestamp_t prevts;

	memset(&prevts, 0, sizeof(prevts));

    int records_for_erf_check = 10;

	/* ERF is a little hard because there's no magic number */

	for (i = 0; i < (guint32)records_for_erf_check; i++) {

		erf_header_t header;
		guint32 packet_size;
		erf_timestamp_t ts;

		if (file_read(&header,1,sizeof(header),wth->fh) != sizeof(header)) {
			if ((*err = file_error(wth->fh)) != 0)
				return -1;
			else
				break; /* eof */
		}

		packet_size = g_ntohs(header.rlen) - sizeof(header);

		/* fail on invalid record type, decreasing timestamps or non-zero pad-bits */
		if (header.type == 0 || header.type != TYPE_ETH ||
		    (header.flags & 0xc0) != 0) {
			return 0;
		}

		if ((ts = pletohll(&header.ts)) < prevts) {
			/* reassembled AAL5 records may not be in time order, so allow 1 sec fudge */
			if (header.type != TYPE_AAL5 || ((prevts-ts)>>32) > 1) {
				return 0;
			}
		}
		memcpy(&prevts, &ts, sizeof(prevts));

		if (common_type == 0) {
			common_type = header.type;
		} else
		if (common_type > 0 && common_type != header.type) {
			common_type = -1;
		}

		if (header.type == TYPE_HDLC_POS ) {
            // do not support HDLS
            return (-1);
		}
        if (file_seek(wth->fh, packet_size, SEEK_CUR, err) == -1) {
            return -1;
        }
	}

	if (file_seek(wth->fh, 0L, SEEK_SET, err) == -1) {	/* rewind */
		return -1;
	}
	wth->data_offset = 0;
    // VALID ERF file
	return 1;
}


int erf_read(wtap *wth,char *p,uint32_t *sec,uint32_t *nsec)
{
    erf_header_t header;
    int common_type = 0;
    if (file_read(&header,1,sizeof(header),wth->fh) != sizeof(header)) {
        if (( file_error(wth->fh)) != 0)
            return -1;
        else
            return (0); // end 
    }

    guint32 packet_size = g_ntohs(header.rlen) - sizeof(header);

    /* fail on invalid record type, decreasing timestamps or non-zero pad-bits */
    if ( header.type != TYPE_ETH ||
        (header.flags & 0xc0) != 0) {
        //printf(" ERF header not supported \n");
        // no valid 
        return -1;
    }
	 
    if (common_type == 0) {
        common_type = header.type;
    } else
    if (common_type > 0 && common_type != header.type) {
        common_type = -1;
    }
	 

    if ( (  packet_size >= MAX_ERF_PACKET ) || 
        (g_ntohs(header.wlen)>MAX_ERF_PACKET) ) {
        printf(" ERF packet size too big  \n");
		assert(0);
        return (-1);
    }

    int err;
    if (file_seek(wth->fh, 2, SEEK_CUR, &err) == -1) {
        return -1;
    }
    int realpkt_size = packet_size-2;

    if (file_read(p,1,realpkt_size ,wth->fh) == realpkt_size) {
        guint64 ts = pletohll(&header.ts);
        *sec = (uint32_t) (ts >> 32);
        uint32_t frac =(ts &0xffffffff);
        double usec_frac =(double)frac*(1000000000.0/(4294967296.0));
        *nsec = (uint32_t) (usec_frac);
        return (g_ntohs(header.wlen));
    }else{
        return (-1);
    }
}


bool CErfFileWriter::Create(char *file_name){
    m_fd=CAP_FOPEN_64(file_name,"wb");
    if (m_fd==0) {
        printf(" ERROR create file \n");
        return(false);
    }
    m_cnt=0;
    return(true);
}

void CErfFileWriter::Delete(){
    if (m_fd) {
       fclose(m_fd);
       m_fd=0;
    }
}


typedef struct erf_dummy_header_ {
    uint16_t dummy;
}erf_dummy_header_t ;

static uint32_t frame_check[20];

bool CErfFileWriter::write_packet(CCapPktRaw * lpPacket){
    erf_header_t header;
    erf_dummy_header_t dummy;
    
    dummy.dummy =0;
    memset(&header,0,sizeof(erf_header_t));
    double nsec_frac = 4294967295.9 *(lpPacket->time_nsec /1000000000.0);
    uint64_t ts= (((uint64_t)lpPacket->time_sec) <<32) +((uint32_t)nsec_frac);
    header.ts =ts;

    uint16_t size=lpPacket->pkt_len;

    uint16_t total_size=(uint16_t)size+sizeof(erf_header_t)+2+4;
    uint16_t align = (total_size & 0x7);
    if (align >0 ) {
        align = 8-align;
    }

    header.flags =4+lpPacket->getInterface();
    header.type =TYPE_ETH;
    header.wlen = g_ntohs((uint16_t)size+4);
    header.rlen = g_ntohs(total_size+align);

    int n = fwrite(&header,1,sizeof(header),m_fd);
    n+= fwrite(&dummy,1,sizeof(dummy),m_fd);
    n+= fwrite(lpPacket->raw,1,size,m_fd);
    n+= fwrite(frame_check,1,4+align,m_fd);

	if (n < (int)(total_size+align)) {
		return false;
	}
	return true;
}



bool CPcapFileWriter::Create(char *file_name){
    m_fd=CAP_FOPEN_64(file_name,"wb");
    if (m_fd==0) {
        printf(" ERROR create file \n");
        return(false);
    }
    m_cnt=0;
    return(true);
}

void CPcapFileWriter::Delete(){
    if (m_fd) {
       fclose(m_fd);
       m_fd=0;
    }
}




#define	PCAP_MAGIC			0xa1b2c3d4
#define	PCAP_SWAPPED_MAGIC		0xd4c3b2a1
#define	PCAP_MODIFIED_MAGIC		0xa1b2cd34
#define	PCAP_SWAPPED_MODIFIED_MAGIC	0x34cdb2a1
#define	PCAP_NSEC_MAGIC			0xa1b23c4d
#define	PCAP_SWAPPED_NSEC_MAGIC		0x4d3cb2a1

/* "libpcap" file header (minus magic number). */
struct pcap_hdr {
    guint32 magic_number;
	guint16	version_major;	/* major version number */
	guint16	version_minor;	/* minor version number */
	gint32	thiszone;	/* GMT to local correction */
	guint32	sigfigs;	/* accuracy of timestamps */
	guint32	snaplen;	/* max length of captured packets, in octets */
	guint32	network;	/* data link type */
};

/* "libpcap" record header. */
struct pcaprec_hdr {
	guint32	ts_sec;		/* timestamp seconds */
	guint32	ts_usec;	/* timestamp microseconds (nsecs for PCAP_NSEC_MAGIC) */
	guint32	incl_len;	/* number of octets of packet saved in file */
	guint32	orig_len;	/* actual length of packet */
};


bool CPcapFileWriter::write_packet(CCapPktRaw * lpPacket){
    if (m_cnt == 0) {
        pcap_hdr header;
        header.magic_number   = PCAP_NSEC_MAGIC;
        header.version_major  = 0x0002;
        header.version_minor  = 0x0004;
        header.thiszone       = 0;
        header.sigfigs        = 0;
        header.snaplen        = 2000;
        header.network        = 1;
        fwrite(&header,1,sizeof(header),m_fd);
    }
    pcaprec_hdr pkt_header;
    pkt_header.ts_sec   = lpPacket->time_sec ;
    pkt_header.ts_usec  = lpPacket->time_nsec;
    pkt_header.incl_len = lpPacket->pkt_len;
    pkt_header.orig_len = lpPacket->pkt_len;
    fwrite(&pkt_header,1,sizeof(pkt_header),m_fd);
    fwrite(lpPacket->raw,1,lpPacket->pkt_len,m_fd);
    m_cnt++;
	return true;
}



#if 0
 //erf_create(wtap *wth,char *p,uint32_t *sec)

static uint8_t DataPacket0[]={

0x00, 0x50, 0x04, 0xB9 ,0xC8, 0xA0, 
0x00, 0x50, 0x04, 0xB9, 0xC5, 0x83, 
0x08, 0x00, 

0x45, 0x10, 0x00, 0x40, 
0x00, 0x00, 0x40, 0x00, 
0x80, 0x06, 0xDD, 0x99, 

0x0A, 0x01, 0x04, 0x91, 
0x0A, 0x01, 0x04, 0x90, 

0x05, 0x11, 
0x00, 0x50, 

0x00, 0x00, 0xF9, 0x00, 
0x00, 0x00, 0x00, 0x00, 

0x60, 0x00, 0x20, 0x00, 
0x5C, 0xA2, 0x00, 0x00, 
0x02, 0x04, 0x05, 0xB4, 
0x00, 0x00, 0x76, 0x4A, 

0x60, 0x02, 0x20, 0x00, 
0x5C, 0xA2, 0x00, 0x00, 
0x02, 0x04, 0x05, 0xB4, 
0x00, 0x00, 0x76, 0x4A, 
0x60, 0x02, 0x20, 0x00, 
0x5C, 0xA2, 0x00, 0x00, 
0x02, 0x04, 0x05, 0xB4, 
0x00, 0x00, 0x76, 0x4A, 

};


void test_erf_create(void){
    CPcapFileWriter erf ;
    erf.Create("my_test.erf");
    int i;
    for (i=0; i<10; i++) {
        erf.write_packet((char *)&DataPacket0[0],sizeof(DataPacket0));
    }
    erf.Delete();
}
#endif


bool CErfFileReader::Create(char *filename, int loops){   

    this->m_loops = loops;
	m_handle = CAP_FOPEN_64(filename, "rb");
    if (m_handle == NULL) {
        fprintf(stderr, "Failed to open file `%s'.\n", filename);
        return false;
    }

	
    CAP_FSEEK_64 (m_handle, 0, SEEK_END);
    m_file_size = CAP_FTELL_64(m_handle);
    rewind (m_handle);

    wtap wth;
    memset(&wth,0,sizeof(wtap));
    int err=0;
    wth.fh =m_handle;
    if ( erf_open(&wth, &err)== 1){
        return (true);
    }else{
        return (false);
    }
}

void CErfFileReader::Delete(){
    if (m_handle) {
        fclose(m_handle);
        m_handle=0;
    }
}


bool CErfFileReader::ReadPacket(CCapPktRaw * lpPacket){
    wtap wth;
    wth.fh = m_handle;
    int length;
    length=erf_read(&wth,lpPacket->raw,&lpPacket->time_sec,
                    &lpPacket->time_nsec
                    );
    if ( length >0   ) {
        lpPacket->pkt_len =(uint16_t)length;
		lpPacket->pkt_cnt++;
        return (true);
    }
    return (false);
}

