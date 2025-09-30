/*
 * Copyright (c) 2004-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2021-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This software is available to you under the terms of the
 * OpenIB.org BSD license included below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <list>
using namespace std;

template <class Data> class MemoryPool
{
    private:
        list<Data *> m_pool;
        int m_allocated;
    public:
        MemoryPool():m_allocated(0){}

        ~MemoryPool()
        {
            while (!m_pool.empty()) {
                delete m_pool.front();
                m_pool.pop_front();
            }
        }

        Data * alloc()
        {
            Data *p = NULL;
            if (m_pool.empty())
                p = new (std::nothrow) Data();
            else {
                p = m_pool.front();
                m_pool.pop_front();
            }
            if (p != NULL) {
                if (p->init()) {
                    delete p;
                    return NULL;
                }
                m_allocated++;
            }
            return p;
        }

        void free(Data * p)
        {
            if (p) {
                m_pool.push_back(p);
                m_allocated--;
            }
        }

        int allocated() { return m_allocated;}
};

#endif //MEMORY_POOL_H_
