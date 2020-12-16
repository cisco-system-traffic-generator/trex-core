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
#include <queue>

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
        if (!Reschedule()) {
            return -1;
        }
        return ( CRingSp::Enqueue((void*)obj) );
    }

    int Dequeue(T * & obj){
        return (CRingSp::Dequeue(*((void **)&obj)));
    }

    void Delete(void){
        while (!m_resched_q.empty()) {
            T* obj = m_resched_q.front();
            delete obj;
            m_resched_q.pop();
        }
        CRingSp::Delete();
    }

    bool Reschedule() {
        while(!m_resched_q.empty() && !isFull()) {
            T* obj = m_resched_q.front();
            CRingSp::Enqueue((void*)obj);
            m_resched_q.pop();
        }
        return m_resched_q.empty();
    }

    void SecureEnqueue(T *obj, bool use_resched=false) {
        while (!Reschedule() || isFull()) {
            if (use_resched) {
                m_resched_q.push(obj);
                return;
            }
        }
        CRingSp::Enqueue((void*)obj);
    }

private:
    // message rescheduling when rte_ring is full
    std::queue<T *> m_resched_q;
};


/* A general RTE ring. Can be multi/single consumer and multi/single producer. */
template <class T>
class CTRing {
public:
    /*
     * Dummy Ctor
     */
    CTRing() {
        m_ring = nullptr;
    }

    /**
     * Create a new ring named *name* in memory.
     * The new ring size is set to *count*, which must be a power of
     * two. Water marking is disabled by default. The real usable ring size
     * is *count-1* instead of *count* to differentiate a free ring from an
     * empty ring.
     * @param name
     *   The name of the ring.
     * @param count
     *   The size of the ring (must be a power of 2).
     * @param socket_id
     *   The *socket_id* argument is the socket identifier in case of
     *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
     *   constraint for the reserved zone.
     * @param sp
     *   A single producer enqueues.
     * @param sc
     *   A single consumer dequeues.
     * @return
     *   True if the ring was created successfully. It asserts otherwise.
     */
    bool Create(std::string name, uint16_t count, int socket_id, bool sp, bool sc) {
        unsigned sp_flag = sp & RING_F_SP_ENQ;
        unsigned sc_flag = sc & RING_F_SC_DEQ;
        unsigned flags = sp_flag | sc_flag;
        m_ring = rte_ring_create((char *)name.c_str(), count, socket_id, flags);
        assert (m_ring);
        return true;
    }

    /**
     * Deletes the ring 
     */
    void Delete() {
        // can't free the memory of DPDK, it is from reserve memory
    }

    /**
    * Enqueue one object on a ring.
    * @param obj
    *   A pointer to the object to be added.
    * @return
    *   - true: Success; object enqueued.
    *   - false: Not enough room in the ring to enqueue; no object is enqueued.
    */
    bool Enqueue(T* obj){
        assert(obj);
        return (rte_ring_enqueue(m_ring, (void*)obj) == 0 ? true : false);
    }

    /**
    * Enqueue several objects on a ring.
    * @param obj
    *   A pointer to a table of pointers (objects) to enqueue.
    * @param n
    *   The number of objects to add in the ring from the obj_table.
    * @return 
    *   - true: Success; n objects enqueued.
    *   - false: Not enough room in the ring to enqueue; no object is enqueued.
    */
    bool EnqueueBulk(T** obj, unsigned int n) {
        assert(obj);
        return (rte_ring_enqueue_bulk(m_ring, (void**)obj, n, NULL) == n ? true : false);
    }

    /**
    * Dequeue one object from a ring.
    *
    * @param obj
    *   A pointer to a void * pointer (object) that will be filled.
    * @return
    *   - true: Success; objects dequeued.
    *   - false: Not enough entries in the ring to dequeue; no object is
    *     dequeued.
    */
    int Dequeue(T* & obj){
        return(rte_ring_dequeue(m_ring, (void **)&obj) == 0 ? true : false);
    }

    /**
    * Dequeue several objects on a ring.
    * @param obj
    *   A pointer to a table of pointers (objects) that will be filled.
    * @param n
    *   The number of objects to dequeue from the ring to the obj_table.
    * @return 
    *   - true: Success; n objects dequeued.
    *   - false: Not enough entries in the ring to dequeue; no object is enqueued.
    */
    bool DequeueBulk(T** obj, unsigned int n) {
        assert(obj);
        return (rte_ring_dequeue_bulk(m_ring, (void**)obj, n, NULL) == n ? true : false);
    }

    /**
     * Test if ring is full.
     *
     * @return
     *   - true: The ring is full.
     *   - false: The ring is not full.
     */
    bool isFull() {
        return (rte_ring_full(m_ring) ? true : false);
    }

    /**
     * Test if ring is empty.
     *
     * @return
     *   - true: The ring is empty.
     *   - false: The ring is not empty.
     */
    bool isEmpty() {
        return (rte_ring_empty(m_ring) ? true : false);
    }

    /**
    * Return the number of entries in a ring.
    * @return
    *   The number of entries in the ring.
    */
    uint32_t Size() {
        return rte_ring_count(m_ring);
    }

private:
    struct rte_ring* m_ring;
};



#endif

