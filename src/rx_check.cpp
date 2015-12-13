#include "rx_check.h"
#include "utl_json.h"
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


void CRxCheckFlowTableStats::Clear(){
  m_total_rx_bytes=0;
  m_total_rx=0;
  m_lookup=0;
  m_found=0;
  m_fif=0;
  m_add=0;
  m_remove=0;
  m_active=0;
  m_err_no_magic=0;
  m_err_drop=0;
  m_err_aged=0;
  m_err_no_magic=0;
  m_err_wrong_pkt_id=0;
  m_err_fif_seen_twice=0;
  m_err_open_with_no_fif_pkt=0;
  m_err_oo_dup=0;
  m_err_oo_early=0;
  m_err_oo_late=0;
  m_err_flow_length_changed=0;

}

#define MYDP(f) if (f)  fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)f)
#define MYDP_A(f)     fprintf(fd," %-40s: %llu \n",#f,(unsigned long long)f)
#define MYDP_J(f)  json+=add_json(#f,f);
#define MYDP_J_LAST(f)  json+=add_json(#f,f,true);



void CRxCheckFlowTableStats::Dump(FILE *fd){
    MYDP (m_total_rx_bytes);
    MYDP (m_total_rx);
    MYDP (m_lookup);
    MYDP (m_found);
    MYDP (m_fif);
    MYDP (m_add);
    MYDP (m_remove);
    MYDP_A (m_active);
	MYDP (m_err_no_magic);
	MYDP (m_err_drop);
	MYDP (m_err_aged);
	MYDP (m_err_no_magic);
	MYDP (m_err_wrong_pkt_id);
	MYDP (m_err_fif_seen_twice);
	MYDP (m_err_open_with_no_fif_pkt);
	MYDP (m_err_oo_dup);
	MYDP (m_err_oo_early);
	MYDP (m_err_oo_late);
    MYDP (m_err_flow_length_changed);
}

void CRxCheckFlowTableStats::dump_json(std::string & json){
    json+="\"stats\" : {";

    MYDP_J (m_total_rx_bytes);
    MYDP_J (m_total_rx);
    MYDP_J (m_lookup);
    MYDP_J (m_found);
    MYDP_J (m_fif);
    MYDP_J (m_add);
    MYDP_J (m_remove);
    MYDP_J (m_active);
    MYDP_J (m_err_no_magic);
    MYDP_J (m_err_drop);
    MYDP_J (m_err_aged);
    MYDP_J (m_err_no_magic);
    MYDP_J (m_err_wrong_pkt_id);
    MYDP_J (m_err_fif_seen_twice);
    MYDP_J (m_err_open_with_no_fif_pkt);
    MYDP_J (m_err_oo_dup);
    MYDP_J (m_err_oo_early);
    MYDP_J (m_err_oo_late);

    /* must be last */
    MYDP_J_LAST (m_err_flow_length_changed);
    json+="},";
}



bool CRxCheckFlowTableMap::Create(int max_size){
    return (true);
}

void CRxCheckFlowTableMap::Delete(){
    remove_all();
}

bool CRxCheckFlowTableMap::remove(uint64_t key ){
	CRxCheckFlow *lp = lookup(key);
    if ( lp ) {
        delete lp;
        m_map.erase(key);
        return(true);
    }else{
        return(false);
    }
}


CRxCheckFlow * CRxCheckFlowTableMap::lookup(uint64_t key ){
    rx_check_flow_map_t::iterator iter;
    iter = m_map.find(key);
    if (iter != m_map.end() ) {
        return ( (*iter).second );
    }else{
        return (( CRxCheckFlow*)0);
    }
}

CRxCheckFlow * CRxCheckFlowTableMap::add(uint64_t key ){
    CRxCheckFlow * flow = new CRxCheckFlow();
    m_map.insert(rx_check_flow_map_t::value_type(key,flow));
    return (flow);

}


void CRxCheckFlowTableMap::dump_all(FILE *fd){

    rx_check_flow_map_iter_t it;
    for (it= m_map.begin(); it != m_map.end(); ++it) {
        CRxCheckFlow *lp = it->second;
        printf ("flow_id: %llu \n",(unsigned long long)lp->m_flow_id);
    }
}


void CRxCheckFlowTableMap::remove_all(){
    if ( m_map.empty() ) 
        return;

    rx_check_flow_map_iter_t it;
    for (it= m_map.begin(); it != m_map.end(); ++it) {
        CRxCheckFlow *lp = it->second;
        delete lp;
    }
    m_map.clear();
}

uint64_t CRxCheckFlowTableMap::count(){
    return ( m_map.size());
}


#ifdef FT_TEST

void test_flowtable (){
    CRxCheckFlowTableMap map;
    map.Create(1000);
    CRxCheckFlow * lp;

     lp=map.lookup(1);
     assert(lp==0);
     lp=map.add(2);
	 lp->m_flow_id = 7;
	 printf("%x\n", lp);
     lp=map.lookup(2);
	 printf("%x\n", lp);
	 assert(lp);
	 map.dump_all(stdout);
}

#endif



void RxCheckManager::tw_handle(){

    m_tw.try_handle_events(m_cur_time);
}


void RxCheckManager::tw_drain(){
    m_on_drain=true;
    m_tw.drain_all();
    m_on_drain=false;
}


std::string CPerTxthreadTemplateInfo::dump_as_json(std::string name){
    std::string json="\"template\":[";
    int i;
    for (i=0;i<MAX_TEMPLATES_STATS;i++){
        char buff[200];
        sprintf(buff,"%llu", (unsigned long long)m_template_info[i]);
        json+=std::string(buff);
        if ( i < MAX_TEMPLATES_STATS-1) {
            json+=std::string(",");
        }
    }
    json+="]," ;
    return (json);
}


void CPerTxthreadTemplateInfo::Add(CPerTxthreadTemplateInfo * obj){
    int i;
    for (i=0; i<MAX_TEMPLATES_STATS; i++) {
        m_template_info[i]+=obj->m_template_info[i];
    }
}


void CPerTxthreadTemplateInfo::Dump(FILE *fd){
    int i;
    for (i=0; i<MAX_TEMPLATES_STATS; i++) {
        if (m_template_info[i]) {
            fprintf (fd," template id: %d %llu \n",i, (unsigned long long)m_template_info[i]);
        }
    }
}


bool RxCheckManager::Create(){
    m_ft.Create(100000);
    m_stats.Clear();
    m_hist.Create();
	m_cur_time=0.00000001;
    m_on_drain=false;

    int i;
    for (i=0; i<MAX_TEMPLATES_STATS;i++ ) {
        m_template_info[i].reset();
    }
	return (true);

}


void RxCheckManager::handle_packet(CRx_check_header * rxh){
    //rxh->dump(stdout);
    m_stats.m_total_rx++;
    if ( rxh->m_magic != RX_CHECK_MAGIC ){
        m_stats.m_err_no_magic++;
        update_template_err(rxh->m_template_id);
        return;
    }
    if ((rxh->m_pkt_id+1) > rxh->m_flow_size ){
        m_stats.m_err_wrong_pkt_id++;
        update_template_err(rxh->m_template_id);
        return;
    }

    m_cur_time=now_sec();

    uint64_t d = (os_get_hr_tick_32() - rxh->m_time_stamp );
    double dt= ptime_convert_hr_dsec(d);
    m_hist.Add(dt);
    // calc jitter per template 
    CPerTemplateInfo *lpt= get_template(rxh->m_template_id);
    lpt->calc(dt);
    lpt->inc_rx_counter();

    CRxCheckFlow * lf;
	/* lookup */
    lf=m_ft.lookup(rxh->m_flow_id);
	m_stats.m_lookup++;


    bool any_err=false;
    if ( rxh->is_fif_dir() ) {
        if (lf==0) {
            /* valid , this is FIF we don't expect flows */
            lf=m_ft.add(rxh->m_flow_id);
            assert(lf);
            lf->m_aging_timer_handle.m_object1=this;

            lf->m_flow_id=rxh->m_flow_id;
            if (rxh->get_both_dir()) {
                lf->set_both_dir();
            }

            CRxCheckFlowPerDir *lpd=&lf->m_dir[rxh->get_dir()];
            lpd->set_fif_seen(rxh->m_flow_size);
            m_stats.m_fif++;
            m_stats.m_add++;
            m_stats.m_active++;
        }else{
            m_stats.m_found++;
            CRxCheckFlowPerDir *lpd=&lf->m_dir[rxh->get_dir()];
            if ( lpd->is_fif_seen() ){
                lf->m_oo_err++;
                any_err=true;
                m_stats.m_err_fif_seen_twice++;
                if ( lpd->m_flow_size != rxh->m_flow_size ){
                    m_stats.m_err_flow_length_changed++;
                    lf->m_oo_err++;
                }
                lpd->m_pkts++;
            }else{
                /* first in direction , we are OK */
                lpd->set_fif_seen(rxh->m_flow_size);
            }
        }
    }else{
		/* NON FIF */
        if (lf==0) {
            /* no flow at it is not the first packet */
            /* the first packet was dropped ?? */
            lf=m_ft.add(rxh->m_flow_id);
            assert(lf);
            lf->m_aging_timer_handle.m_object1=this;
            if (rxh->get_both_dir()) {
                lf->set_both_dir();
            }

            lf->m_flow_id=rxh->m_flow_id;
            CRxCheckFlowPerDir * lpd=&lf->m_dir[rxh->get_dir()];

            lpd->set_fif_seen(rxh->m_flow_size);

            m_stats.m_add++;
            m_stats.m_active++;
			m_stats.m_err_open_with_no_fif_pkt++;
            any_err=true;
            lf->m_oo_err++;
        }else{
			m_stats.m_found++;

            CRxCheckFlowPerDir *lpd=&lf->m_dir[rxh->get_dir()];
            if ( !lpd->is_fif_seen() ){
                // init this dir 
                lpd->set_fif_seen(rxh->m_flow_size);

            }else{
                if ( lpd->m_flow_size != rxh->m_flow_size ){
                    m_stats.m_err_flow_length_changed++;
                    lf->m_oo_err++;
                    any_err=true;
                }

                /* check seq number */
                uint16_t c_seq=lpd->m_seq;
                if ((c_seq) != rxh->m_pkt_id) {
                    /* out of order issue */
                    lf->m_oo_err++;
                    any_err=true;

                    if (c_seq-1  == rxh->m_pkt_id) {
                        m_stats.m_err_oo_dup++;
                    }else{
                        if ((c_seq ) < rxh->m_pkt_id ) {
                            m_stats.m_err_oo_late++;
                        }else{
                            m_stats.m_err_oo_early++;
                        }
                    }
                }
                /* reset the seq */
                lpd->m_seq=rxh->m_pkt_id+1;
                lpd->m_pkts++;
            }

        }
    }

    if (any_err) {
        update_template_err(rxh->m_template_id);
    }

    m_tw.restart_timer(&lf->m_aging_timer_handle,m_cur_time+std::max(rxh->m_aging_sec,(uint16_t)5));
    /* teminate flow if needed */
    if ( lf->is_all_pkts_seen() ){
            /* handel from termination */
            m_tw.stop_timer(&lf->m_aging_timer_handle);
            lf->set_aged_correctly();
            on_flow_end(lf);
    }

    if ((m_stats.m_lookup & 0xff)==0) {
        /* handle aging from time to time */
        tw_handle()  ;
    }
}

void RxCheckManager::update_template_err(uint8_t template_id){
    get_template(template_id)->inc_error_counter();
}


bool RxCheckManager::on_flow_end(CRxCheckFlow * lp){
	m_stats.m_remove++;
	m_stats.m_active--;
    if ( !m_on_drain ){
        uint16_t exp=lp->get_total_pkt_expected();
        uint16_t seen=lp->get_total_pkt_seen();

        if (  exp > seen ){
            m_stats.m_err_drop +=(exp - seen);
        }
        if (!lp->is_aged_correctly()) {
            m_stats.m_err_aged++;
        }
    }
	m_ft.remove(lp->m_flow_id);
	return(true);
}



void RxCheckManager::Delete(){
    m_ft.Delete();
    m_hist.Delete();
}

void  flow_aging_callback(CFlowTimerHandle * t){
	CRxCheckFlow * lp = (CRxCheckFlow *)t->m_object;
	RxCheckManager * lpm = (RxCheckManager *)t->m_object1;
	assert(lp);
	assert(lpm);
	assert(t->m_id == 0x1234);
	lpm->on_flow_end(lp);
}

void RxCheckManager::template_dump_json(std::string & json){
    json+="\"template\" : [";
    int i;
    bool is_first=true;
    for (i=0; i<MAX_TEMPLATES_STATS;i++ ) {
        CPerTemplateInfo * lp=get_template(i);
        if ( is_first==true ){
            is_first=false;
        }else{
            json+=",";
        }
        json+="{";
        json+=add_json("id",(uint32_t)i);
        json+=add_json("val",(uint64_t)lp->get_error_counter());
        json+=add_json("rx_pkts",(uint64_t)lp->get_rx_counter());
        json+=add_json("jitter",(uint64_t)lp->get_jitter_usec(),true);
        json+="}";
    }
    json+="],";

}

uint32_t RxCheckManager::getTemplateMaxJitter(){
    uint32_t res=0;
    int i;
    for (i=0; i<MAX_TEMPLATES_STATS;i++ ) {
        CPerTemplateInfo * lp=get_template(i);
        uint32_t jitter=lp->get_jitter_usec();
        if ( jitter > res ) {
            res =jitter;
        }
    }
    return ( res );
}

void RxCheckManager::DumpTemplate(FILE *fd,bool verbose){
    int i;
    bool has_template=false;
    int cnt=0;
    for (i=0; i<MAX_TEMPLATES_STATS;i++ ) {
        CPerTemplateInfo * lp=get_template(i);
        if (verbose || (lp->get_error_counter()>0)) {
            has_template=true;
            if (cnt==0){
                fprintf(fd,"\n");
            }
            fprintf(fd,"[id:%2d val:%8llu,rx:%8llu], ",i, (unsigned long long)lp->get_error_counter(), (unsigned long long)lp->get_rx_counter());
            cnt++;
            if (cnt>5) {
                cnt=0;
            }
        }
    }
    if ( has_template ){
        fprintf(fd,"\n");
    }
}

void RxCheckManager::DumpTemplateFull(FILE *fd){
    int i;
    for (i=0; i<MAX_TEMPLATES_STATS;i++ ) {
        CPerTemplateInfo * lp=get_template(i);
        fprintf(fd," template_id_%2d , errors:%8llu,  jitter: %llu  rx : %llu \n",
                i,
                (unsigned long long)lp->get_error_counter(),
                (unsigned long long)lp->get_jitter_usec(),
                (unsigned long long)lp->get_rx_counter() );
    }
}


void RxCheckManager::DumpShort(FILE *fd){
     m_hist.update();
     fprintf(fd,"------------------------------------------------------------------------------------------------------------\n");
     fprintf(fd,"rx check:  avg/max/jitter latency, %8.0f  ,%8.0f, %8d ",m_hist.get_average_latency(),m_hist.get_max_latency(),getTemplateMaxJitter());
     fprintf(fd,"     | ");
     m_hist.DumpWinMax(fd);
     DumpTemplate(fd,false);
     fprintf(fd,"\n");
     fprintf(fd,"---\n");
     fprintf(fd," active flows: %8llu, fif: %8llu,  drop: %8llu, errors: %8llu \n",
             (unsigned long long)m_stats.m_active,
             (unsigned long long)m_stats.m_fif,
             (unsigned long long)m_stats.m_err_drop,
             (unsigned long long)m_stats.get_total_err());
     fprintf(fd,"------------------------------------------------------------------------------------------------------------\n");

}

void RxCheckManager::Dump(FILE *fd){
	m_stats.Dump(fd);
    m_hist.DumpWinMax(fd);
    m_hist.Dump(fd);
    DumpTemplateFull(fd);
    fprintf(fd," ager :\n");
    m_tw.Dump(fd);
}

void RxCheckManager::dump_json(std::string & json){

    json="{\"name\":\"rx-check\",\"type\":0,\"data\":{";
    m_stats.dump_json(json);
    m_hist.dump_json("latency_hist",json);
    template_dump_json(json);
    m_tw.dump_json(json);
    json+="\"unknown\":0}}" ;
}

