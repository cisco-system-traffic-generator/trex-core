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

#include <assert.h>
#include <stdint.h>
#include <string>
#include <iostream>
#include <new>
#include "nat_check_flow_table.h"
/*
  classes in this file:
  CNatCheckFlowTableList - List implementation
  CNatCheckFlowTableMap - Map implementation
  CNatData - element of data that exists in the map and the list.
  CNatCheckFlowTable - Wrapper class which is the interface to outside.
  New element is always inserted to the list and map.
  If element is removed, it is always removed from list and map together.
  Map is used to lookup elemnt by key.
  List is used to clean up old elements by timestamp. We guarantee the list is always sorted by timestamp,
  since we always insert at the end, with increasing timestamp.
*/

std::ostream& operator<<(std::ostream& os, const CNatData &cn) {
    os << "(" << &cn << ")" << "data:" << cn.m_data << " time:" << cn.m_timestamp;
    os << " prev:" << cn.m_prev << " next:" << cn.m_next;
    return os;
}

// map implementation
CNatData *CNatCheckFlowTableMap::erase(uint64_t key) {
    nat_check_flow_map_iter_no_const_t it = m_map.find(key);

    if (it != m_map.end()) {
        CNatData *val = it->second;
        m_map.erase(it);
        return val;
    }

    return NULL;
}

bool CNatCheckFlowTableMap::find(uint64_t key, uint32_t &val) {
    nat_check_flow_map_iter_t it = m_map.find(key);

    if (it != m_map.end()) {
        CNatData *data = it->second;
        val = data->get_data();
        return true;
    }

    return false;
}

// for testing
bool CNatCheckFlowTableMap::verify(uint32_t *arr, int size) {
    uint32_t val = -1;
    int real_size = 0;

    for (int i = 0; i < size; i++) {
        if (arr[i] == -1)
            continue;
        real_size++;
        find(i, val);
        if (val != arr[i])
            return false;
    }

    if (m_map.size() != real_size) {
        std::cout << "Error: Wrong map size " << m_map.size() << ". Should be " << real_size << std::endl;
        return false;
    }

    return true;
}

CNatData * CNatCheckFlowTableMap::insert(uint64_t key, uint32_t val, double time) {
    CNatData *elem = new CNatData;
    assert(elem);
    elem->set_data(val);
    elem->set_key(key);
    elem->set_timestamp(time);
    std::pair<uint64_t, CNatData *> pair = std::pair<uint64_t, CNatData *>(key, elem);

    if (m_map.insert(pair).second == false) {
        delete elem;
        return NULL;
    } else {
        return elem;
    }
}

std::ostream& operator<<(std::ostream& os, const CNatCheckFlowTableMap& cf) {
    nat_check_flow_map_iter_t it;

    os << "NAT check flow table map:\n";
    for (it = cf.m_map.begin(); it != cf.m_map.end(); it++) {
        CNatData *data = it->second;
        uint32_t key = it->first;
        os << "  " << key << ":" << *data << std::endl;
    }
    return os;
}

void CNatCheckFlowTableList::dump_short(FILE *fd) {
    fprintf(fd, "list:\n");
    // this is not fully safe, since we call it from CP core, and rx core changes the list.
    // It is usefull as a debug function
    if (m_head) {
        fprintf(fd, "  head: time:%f key:%x data:%x\n", m_head->get_timestamp(), m_head->get_key(), m_head->get_data());
        fprintf(fd, "  tail: time:%f key:%x data:%x\n", m_tail->get_timestamp(), m_tail->get_key(), m_tail->get_data());
    }
}

// list implementation
// The list is always sorted by timestamp, since we always insert at the end, with increasing timestamp
void CNatCheckFlowTableList::erase(CNatData *data) {
    if (m_head == data) {
        m_head = data->m_next;
    }
    if (m_tail == data) {
        m_tail = data->m_prev;
    }
    if (data->m_prev) {
        data->m_prev->m_next = data->m_next;
    }
    if (data->m_next) {
        data->m_next->m_prev = data->m_prev;
    }
}

// insert as last element in list
void CNatCheckFlowTableList::insert(CNatData *data) {
    data->m_prev = m_tail;
    data->m_next = NULL;

    if (m_tail != NULL) {
        m_tail->m_next = data;
    } else {
        // if m_tail is NULL m_head is also NULL
        m_head = data;
    }
    m_tail = data;
}

bool CNatCheckFlowTableList::verify(uint32_t *arr, int size) {
    int index = -1;
    CNatData *elem = m_head;
    int count = 0;

    while (index < size - 1) {
        index++;
        if (arr[index] == -1) {
            continue;
        }
        count++;

        if (elem->get_data() != arr[index]) {
            return false;
        }

        if (elem == NULL) {
            std::cout << "Too few items in list. Only " << count << std::endl;
            return false;
        }
        elem = elem->m_next;
    }

    // We expect number of items in list to be like num of val!=-1 in arr
    if (elem != NULL) {
        std::cout << "Too many items in list" << std::endl;
        return false;
    }

    return true;
}

std::ostream& operator<<(std::ostream& os, const CNatCheckFlowTableList& cf) {
    CNatData *it = cf.m_head;

    os << "NAT check flow table list:\n";
    os << "  head:" << cf.m_head << " tail:" << cf.m_tail << std::endl;
    while (it != NULL) {
        os << "  " << *it  << std::endl;
        it = it->m_next;
    }

    return os;
}

// flow table
std::ostream& operator<<(std::ostream& os, const CNatCheckFlowTable& cf) {
    os << "========= Flow table start =========" << std::endl;
    os << cf.m_map;
    os << cf.m_list;
    os << "========= Flow table end =========" << std::endl;

    return os;
}

void CNatCheckFlowTable::clear_old(double time) {
    CNatData *data = m_list.get_head();
    uint32_t val;

    while (data) {
        if (data->get_timestamp() < time) {
            erase(data->get_key(), val);
            data = m_list.get_head();
        } else {
            break;
        }
    }
}

void CNatCheckFlowTable::dump(FILE *fd) {
    fprintf(fd, "map size:%lu\n", m_map.size());
    m_list.dump_short(fd);
}

bool CNatCheckFlowTable::erase(uint64_t key, uint32_t &val) {
    CNatData *data = m_map.erase(key);

    if (!data)
        return false;

    val = data->get_data();
    m_list.erase(data);
    delete data;

    return true;
}

bool CNatCheckFlowTable::insert(uint64_t key, uint32_t val, double time) {
    CNatData *res;

    res = m_map.insert(key, val, time);
    if (!res) {
        return false;
    } else {
        m_list.insert(res);
    }
    return true;
}

CNatCheckFlowTable::~CNatCheckFlowTable() {
    clear_old(UINT64_MAX);
}

bool CNatCheckFlowTable::test() {
    uint32_t size = 100;
    uint32_t arr[size];
    int i;
    uint32_t val;

    for (i = 0; i < size; i++) {
        arr[i] = i+200;
    }

    // insert some elements
    for (i = 0; i < size; i++) {
        val = arr[i];
        assert(insert(i, val, i) == true);
    }

    // insert same elements. should fail
    for (i = 0; i < size; i++) {
        val = arr[i];
        assert(insert(i, val, val) == false);
    }

    // remove element we did not insert
    assert(erase(size, val) == false);

    assert (m_map.verify(arr, size) == true);
    assert (m_list.verify(arr, size) == true);

    // remove even keys
    for (i = 0; i < size; i += 2) {
        assert(erase(i, val) == true);
        assert (val == arr[i]);
        arr[i] = -1;
    }

    assert (m_map.verify(arr, size) == true);
    assert (m_list.verify(arr, size) == true);

    // clear half of the old values (We removed the even already, so 1/4 should be left)
    clear_old(size/2);
    for (i = 0; i < size/2; i++) {
        arr [i] = -1;
    }

    assert (m_map.verify(arr, size) == true);
    assert (m_list.verify(arr, size) == true);

    return true;
}
