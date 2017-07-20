#ifndef UTL_COUNTERS_H_1
#define UTL_COUNTERS_H_1

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

#include <stdint.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <assert.h>

class CGCountersUtl64;
class CGCountersUtl32 {

    friend CGCountersUtl64;

public:
    CGCountersUtl32(uint32_t *base,uint32_t cnt){
        m_base=base;
        m_cnt=cnt;
    }

    CGCountersUtl32 & operator+=(const CGCountersUtl32 &rhs);


protected:
      uint32_t *m_base;
      uint32_t  m_cnt;
};



class CGCountersUtl64 {

public:
    CGCountersUtl64(uint64_t *base,uint32_t cnt){
        m_base=base;
        m_cnt=cnt;
    }

    CGCountersUtl64 & operator+=(const CGCountersUtl64 &rhs);

    CGCountersUtl64 & operator=(const CGCountersUtl32 &rhs);

    CGCountersUtl64 & operator=(const CGCountersUtl64 &rhs);
    

private:
      uint64_t *m_base;
      uint32_t  m_cnt;
};


class CGSimpleBase {
public:
    CGSimpleBase(){
        m_dump_zero=false;
    }
    virtual ~CGSimpleBase(){
    }
    virtual void update(){
    }

    virtual void dump_val(FILE *fd){

    }

    void set_name(std::string name){
        m_name=name;
    }
    virtual std::string get_name(){
        return(m_name);
    }

    virtual void set_help(std::string name){
        m_help=name;
    }

    std::string get_help(){
        return(m_help);
    }

    bool is_skip_zero(){
        return( (get_dump_zero()==false) && is_zero_val() );
    }
    virtual bool is_zero_val(){
        return(false);
    }

    void set_dump_zero(bool zero){
        m_dump_zero=zero;
    }

    bool get_dump_zero(){
        return(m_dump_zero);
    }


    void set_units(std::string name){
        m_units=name;
    }

    std::string get_units(){
        return(m_units);
    }


private:
    bool        m_dump_zero;
    std::string m_help;
    std::string m_name;
    std::string m_units;
};

class CGSimpleBar : public CGSimpleBase {
public:
    virtual void dump_val(FILE *fd){
         fprintf(fd," %15s ","---");
    }
};

class CGSimpleRefCnt32 : public CGSimpleBase {
public:
    CGSimpleRefCnt32(uint32_t *p){
        m_p =p;
    }

    virtual bool is_zero_val(){
        return(*m_p==0);
    }

    virtual void dump_val(FILE *fd){
        fprintf(fd," %15lu ",(ulong)*m_p);
    }

private:
     uint32_t *m_p;
};


class CGSimpleRefCnt64 : public CGSimpleBase {
public:
    CGSimpleRefCnt64(uint64_t *p){
        m_p =p;
    }

    virtual bool is_zero_val(){
        return(*m_p==0);
    }

    virtual void dump_val(FILE *fd){
        fprintf(fd," %15lu ",*m_p);
    }

private:
     uint64_t *m_p;
};

class CGSimpleRefCntDouble : public CGSimpleBase {
public:
    CGSimpleRefCntDouble(double *p,
                         std::string units){
        m_p =p;
        m_units=units;
    }

    virtual bool is_zero_val(){
        return(*m_p==0.0);
    }

    virtual void dump_val(FILE *fd);

private:
     double *m_p;
     std::string m_units;
};


/* one Column of counters */

class CGTblClmCounters {
public:
    ~CGTblClmCounters();

    void add_count(CGSimpleBase* cnt){
        m_counters.push_back(cnt);
    }
    void set_name(std::string  name){
        m_name=name;
    }
    std::string get_name(){
        return (m_name.c_str());
    }
    uint32_t get_size(){
        return (m_counters.size());
    }
    CGSimpleBase* get_cnt(int index){
        return(m_counters[index]);
    }
    
private:
   std::string                m_name;
   std::vector<CGSimpleBase*> m_counters;
};


class CTblGCounters {
public:
    CTblGCounters(){
    }

    void add(CGTblClmCounters * clm){
        m_counters.push_back(clm);
        verify();
    }

    void dump_table(FILE *fd,bool zeros,bool desc);

private:
    void verify();
    bool can_skip_zero(int index);
    bool dump_line(FILE *fd,int index,bool desc);


private:
    std::vector<CGTblClmCounters*> m_counters;
};



#endif
