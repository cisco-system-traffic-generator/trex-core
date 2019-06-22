#ifndef C_RING_H
#define C_RING_H
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


#include <assert.h>
#include <stdint.h>
#include <string>
#include <rte_config.h>
#include <rte_ring.h>

class CRingSp {
public:
    CRingSp(){
        m_ring=0;
    }

    bool Create(std::string name, 
                uint16_t cnt,
                int socket_id){
        m_ring=rte_ring_create((char *)name.c_str(), 
                               cnt,
				 socket_id, 
                 RING_F_SP_ENQ | RING_F_SC_DEQ);
        assert(m_ring);
        return(true);
    }

    void Delete(void){
        // can't free the memory of DPDK, it is from reserve memory 
    }

    int Enqueue(void *obj){
        assert(obj);
        return (rte_ring_sp_enqueue(m_ring,obj));
    }

    int Dequeue(void * & obj){
        return(rte_ring_mc_dequeue(m_ring,(void **)&obj));
    }

    bool isFull(void){
        return ( rte_ring_full(m_ring)?true:false );
    }

    bool isEmpty(void){
        return ( rte_ring_empty(m_ring)?true:false );
    }

private:
    struct rte_ring * m_ring;
};

template <class T>
class CTRingSp : public CRingSp {
public:
    int Enqueue(T *obj){
        assert(obj);
        return ( CRingSp::Enqueue((void*)obj) );
    }

    int Dequeue(T * & obj){
        return (CRingSp::Dequeue(*((void **)&obj)));
    }
};




#endif

