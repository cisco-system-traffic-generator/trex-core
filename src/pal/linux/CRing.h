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
#include <queue> 



typedef std::queue<void *>  my_stl_queue_t;
               
class CRingSp {
public:
    CRingSp(){
        m_queue=0;
    }

    bool Create(std::string name, 
                uint16_t cnt,
                int socket_id){
        m_queue = new my_stl_queue_t();
        assert(m_queue);
        return(true);
    }

    void Delete(void){
        if (m_queue) {
            delete m_queue ;
            m_queue=0;
        }
    }

    int Enqueue(void *obj){
        m_queue->push(obj);
        return (0);
    }

    int Dequeue(void * & obj){
        if ( !m_queue->empty() ){
            obj=  m_queue->front();
            m_queue->pop();
            return (0);
        }else{
            return (1);
        }
    }

    bool isFull(void){
        return (false);
    }

    bool isEmpty(void){
        return ( m_queue->empty() ?true:false); 
    }

private:
    my_stl_queue_t * m_queue;
};

template <class T>
class CTRingSp : public CRingSp {
public:
    int Enqueue(T *obj){
        return ( CRingSp::Enqueue((void*)obj) );
    }

    int Dequeue(T * & obj){
        return (CRingSp::Dequeue(*((void **)&obj)));
    }
};




#endif

