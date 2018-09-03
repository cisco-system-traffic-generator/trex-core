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
#include "utl_json.h"
#include <stdint.h>
#include <json/json.h>

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

typedef enum { scINFO =0x12, 
               scWARNING = 0x13,
               scERROR = 0x14
               } counter_info_t_;

typedef uint8_t counter_info_t;

class CGSimpleBase {
public:
    CGSimpleBase(){
        m_dump_zero=false;
        m_info=scINFO;
        m_is_abs = false;
    }

    void set_abs(bool is_abs) {
        m_is_abs = is_abs;
    }

    void set_id(uint32_t id){
        m_id=id;
    }
    uint32_t get_id(){
        return (m_id);
    }

    virtual ~CGSimpleBase(){
    }
    virtual void update(){
    }

    virtual std::string get_val_as_str(){
        return("");
    }

    virtual void get_val(Json::Value &obj) {
    }

    virtual void dump_val(FILE *fd){
    }

    void set_info_level(counter_info_t info){
        m_info=info;
    }

    counter_info_t get_info_level(){
        return (m_info);
    }
    std::string get_info_as_str(){
        switch (m_info) {
        case scINFO:
            return ("info");
            break;
        case scWARNING:
            return ("warnig");
            break;
        case scERROR:
            return ("error");
            break;
        }
        return ("unknown");
    }
     
    std::string get_info_as_short_str(){
        switch (m_info) {
        case scINFO:
            return (" ");
            break;
        case scWARNING:
            return ("-");
            break;
        case scERROR:
            return ("*");
            break;
        }
        return ("unknown");
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

    virtual std::string dump_as_json(bool last);

    virtual std::string dump_as_json_desc(bool last){
        return (add_json(m_name, m_help,last));
    }

    virtual Json::Value get_json_desc(void);

    virtual bool is_real(void) {
        return true;
    }

protected:
    uint32_t       m_id;
    counter_info_t m_info;
    bool        m_dump_zero;
    std::string m_help;
    std::string m_name;
    std::string m_units;
    bool        m_is_abs;
};

class CGSimpleBar : public CGSimpleBase {
public:
    virtual void dump_val(FILE *fd){
         fprintf(fd," %15s ","---");
    }
    virtual bool is_real(void) {
        return false;
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

    virtual std::string dump_as_json(bool last){
        return (add_json(m_name, *m_p,last));
    }

    virtual std::string get_val_as_str(){
        return (std::to_string(*m_p));
    }

    virtual void get_val(Json::Value &obj) {
        obj[std::to_string(m_id)] = *m_p;
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

    virtual std::string dump_as_json(bool last){
        return (add_json(m_name, *m_p,last));
    }

    virtual std::string get_val_as_str(){
        return (std::to_string(*m_p));
    }

    virtual void get_val(Json::Value &obj) {
        obj[std::to_string(m_id)] = *m_p;
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

    virtual std::string get_val_as_str(){
        return (std::to_string(*m_p));
    }

    virtual void get_val(Json::Value &obj) {
        obj[std::to_string(m_id)] = *m_p;
    }

    virtual std::string dump_as_json(bool last){
        return (add_json(m_name, *m_p,last));
    }

private:
     double *m_p;
};


/* one Column of counters */

class CGTblClmCounters {
public:

    CGTblClmCounters(){
        m_free_objects=0;
        m_index=0;
    }
    ~CGTblClmCounters();

    /* enable will tell this object to free counters in the vector */
    void set_free_objects_own(bool enable){
        m_free_objects=enable;
    }

    void add_count(CGSimpleBase* cnt){
        cnt->set_id(m_index);
        m_index++;
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
   uint32_t                   m_index;
   bool                       m_free_objects;
   std::string                m_name;
   std::vector<CGSimpleBase*> m_counters;
};


class CTblGCounters {
public:
    CTblGCounters(){
        m_free_objects=false;
        m_epoch = 0;
    }

    ~CTblGCounters();

    /* enable will tell this object to free counters in the vector */
    void set_free_objects_own(bool enable){
        m_free_objects=enable;
    }

    void add(CGTblClmCounters * clm){
        m_counters.push_back(clm);
        verify();
    }

    void dump_table(FILE *fd,bool zeros,bool desc);



    void dump_as_json(std::string name,
                      std::string & json);

    void dump_meta(std::string name,
                   Json::Value & json);

    void dump_values(std::string name,
                     bool zeros,
                     Json::Value & obj);

    void inc_epoch(void);

private:
    void verify();
    bool can_skip_zero(int index);
    bool dump_line(FILE *fd,int index,bool desc);


private:
    uint64_t                   m_epoch;
    bool                       m_free_objects;
    std::vector<CGTblClmCounters*> m_counters;
};



#endif
