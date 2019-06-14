#ifndef CLOSE_HASH_H
#define CLOSE_HASH_H

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

                    
#include "dlist.h"
#include <string.h>
#include <stdio.h>
#include <string>

struct CCloseHashRec {
  TCGenDList m_list;
};

enum HASH_STATUS {
    hsOK,
    hsERR_INSERT_FULL       = -1,
    hsERR_INSERT_DUPLICATE  = -2,
    hsERR_REMOVE_NO_KEY     = -3
};


template <class K>
class CHashEntry : public TCDListNode {
public:
    K key;
};


class COpenHashCounters {
public:
    uint32_t   m_max_entry_in_row;
    
    uint32_t   m_cmd_find;   
    uint32_t   m_cmd_open;   
    uint32_t   m_cmd_remove; 

    COpenHashCounters(){
        m_max_entry_in_row=0;
        m_cmd_find=0;
        m_cmd_open=0;
        m_cmd_remove=0;
    }

    void UpdateFindEntry(uint32_t iter){
        if ( iter>m_max_entry_in_row ) {
            m_max_entry_in_row=iter;
        }
    }

    void Clear(void){
        memset(this,0,sizeof(COpenHashCounters));
    }
    void Dump(FILE *fd){
        fprintf(fd,"m_max_entry_in_row       : %lu \n",(ulong)m_max_entry_in_row);
        fprintf(fd,"m_cmd_find               : %lu \n",(ulong)m_cmd_find);
        fprintf(fd,"m_cmd_open               : %lu \n",(ulong)m_cmd_open);
        fprintf(fd,"m_cmd_remove             : %lu \n",(ulong)m_cmd_remove);
    }

};


/*
example for hash env 

class HASH_ENV{
  public:
   static uint32_t Hash(const KEY & key);
}
*/

typedef void (*close_hash_on_detach_cb_t)(void *userdata,void  *obh);


template<class KEY>
class CCloseHash {

public:

    typedef CHashEntry<KEY>  hashEntry_t;

    CCloseHash();

    ~CCloseHash();

    bool Create(uint32_t size);
    
    void Delete();
    
    void reset();
public:
    
    HASH_STATUS insert(hashEntry_t * entry,uint32_t hash);

    /* insert without check */
    HASH_STATUS insert_nc(hashEntry_t * entry,uint32_t hash);
    
    HASH_STATUS remove(hashEntry_t * entry);

    HASH_STATUS remove_by_key(const KEY & key,uint32_t hash,hashEntry_t * & entry);

    hashEntry_t * find(const KEY & key,uint32_t hash);

     /* iterate all, detach and call the callback */
    void detach_all(void *userdata,close_hash_on_detach_cb_t cb);


    uint32_t get_hash_size(){
        return m_maxEntries;
    }

    uint32_t get_count(){
        return m_numEntries;
    }

public:
    void Dump(FILE *fd);

private:
    CCloseHashRec *              m_tbl; 
    uint32_t                     m_size;  // size of m_tbl log2
    uint32_t                     m_mask;
    uint32_t                     m_numEntries; // Number of entries in hash
    uint32_t                     m_maxEntries; // Max number of entries allowed
    COpenHashCounters            m_stats;
};



template<class KEY>
CCloseHash<KEY>::CCloseHash(){
    m_size = 0;
    m_mask=0;
    m_tbl=0; 
    m_numEntries=0;
    m_maxEntries=0;
}

template<class KEY>
CCloseHash< KEY>::~CCloseHash(){
}


inline static uint32_t hash_FindPower2(uint32_t number){
    uint32_t  powerOf2=1;
    while(powerOf2 < number){
        if(powerOf2==0x80000000)
            return 0;
        powerOf2<<=1;
    }
    return(powerOf2);
}
    
template<class KEY>
bool CCloseHash<KEY>::Create(uint32_t size){

    m_size = hash_FindPower2(size);
    m_mask=(m_size-1);
    m_tbl = new CCloseHashRec[m_size];
    if (!m_tbl) {
        return false;
    }
    int i;
    for (i=0; i<(int)m_size; i++) {
        CCloseHashRec * lp =&m_tbl[i];
        if ( !lp->m_list.Create() ){
            return(false);
        }
    }
    m_maxEntries = m_size;
    reset();
    return true;
}

template<class KEY>
void CCloseHash<KEY>::Delete(){
    delete []m_tbl;
    m_tbl=0;
    m_size = 0;
    m_mask=0;
    m_numEntries=0;
    m_maxEntries=0;
}



template<class KEY>
HASH_STATUS CCloseHash<KEY>::insert(hashEntry_t * entry,uint32_t hash){

    m_stats.m_cmd_open++;
    uint32_t place = hash & (m_mask);
    CCloseHashRec * lpRec= &m_tbl[place];
    TCGenDListIterator iter(lpRec->m_list);
    hashEntry_t *lp;
    for ( ;(lp=(hashEntry_t *)iter.node()); iter++){
        if (lp->key==entry->key){
            return(hsERR_INSERT_DUPLICATE);
        }
    }
    lpRec->m_list.append(entry);
    m_numEntries++;
    return(hsOK);
}

template<class KEY>
HASH_STATUS CCloseHash<KEY>::insert_nc(hashEntry_t * entry,uint32_t hash){

    m_stats.m_cmd_open++;
    uint32_t place = hash & (m_mask);
    CCloseHashRec * lpRec= &m_tbl[place];
    lpRec->m_list.append(entry);
    m_numEntries++;
    return(hsOK);
}



template<class KEY>
HASH_STATUS  CCloseHash<KEY>::remove(hashEntry_t * entry){
    m_stats.m_cmd_remove++;
    entry->detach();
    m_numEntries--;
    return(hsOK);
}

template<class KEY>
HASH_STATUS CCloseHash<KEY>::remove_by_key(const KEY & key,uint32_t hash,hashEntry_t * & entry){
    m_stats.m_cmd_remove++;
    uint32_t place = hash & (m_mask);
    CCloseHashRec * lpRec= &m_tbl[place];
    TCGenDListIterator iter(lpRec->m_list);
    hashEntry_t *lp;
    for ( ;(lp=(hashEntry_t *)iter.node()); iter++){
        if ( lp->key == key ){
            lp->detach();
            entry=lp;
            m_numEntries--;
            return(hsOK);
        }
    }
    return(hsERR_REMOVE_NO_KEY);
}


template<class KEY>
typename CCloseHash<KEY>::hashEntry_t * CCloseHash<KEY>::find(const KEY & key,uint32_t hash){
    m_stats.m_cmd_find++;
    uint32_t place = hash & (m_mask);
    CCloseHashRec * lpRec= &m_tbl[place];
    TCGenDListIterator iter(lpRec->m_list);
    hashEntry_t *lp;
    int f_itr=0;
    for ( ;(lp=(hashEntry_t *)iter.node()); iter++){
        if ( lp->key == key ){
            m_stats.UpdateFindEntry(f_itr);
            return (lp);
        }
        f_itr++;
    }
    return((hashEntry_t *)0);
}


template<class KEY>
void CCloseHash<KEY>::detach_all(void *userdata,
                                 close_hash_on_detach_cb_t cb){
    for(int i=0;i<m_size;i++){
        TCGenDList * lp=&m_tbl[i].m_list;
        while ( !lp->is_empty() ){
            TCDListNode* obj=lp->remove_head();
            m_numEntries--;
            cb(userdata,(void *)obj);
        }
    }
}



template<class KEY>
void CCloseHash<KEY>::reset(){
    m_stats.Clear();
    for(int i=0;i<m_size;i++){
        m_tbl[i].m_list.Create();
    }
    m_numEntries=0;
}




template<class KEY>
void CCloseHash<KEY>::Dump(FILE *fd){
	fprintf(fd," stats \n");
    fprintf(fd," ---------------------- \n");
    m_stats.Dump(fd);
    fprintf(fd," ---------------------- \n");
    fprintf(fd," size         : %lu \n",(ulong)m_size);
    fprintf(fd," num entries  : %lu \n",(ulong)m_numEntries);
}



#endif

