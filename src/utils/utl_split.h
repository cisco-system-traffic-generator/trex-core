#ifndef UTL_SPLIT_H
#define UTL_SPLIT_H
/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2016 Cisco Systems, Inc.

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


inline uint32_t utl_split_int(uint32_t val,
                              int thread_id,
                              int num_threads){
    assert(thread_id<num_threads);
    uint32_t s=val/num_threads;
    uint32_t m=val%num_threads;
    if (thread_id==0) {
        s+=m;
    }
    if (s==0) {
        s=1;
    }
    return(s);
}


#endif
