#ifndef RTE_ATOMIC_STUB_H
#define RTE_ATOMIC_STUB_H
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
/* stubs for DPDK function for simulation */

static inline void rte_mb(void){
}

static inline void rte_wmb(void){
}

static inline void rte_rmb(void){
}

static inline void rte_smp_mb(void){
}

static inline void rte_smp_wmb(void){
}

static inline void rte_smp_rmb(void){
}

static inline void rte_io_mb(void){
}

static inline void rte_io_wmb(void){
}

static inline void rte_io_rmb(void){
}

static inline void rte_compiler_barrier(){
}

/*------------------------- 32 bit atomic operations -------------------------*/


typedef struct {
	volatile int32_t cnt; /**< An internal counter value. */
} rte_atomic32_t;

#define RTE_ATOMIC32_INIT(val) { (val) }

static inline void
rte_atomic32_init(rte_atomic32_t *v)
{
	v->cnt = 0;
}

static inline int32_t
rte_atomic32_read(const rte_atomic32_t *v)
{
	return v->cnt;
}

static inline void
rte_atomic32_set(rte_atomic32_t *v, int32_t new_value)
{
	v->cnt = new_value;
}

static inline void
rte_atomic32_add(rte_atomic32_t *v, int32_t inc)
{
    v->cnt+=inc;
}

static inline void
rte_atomic32_sub(rte_atomic32_t *v, int32_t dec)
{
    v->cnt-=dec;
}

static inline void
rte_atomic32_inc(rte_atomic32_t *v){
    rte_atomic32_add(v, 1);
}


static inline void
rte_atomic32_dec(rte_atomic32_t *v){
    rte_atomic32_sub(v, 1);
}

static inline int32_t
rte_atomic32_add_return(rte_atomic32_t *v, int32_t inc)
{
    int32_t res=v->cnt;
    v->cnt+=inc;
    return(res);
}

#endif 
