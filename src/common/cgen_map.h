#ifndef C_GEN_MAP
#define C_GEN_MAP
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


#include <stdint.h>
#include <map>
#include <string>

template<class KEY, class VAL>
class CGenericMap   {
public:
    typedef std::map<KEY, VAL*,  std::less<KEY> > gen_map_t;
    typename gen_map_t::iterator gen_map_iter_t;
    typedef void (free_map_object_func_t)(VAL *p);


    bool Create(void){
        return(true);
    }
    void Delete(){
        //remove_all();
    }
    VAL * remove(KEY  key ){
        VAL *lp = lookup(key);
        if ( lp ) {
            m_map.erase(key);
            return (lp);
        }else{
            return(0);
        }
    }


    void remove_no_lookup(KEY  key ){
         m_map.erase(key);
    }


    VAL * lookup(KEY  key ){
        typename gen_map_t::iterator iter;
        iter = m_map.find(key);
        if (iter != m_map.end() ) {
            return ( (*iter).second );
        }else{
            return (( VAL*)0);
        }
    }
    void add(KEY  key,VAL * val){
        m_map.insert(typename gen_map_t::value_type(key,val));
    }


    void remove_all(free_map_object_func_t func){
        if ( m_map.empty() ) 
            return;

        typename gen_map_t::iterator it;
        for (it= m_map.begin(); it != m_map.end(); ++it) {
            VAL *lp = it->second;
            func(lp);
        }
        m_map.clear();
    }

    void dump_all(FILE *fd){
        typename gen_map_t::iterator it;
        for (it= m_map.begin(); it != m_map.end(); ++it) {
            VAL *lp = it->second;
            lp->Dump(fd);
        }
    }

    uint64_t count(void){
        return ( m_map.size());
    }

public:
    gen_map_t  m_map;
};

#endif
