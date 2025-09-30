/*
 * Copyright (c) 2021-2021 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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


#ifndef RELEASE_COINTAINERS_H_
#define RELEASE_COINTAINERS_H_

#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <set>

// Release diffrent containters under the same API

template<typename T>
    void release_container_data(T &data) {
    }

template<typename T>
    void release_container_data(T *data) {
         delete data;
    }

template<typename T, typename A>
    void release_container_data(std::vector<T,A> &data) {
         for (typename std::vector<T,A>::iterator it = data.begin(); it != data.end(); ++it)
              release_container_data(*it);

         data.clear();
    }

template<typename T, typename A>
    void release_container_data(std::vector<T,A> *data) {
         if (data)
             release_container_data(*data);
    }

template<typename K, typename V, typename C, typename A>
    void release_container_data(std::map<K,V,C,A> &data) {
         for (typename std::map<K,V,C,A>::iterator it = data.begin(); it != data.end(); ++it) {
             release_container_data(it->first);
             release_container_data(it->second);
         }

         data.clear();
    }

template<typename K, typename V, typename C, typename A>
    void release_container_data(std::map<K,V,C,A> *data) {
         if (data)
             release_container_data(*data);
    }

template<typename T, typename C, typename A>
    void release_container_data(std::set<T,C,A> &data) {
         for (typename std::set<T,C,A>::iterator it = data.begin(); it != data.end(); ++it)
              release_container_data(*it);

         data.clear();
    }

template<typename T, typename C>
    void release_container_data(std::set<T,C> *data) {
         if (data)
             release_container_data(*data);
    }

template<typename T, typename A>
    void release_container_data(std::list<T,A> &data) {
         for (typename std::list<T,A>::iterator it = data.begin(); it != data.end(); ++it)
              release_container_data(*it);

         data.clear();
    }

template<typename T, typename A>
    void release_container_data(std::list<T,A> *data) {
         if (data)
             release_container_data(*data);
    }

#endif /* RELEASE_COINTAINERS_H_ */
