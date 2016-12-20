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
* $Id: erf.h 15544 2005-08-26 19:40:46Z guy $
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


#ifndef __W_ERF_H__
#define __W_ERF_H__
#include "captureFile.h"
/* Record type defines */
#define TYPE_LEGACY	0
#define TYPE_HDLC_POS	1
#define TYPE_ETH	2
#define TYPE_ATM	3
#define TYPE_AAL5	4
#include <stdio.h>

typedef uint64_t guint64 ;
typedef uint32_t guint32 ;
typedef int32_t gint32;
typedef uint16_t guint16 ;
typedef uint8_t guint8 ;
typedef uint8_t gchar;




#define g_htonl PAL_NTOHL
#define g_htons PAL_NTOHS
#define g_ntohs PAL_NTOHS
#define g_ntohl PAL_NTOHL


#ifndef pntohs
#define pntohs(p)  ((guint16)                       \
                    ((guint16)*((const guint8 *)(p)+0)<<8|  \
                     (guint16)*((const guint8 *)(p)+1)<<0))
#endif

#ifndef pntoh24
#define pntoh24(p)  ((guint32)*((const guint8 *)(p)+0)<<16| \
                     (guint32)*((const guint8 *)(p)+1)<<8|  \
                     (guint32)*((const guint8 *)(p)+2)<<0)
#endif

#ifndef pntohl
#define pntohl(p)  ((guint32)*((const guint8 *)(p)+0)<<24|  \
                    (guint32)*((const guint8 *)(p)+1)<<16|  \
                    (guint32)*((const guint8 *)(p)+2)<<8|   \
                    (guint32)*((const guint8 *)(p)+3)<<0)
#endif

#ifndef pntohll
#define pntohll(p)  ((guint64)*((const guint8 *)(p)+0)<<56|  \
                     (guint64)*((const guint8 *)(p)+1)<<48|  \
                     (guint64)*((const guint8 *)(p)+2)<<40|  \
                     (guint64)*((const guint8 *)(p)+3)<<32|  \
                     (guint64)*((const guint8 *)(p)+4)<<24|  \
                     (guint64)*((const guint8 *)(p)+5)<<16|  \
                     (guint64)*((const guint8 *)(p)+6)<<8|   \
                     (guint64)*((const guint8 *)(p)+7)<<0)
#endif


#ifndef phtons
#define phtons(p)  ((guint16)                       \
                    ((guint16)*((const guint8 *)(p)+0)<<8|  \
                     (guint16)*((const guint8 *)(p)+1)<<0))
#endif

#ifndef phtonl
#define phtonl(p)  ((guint32)*((const guint8 *)(p)+0)<<24|  \
                    (guint32)*((const guint8 *)(p)+1)<<16|  \
                    (guint32)*((const guint8 *)(p)+2)<<8|   \
                    (guint32)*((const guint8 *)(p)+3)<<0)
#endif

#ifndef pletohs
#define pletohs(p) ((guint16)                       \
                    ((guint16)*((const guint8 *)(p)+1)<<8|  \
                     (guint16)*((const guint8 *)(p)+0)<<0))
#endif

#ifndef pletoh24
#define pletoh24(p) ((guint32)*((const guint8 *)(p)+2)<<16|  \
                     (guint32)*((const guint8 *)(p)+1)<<8|  \
                     (guint32)*((const guint8 *)(p)+0)<<0)
#endif


#ifndef pletohl
#define pletohl(p) ((guint32)*((const guint8 *)(p)+3)<<24|  \
                    (guint32)*((const guint8 *)(p)+2)<<16|  \
                    (guint32)*((const guint8 *)(p)+1)<<8|   \
                    (guint32)*((const guint8 *)(p)+0)<<0)
#endif


#ifndef pletohll
#define pletohll(p) ((guint64)*((const guint8 *)(p)+7)<<56|  \
                     (guint64)*((const guint8 *)(p)+6)<<48|  \
                     (guint64)*((const guint8 *)(p)+5)<<40|  \
                     (guint64)*((const guint8 *)(p)+4)<<32|  \
                     (guint64)*((const guint8 *)(p)+3)<<24|  \
                     (guint64)*((const guint8 *)(p)+2)<<16|  \
                     (guint64)*((const guint8 *)(p)+1)<<8|   \
                     (guint64)*((const guint8 *)(p)+0)<<0)
#endif


 /*
  * The timestamp is 64bit unsigned fixed point little-endian value with
  * 32 bits for second and 32 bits for fraction.
  */
typedef guint64 erf_timestamp_t;

typedef struct erf_record {
	erf_timestamp_t	ts;
	guint8		type;
	guint8		flags;
	guint16		rlen;
	guint16		lctr;
	guint16		wlen;
} erf_header_t;

#define MAX_RECORD_LEN	0x10000 /* 64k */
#define RECORDS_FOR_ERF_CHECK	3
#define FCS_BITS	32

#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif

/*
 * ATM snaplength
 */
#define ATM_SNAPLEN		48

/*
 * Size of ATM payload 
 */
#define ATM_SLEN(h, e)		ATM_SNAPLEN
#define ATM_WLEN(h, e)		ATM_SNAPLEN

/*
 * Size of Ethernet payload
 */
#define ETHERNET_WLEN(h, e)	(g_htons((h)->wlen))
#define ETHERNET_SLEN(h, e)	min(ETHERNET_WLEN(h, e), g_htons((h)->rlen) - sizeof(*(h)) - 2)

/*
 * Size of HDLC payload
 */
#define HDLC_WLEN(h, e)		(g_htons((h)->wlen))
#define HDLC_SLEN(h, e)		min(HDLC_WLEN(h, e), g_htons((h)->rlen) - sizeof(*(h)))

//int erf_open(wtap *wth, int *err, gchar **err_info);



struct wtap {
	FILE *			fh;
	int			    file_type;
	long			data_offset;
};

int erf_open(wtap *wth, int *err);

int erf_read(wtap *wth,char *p,uint32_t *sec,uint32_t *nsec);



class CErfFileWriter :  public CFileWriterBase {
public:
	virtual ~CErfFileWriter(){
		Delete();
	}
    virtual bool Create(char *file_name);
    void Delete();
    virtual bool write_packet(CCapPktRaw * lpPacket);
    
    /**
     * flush all packets to disk
     * 
     */
    void flush_to_disk();
    
private:
    FILE *m_fd;
    int m_cnt;

};


class CPcapFileWriter : CFileWriterBase{
public:
    bool Create(char *file_name);
    void Delete();

    bool write_packet(CCapPktRaw * lpPacket);
private:
    FILE *m_fd;
    int m_cnt;
};

class CErfFileReader : public CCapReaderBase  {
public:
	virtual ~CErfFileReader() { Delete();}
    bool Create(char *filename, int loops = 0);
    void Delete();

    virtual bool ReadPacket(CCapPktRaw * lpPacket);
    virtual void Rewind();

    virtual capture_type_e get_type() {
        return ERF;
    }

private:
    FILE * m_handle;
};



#endif /* __W_ERF_H__ */
