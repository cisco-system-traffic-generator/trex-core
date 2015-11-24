#include "rx_check_header.h"
#include <common/basic_utils.h>
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



void CRx_check_header::dump(FILE *fd){

    fprintf(fd,"  time_stamp : %x \n",m_time_stamp);
    uint64_t d = (os_get_hr_tick_32() - m_time_stamp );
    dsec_t dd= ptime_convert_hr_dsec(d);

    fprintf(fd,"  time_stamp : %f  \n",dd);
    fprintf(fd,"  magic      : %x \n",m_magic);
    fprintf(fd,"  pkt_id     : %x \n",m_pkt_id);
    fprintf(fd,"  size       : %x \n",m_flow_size);

    fprintf(fd,"  flow_id    : %lx \n",m_flow_id);
    fprintf(fd,"  flags      : %x \n",m_flags);
}



void CNatOption::dump(FILE *fd){

    fprintf(fd,"  op        : %x \n",get_option_type());
    fprintf(fd,"  ol        : %x \n",get_option_len());
    fprintf(fd,"  thread_id : %x \n",get_thread_id());
    fprintf(fd,"  magic     : %x \n",get_magic());
    fprintf(fd,"  fid       : %x \n",get_fid());
    utl_DumpBuffer(stdout,(void *)&u.m_data[0],8,0);
}

