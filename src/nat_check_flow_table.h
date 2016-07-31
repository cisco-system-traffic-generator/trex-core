/*
  Ido Barnea
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

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
#ifndef  NAT_CHECK_FLOW_TABLE_H
#define  NAT_CHECK_FLOW_TABLE_H
#include <map>

class CNatData;
class CNatCheckFlowTableList;

typedef std::map<uint64_t, CNatData *, std::less<uint64_t> > nat_check_flow_map_t;
typedef nat_check_flow_map_t::const_iterator nat_check_flow_map_iter_t;
typedef nat_check_flow_map_t::iterator nat_check_flow_map_iter_no_const_t;

// One element of list and map
class CNatData {
    friend class CNatCheckFlowTableList; // access m_next and m_prev
    friend std::ostream& operator<<(std::ostream& os, const CNatCheckFlowTableList& cf);
    friend std::ostream& operator<<(std::ostream& os, const CNatData &cn);

 public:
    uint32_t get_data() {return m_data;}
    void set_data(uint32_t val) {m_data = val;}
    uint32_t get_key() {return m_key;}
    void set_key(uint32_t val) {m_key = val;}
    double get_timestamp() {return m_timestamp;}
    void set_timestamp(double val) {m_timestamp = val;}
    CNatData() {
        m_next = NULL;
        m_prev = NULL;
    }

 private:
    double m_timestamp;
    uint64_t m_key;
    CNatData *m_prev;
    CNatData *m_next;
    uint32_t m_data;
};

class CNatCheckFlowTableMap   {
    friend class CNatCheckFlowTable;
    friend std::ostream& operator<<(std::ostream& os, const CNatCheckFlowTableMap& cf);

 private:
    void clear(void) {m_map.clear();}
    CNatData *erase(uint64_t key);
    bool find(uint64_t key, uint32_t &val);
    CNatData *insert(uint64_t key, uint32_t val, double time);
    bool verify(uint32_t *arr, int size);
    uint64_t size(void) {return m_map.size();}

 private:
    nat_check_flow_map_t m_map;
};

class CNatCheckFlowTableList   {
    friend class CNatCheckFlowTable;
    friend std::ostream& operator<<(std::ostream& os, const CNatCheckFlowTableList& cf);

 private:
    void dump_short(FILE *fd);
    void erase(CNatData *data);
    CNatData *get_head() {return m_head;}
    void insert(CNatData *data);
    bool verify(uint32_t *arr, int size);

    CNatCheckFlowTableList() {
        m_head = NULL;
        m_tail = NULL;
    }

 private:
    CNatData *m_head;
    CNatData *m_tail;
};

class CNatCheckFlowTable {
    friend std::ostream& operator<<(std::ostream& os, const CNatCheckFlowTable& cf);

 public:
    ~CNatCheckFlowTable();
    void clear_old(double time);
    void dump(FILE *fd);
    bool erase(uint64_t key, uint32_t &val);
    bool insert(uint64_t key, uint32_t val, double time);
    bool test();

 private:
    CNatCheckFlowTableMap m_map;
    CNatCheckFlowTableList m_list;
};

#endif
