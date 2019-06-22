#include "utl_policer.h"


bool CPolicer::update(double dsize,double now_sec){
    if ( m_last_time ==0.0 ) {
        /* first time */
        m_last_time = now_sec;
        return (true);
    }
    if (m_cir == 0.0) {
        return (false);
    }

        // check if there is a need to add tokens
    if(now_sec > m_last_time) {
        dsec_t dtime=(now_sec - m_last_time);
        dsec_t dsize =dtime*m_cir;
        m_level +=dsize;
        if (m_level > m_bucket_size) {
            m_level = m_bucket_size;
        }
        m_last_time = now_sec;
    }

    if (m_level > dsize) {
        m_level -= dsize;
        return (true);
    }else{
        return (false);
    }
}



