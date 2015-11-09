#ifndef  RX_CHECK_H
#define  RX_CHECK_H
/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

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


#include "timer_wheel_pq.h"
#include "rx_check_header.h"
#include "time_histogram.h"
#include "utl_jitter.h"

                                 

typedef enum {
    CLIENT_SIDE = 0,    
    SERVER_SIDE = 1,    
    CS_NUM = 2,
    CS_INVALID = 255
} pkt_dir_enum_t;

typedef uint8_t   pkt_dir_t  ;


void  flow_aging_callback(CFlowTimerHandle * t);

class CRxCheckFlowPerDir {
public:
    CRxCheckFlowPerDir(){
        m_flags=0;
        m_pkts=0;
        m_seq=0;
        m_flow_size=0;

    }
    uint16_t         m_flow_size; // how many packets in this direction 
    uint16_t         m_pkts;
    uint16_t         m_seq;
private:
    uint16_t         m_flags;
public:

    void set_fif_seen(uint16_t flow_per_dir){
        m_pkts=1;
        m_seq=1;
        m_flags |=2;
        m_flow_size=flow_per_dir;
    }
    bool is_fif_seen(){
        return ( (m_flags & 2) ==2 ?true:false);
    }

    void set_init_not_from_first_packet(){
        m_flags |=1;
    }
    bool is_init_not_from_first_pkt(){
		return ( (m_flags & 2) ==2 ?true:false);
    }
};

class CRxCheckFlow {
public:
    CRxCheckFlow(){
        m_aging_timer_handle.m_callback =flow_aging_callback;
        m_aging_timer_handle.m_object = (void *)this;
        m_aging_timer_handle.m_id= 0x1234;
		m_oo_err=0;
        m_flags=0;
    }


public:
    /* timestamp of FIF */
    uint64_t            m_flow_id; /* key*/
    CRxCheckFlowPerDir  m_dir[CS_NUM];
    CFlowTimerHandle    m_aging_timer_handle;
    uint16_t            m_oo_err; /* out of order issue */
    uint16_t            m_flags;
public:

    uint16_t  get_total_pkt_seen(void){
        return (m_dir[0].m_pkts+
                m_dir[1].m_pkts);
    }
    uint16_t  get_total_pkt_expected(void){
        return (m_dir[0].m_flow_size+
                m_dir[1].m_flow_size);
    }

    bool      is_all_pkts_seen(void){
        int i;
        int c=0;
        for (i=0; i<2; i++) {
            if ( (m_dir[i].m_pkts!=m_dir[i].m_flow_size) ){
                return (false);
            }
            if (m_dir[i].m_flow_size>0) {
                c++;
            }
        }
        int expc=is_both_dir()?2:1;
        if ( expc == c ) {
            return ( (m_oo_err==0)?true:false );
        }
        return (false);
    }

    void set_both_dir(){
        m_flags |=2;
    }
    bool is_both_dir(){
        return ( (m_flags & 2) == 2 ?true:false);
    }



    void set_aged_correctly(){
        m_flags |=4;
    }
    bool is_aged_correctly(){
        return ( (m_flags & 4) ==4 ?true:false);
    }

};




class CRxCheckFlowTableStats {
public:

    uint64_t  m_total_rx;
    uint64_t  m_total_rx_bytes;

    uint64_t  m_lookup;
    uint64_t  m_found;
    uint64_t  m_fif;
    uint64_t  m_add;
    uint64_t  m_remove;
    uint64_t  m_active;
	uint64_t  m_err_drop;
	uint64_t  m_err_aged;


    uint64_t  m_err_no_magic;
    uint64_t  m_err_wrong_pkt_id;
    uint64_t  m_err_fif_seen_twice;
	uint64_t  m_err_open_with_no_fif_pkt;

	uint64_t  m_err_oo_dup; /* got same packets id twice  expect 1 got 0 */

	uint64_t  m_err_oo_early; /* miss packet ,expect 1 got 2 */
	uint64_t  m_err_oo_late;  /* early packet ,expect 7 got 6 */

    uint64_t  m_err_flow_length_changed;  /* early packet ,expect 7 got 6 */

    uint64_t get_total_err(void){
        return (m_err_drop+m_err_aged+
                    m_err_no_magic+
                    m_err_wrong_pkt_id+
                    m_err_fif_seen_twice+
                    m_err_open_with_no_fif_pkt+
                    m_err_oo_dup+
                    m_err_oo_early+
                    m_err_oo_late+m_err_flow_length_changed);

    }

public:
    void Clear();
    void Dump(FILE *fd);
    void dump_json(std::string & json);
};



typedef CRxCheckFlow * rx_check_flow_ptr;
typedef std::map<uint64_t, rx_check_flow_ptr, std::less<uint64_t> > rx_check_flow_map_t;
typedef rx_check_flow_map_t::iterator rx_check_flow_map_iter_t;


class CRxCheckFlowTableMap   {
public:
    virtual bool Create(int max_size);
    virtual void Delete();
    virtual bool remove(uint64_t fid );
    virtual CRxCheckFlow * lookup(uint64_t fid );
    virtual CRxCheckFlow * add(uint64_t fid );
    virtual void remove_all(void);
    void dump_all(FILE *fd);
    uint64_t count(void);
public:
    rx_check_flow_map_t          m_map;
};



class uint64_tHashEnv
{
public:
	static uint32_t Hash(uint64_t x)
	{
		return ( (x >>40) ^ (x & 0xffffffff));
	}
};

template<class T>
class CMyFSA {
public:
	bool Create(uint32_t size, bool supportGetNext, bool ctorRequired){
		return(true);
	}

    void Delete(){
	}
	void Reset(){
	}

	T * GetNewItem(){
		return (new T());
	}

	void ReturnItem(T *obj){
		delete obj;
	}
};



#if 0

typedef CHashEntry<uint64_t,CRxCheckFlow> rx_c_hash_ent_t;
typedef CCloseHash<uint64_t,CRxCheckFlow,uint64_tHashEnv,CMyFSA<rx_c_hash_ent_t> > rx_c_hash_t;


class CRxCheckFlowTableHash   {
public:
    bool Create(int max_size){
		return ( m_hash.Create(max_size,0,false,false,true) );
	}
    void Delete(){
		m_hash.Delete();
	}
    bool remove(uint64_t fid ) {
		return(m_hash.Remove(fid)==hsOK?true:false);
	}
    CRxCheckFlow * lookup(uint64_t fid ){
		rx_c_hash_ent_t *lp=m_hash.Find(fid);
		if (lp) {
			return (&lp->value);
		}else{
			return ((CRxCheckFlow *)NULL);
		}
	}
    CRxCheckFlow * add(uint64_t fid ){
		rx_c_hash_ent_t *lp;
		assert(m_hash.Insert(fid,lp)==hsOK);
		return (&lp->value);
	}

    void remove_all(void){
		
	}
    void dump_all(FILE *fd){
		m_hash.Dump(fd);
	}
    uint64_t count(void){
		return ( m_hash.GetSize());

	}
public:

	rx_c_hash_t                  m_hash;
};

#endif


// must be 2^
#define MAX_TEMPLATES_STATS 32

#define MAX_TEMPLATES_STATS_MASK (MAX_TEMPLATES_STATS-1)

class CPerTxthreadTemplateInfo {

public:
    CPerTxthreadTemplateInfo(){
        Clear();
    }
    void Clear(){
        memset(m_template_info,0,sizeof(m_template_info));
    }
    void Dump(FILE *fd);

    void Add(CPerTxthreadTemplateInfo * obj);

    void inc_template(uint8_t index){
        if (index<MAX_TEMPLATES_STATS) {
            m_template_info[index]++;
        }else{
            m_template_info[MAX_TEMPLATES_STATS-1]++;
        }
    }

    std::string dump_as_json(std::string name);

    uint64_t               m_template_info[MAX_TEMPLATES_STATS];
};

class CPerTemplateInfo {
public:
    CPerTemplateInfo()  {
        reset();
    }

    void reset(){
        m_errors=0;
        m_rx_pkts=0;
        m_jitter.reset();
    }

    void  calc(double dtime){
        m_jitter.calc(dtime);
    }

    uint32_t get_jitter_usec(){
        return ((uint32_t)(m_jitter.get_jitter()*1000000.0));
    }

    void      inc_error_counter(void){
        m_errors++;

    }

    uint64_t get_error_counter(){
        return (m_errors);
    }

    void      inc_rx_counter(void){
        m_rx_pkts++;
    }

    uint64_t get_rx_counter(){
        return (m_rx_pkts);
    }


private:
    uint64_t m_rx_pkts;
    CJitter  m_jitter;
    uint64_t m_errors;
};

class RxCheckManager {

public:
    bool Create();
    void Delete();
    void handle_packet(CRx_check_header * rxh);
	void Dump(FILE *fd);
    void DumpShort(FILE *fd);
    void DumpTemplate(FILE *fd,bool verbose);
    void DumpTemplateFull(FILE *fd);

    uint32_t getTemplateMaxJitter();

    void template_dump_json(std::string & json);

    uint64_t getTotalRx(void){
        return ( m_stats.m_total_rx );
    }

	void tw_drain();
    void tw_handle();

    void dump_json(std::string & json );

protected:
    void update_template_err(uint8_t template_id);

    CPerTemplateInfo * get_template(uint8_t index){
        uint8_t _id;
         if ( index < MAX_TEMPLATES_STATS_MASK ){
             _id=index;
         }else{
             _id=MAX_TEMPLATES_STATS_MASK;
         }
        return (&m_template_info[_id]);
    }

    bool on_flow_end(CRxCheckFlow * lp);
    friend void  flow_aging_callback(CFlowTimerHandle * t);
public:
    
    CTimerWheel                     m_tw;
    CRxCheckFlowTableMap           m_ft;
    CRxCheckFlowTableStats         m_stats;

    CTimeHistogram                 m_hist;
    CPerTemplateInfo               m_template_info[MAX_TEMPLATES_STATS];
    bool                           m_on_drain;
public:
    dsec_t                         m_cur_time;

};





#endif
