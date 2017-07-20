#include "utl_dbl_human.h"
#include <stdint.h>
#include "os_time.h"

static uint8_t human_tbl[]={
    ' ',
    'K',
    'M',
    'G',
    'T'
};

std::string double_to_human_str(double num,
                                std::string units,
                                human_kbyte_t etype){
    double abs_num=num;
    if (num<0.0) {
        abs_num=-num;
    }
    int i=0;
    int max_cnt=sizeof(human_tbl)/sizeof(human_tbl[0]);
    double div =1.0;
    double f=1000.0;
    if (etype ==KBYE_1024){
        f=1024.0;
    }
    while ((abs_num > f ) && (i < max_cnt - 1)){
        abs_num/=f;
        div*=f;
        i++;
    }

    char buf [100];
    sprintf(buf,"%10.2f %c%s",num/div,human_tbl[i],units.c_str());
    std::string res(buf);
    return (res);
}



CBwMeasure::CBwMeasure() {
    reset();
}

void CBwMeasure::reset(void) {
   m_start=false;
   m_last_time_msec=0;
   m_last_bytes=0;
   m_last_result=0.0;
};

double CBwMeasure::calc_MBsec(uint32_t dtime_msec,
                             uint64_t dbytes){
    double rate=0.000008*( (  (double)dbytes*(double)os_get_time_freq())/((double)dtime_msec) );
    return (rate);
}

double CBwMeasure::add(uint64_t size) {
    if ( false == m_start ){
        m_start=true;
        m_last_time_msec = os_get_time_msec() ;
        m_last_bytes=size;
        return (0.0);
    }

    uint32_t ctime=os_get_time_msec();
    if ((ctime - m_last_time_msec) <os_get_time_freq() )  {
        return  (m_last_result);
    }

    uint32_t dtime_msec = ctime-m_last_time_msec;
    uint64_t dbytes     = size - m_last_bytes;

    m_last_time_msec    = ctime;
    m_last_bytes        = size;

    m_last_result= 0.5*calc_MBsec(dtime_msec,dbytes) +0.5*(m_last_result);
    return ( m_last_result );
}


float CPPSMeasure::calc_pps(uint32_t dtime_msec,
                             uint32_t pkts){
    float rate=( (  (float)pkts*(float)os_get_time_freq())/((float)dtime_msec) );
    return (rate);

}


float CPPSMeasure::add(uint64_t pkts){
    if ( false == m_start ){
        m_start=true;
        m_last_time_msec = os_get_time_msec() ;
        m_last_pkts=pkts;
        return (0.0);
    }

    uint32_t ctime=os_get_time_msec();
    if ((ctime - m_last_time_msec) <os_get_time_freq() )  {
        return  (m_last_result);
    }

    uint32_t dtime_msec = ctime-m_last_time_msec;
    uint32_t dpkts     = (pkts - m_last_pkts);

    m_last_time_msec    = ctime;
    m_last_pkts        = pkts;

    m_last_result= 0.5*calc_pps(dtime_msec,dpkts) +0.5*(m_last_result);
    return ( m_last_result );
}



