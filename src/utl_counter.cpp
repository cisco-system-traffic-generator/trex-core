/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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
#include "utl_counter.h"
#include "utl_dbl_human.h"


void CGSimpleRefCntDouble::dump_val(FILE *fd){
    fprintf(fd," %15s ",double_to_human_str((double)*m_p,m_units.c_str(),KBYE_1000).c_str());
}


CGCountersUtl64 & CGCountersUtl64::operator=(const CGCountersUtl64 &rhs){
    assert(rhs.m_cnt==m_cnt);
    int i;
    for (i=0; i<m_cnt; i++) {
        m_base[i]=rhs.m_base[i];
    }
    return *this;
}


CGCountersUtl64 & CGCountersUtl64::operator=(const CGCountersUtl32 &rhs){
    assert(rhs.m_cnt==m_cnt);
    int i;
    for (i=0; i<m_cnt; i++) {
        m_base[i]=rhs.m_base[i];
    }
    return *this;
}

CGCountersUtl64 & CGCountersUtl64::operator+=(const CGCountersUtl64 &rhs){
    assert(rhs.m_cnt==m_cnt);
    int i;
    for (i=0; i<m_cnt; i++) {
        m_base[i]+=rhs.m_base[i];
    }
    return *this;
}

CGCountersUtl32 & CGCountersUtl32::operator+=(const CGCountersUtl32 &rhs){
    assert(rhs.m_cnt==m_cnt);
    int i;
    for (i=0; i<m_cnt; i++) {
        m_base[i]+=rhs.m_base[i];
    }
    return *this;
}

CTblGCounters::~CTblGCounters(){
    if (m_free_objects){
        int i;
        for (i=0; i<m_counters.size(); i++){
            CGTblClmCounters * lp=m_counters[i];
            delete lp;
        }

    }
    m_counters.clear();
}

CGTblClmCounters::~CGTblClmCounters(){
    if (m_free_objects){
        int i;
        for (i=0; i<m_counters.size(); i++){
            CGSimpleBase* lp=m_counters[i];
            delete lp;
        }
    }
    m_counters.clear();
}

bool  CTblGCounters::can_skip_zero(int index){
    int i;
    uint8_t size = m_counters.size();
    for (i=0; i<size; i++) {
        CGTblClmCounters* lp=m_counters[i];
        CGSimpleBase* lpcnt=lp->get_cnt(index);
        if (!lpcnt->is_skip_zero()){
            return(false);
        }
    }
    return(true);
}

bool  CTblGCounters::dump_line(FILE *fd,int index,bool desc){
    int i;
    uint8_t size = m_counters.size();
    CGTblClmCounters* lp=m_counters[0];
    CGSimpleBase* lpcnt=lp->get_cnt(index);
    fprintf(fd," %20s  |",lpcnt->get_name().c_str());
    for (i=0; i<size; i++) {
        lp=m_counters[i];
        lpcnt=lp->get_cnt(index);
        lpcnt->dump_val(fd);
        fprintf(fd,"  |  ");
    }
    if (desc){
        lp=m_counters[0];
        lpcnt=lp->get_cnt(index);
        fprintf(fd," %s",lpcnt->get_help().c_str());
    }
    fprintf(fd,"\n");
    return(true);
}

void CTblGCounters::dump_table(FILE *fd,bool zeros,bool desc){

    uint8_t size=m_counters.size();
    int i;
    fprintf(fd," %20s  |","");
    for (i=0; i<size; i++) {
        CGTblClmCounters* lp=m_counters[i];
        fprintf(fd," %15s ",lp->get_name().c_str());
        fprintf(fd,"  |  ");
    }
    fprintf(fd,"\n");
    fprintf(fd," -----------------------------------------------------------------------------------------\n");
    CGTblClmCounters* lp=m_counters[0];
    for (i=0; i<lp->get_size(); i++) {
        if ((zeros==true) || (!can_skip_zero(i)) ){
            dump_line(fd,i,desc);
        }
    }
}


void CTblGCounters::verify(){
    int i;
    int size=-1;
    for (i=0;i<m_counters.size(); i++ ) {
        CGTblClmCounters* lp=m_counters[i];
        if (size==-1) {
            size=lp->get_size();
        }else{
            assert(size==lp->get_size());
        }
    }
}



