#ifndef DLIST_HPP
#define DLIST_HPP

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

#include <stdint.h>
#include <assert.h>


struct TCDListNode {
    TCDListNode * m_next;
    TCDListNode * m_prev;

public:

    void reset(){
        m_next = 0;
        m_prev = 0;
    }

    void set_self(){
        m_next = this;
        m_prev = this;
    }

    bool is_self(){
        if (m_next == this) {
            return (true);
        }
        return (false);
    }

     void detach();

     void add_after(TCDListNode* node);

     void add_before(TCDListNode* node);
};

inline void TCDListNode::detach(){
    m_next->m_prev = m_prev;
    m_prev->m_next = m_next;
    m_next=0;
    m_prev=0;
}

inline void TCDListNode::add_after(TCDListNode* node){
    node->m_next = m_next;
    m_next = node;
    node->m_prev = this;
    node->m_next->m_prev = node;
}

inline void TCDListNode::add_before(TCDListNode* node){
    node->m_next = this;
    m_prev->m_next = node;
    node->m_prev = m_prev;
    m_prev = node;
}


struct TCGenDList {

    TCDListNode  m_head;

    public:

    bool Create(){
        m_head.set_self();
        return(true);
    }
    
    inline void append(TCDListNode* node){
        m_head.add_before(node);
    }
    inline void insert_head(TCDListNode* node){
        m_head.add_after(node);
    }
    inline bool is_empty(){
        return (m_head.is_self());
    }

    TCDListNode* remove_head(){
        assert(is_empty()==false);
        TCDListNode* res=m_head.m_next;
        res->detach();
        return(res);
    }

};

class TCGenDListIterator {
protected:
    TCGenDList	* m_l;
    TCDListNode * m_cursor;
    bool          m_end;

public:
    inline TCGenDListIterator(const TCGenDList& l);

    inline void operator = (const TCGenDList& l);

    inline void operator = (const TCGenDListIterator& i);

    inline TCDListNode* node();

    inline TCDListNode* operator ++();

    inline TCDListNode* operator ++(int);

private:

    void eval_end(){
        m_end = (m_cursor == &m_l->m_head) ?true:false;
    }
};

inline TCGenDListIterator::TCGenDListIterator(const TCGenDList& l){
    m_l = (TCGenDList*)&l;
    m_cursor = l.m_head.m_next;
    eval_end();
}

inline void TCGenDListIterator::operator = (const TCGenDList& l){
    m_l = (TCGenDList*)&l;
    m_cursor = l.m_head.m_next;
    eval_end();
}

inline void TCGenDListIterator::operator = (const TCGenDListIterator& i){
    m_l = i.m_l;
    m_cursor = i.m_cursor;
    m_end = i.m_end;
}

inline TCDListNode* TCGenDListIterator::node(){
    return (m_end ? 0: m_cursor );
}

inline TCDListNode* TCGenDListIterator::operator ++ (){
    m_cursor = m_cursor->m_next;
    eval_end();
    return (node());
}

inline TCDListNode* TCGenDListIterator::operator++(int){
    m_cursor = m_cursor->m_next;
    eval_end();
    return (node());
}



#endif  
