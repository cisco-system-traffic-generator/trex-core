#ifndef OS_TIME_H
#define OS_TIME_H
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
#include <time.h>

typedef uint64_t   hr_time_t; // time in high res tick 
typedef uint32_t   hr_time_32_t; // time in high res tick 
typedef double     dsec_t;    //time in sec double

uint32_t 	 os_get_time_msec();
uint32_t 	 os_get_time_freq();

static inline double
usec_to_sec(double usec) {
    return (usec / (1000 * 1000));
}



#ifdef LINUX

#ifdef RTE_DPDK


//extern "C" uint64_t rte_get_hpet_hz(void);

#include "rte_cycles.h"

static inline hr_time_t    os_get_hr_tick_64(void){
    return (rte_rdtsc());
}

static inline hr_time_32_t os_get_hr_tick_32(void){
    return ( (uint32_t)os_get_hr_tick_64());
}

static inline hr_time_t    os_get_hr_freq(void){
    return (rte_get_tsc_hz() );
}



#else


/* read the rdtsc register for ticks
   works for 64 bit aswell
*/
static inline void  platform_time_get_highres_tick_64(uint64_t* t)
{
    uint32_t lo, hi;
    __asm__ __volatile__ (      // serialize
	  "xorl %%eax,%%eax \n        cpuid"
	  ::: "%rax", "%rbx", "%rcx", "%rdx");
     /* We cannot use "=A", since this would use %rax on x86_64 and return
     only the lower 32bits of the TSC */
     __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    *t = (uint64_t)hi << 32 | lo;
}

static inline uint32_t  platform_time_get_highres_tick_32()
{
    uint64_t t;
    platform_time_get_highres_tick_64(&t);
    return ((uint32_t)t);
}


static inline hr_time_t    os_get_hr_freq(void){
	return (3000000000ULL);
}

static inline hr_time_t os_get_hr_tick_64(void) {
 hr_time_t res;
 platform_time_get_highres_tick_64(&res);
 return (res);
}

static inline uint32_t os_get_hr_tick_32(void) {
	return (platform_time_get_highres_tick_32());
}

#endif

#else

hr_time_t    os_get_hr_tick_64(void);
hr_time_32_t os_get_hr_tick_32(void);
hr_time_t    os_get_hr_freq(void);
#endif


/* convert delta time */
static inline hr_time_t ptime_convert_dsec_hr(dsec_t dsec){
    return((hr_time_t)(dsec*(double)os_get_hr_freq()) );
}

/* convert delta time */
static inline dsec_t  ptime_convert_hr_dsec(hr_time_t hrt){
    return ((dsec_t)((double)hrt/(double)os_get_hr_freq() ));
}


extern  hr_time_t start_time;

static inline void time_init(){
	start_time=os_get_hr_tick_64();
}

/* should be fixed , need to move to high rez tick */
static inline dsec_t now_sec(void){
    hr_time_t d=os_get_hr_tick_64() - start_time;
	return ( ptime_convert_hr_dsec(d) );
}


static inline 
void delay(int msec){

    if (msec == 0) 
    {//user that requested that probebly wanted the minimal delay 
     //but because of scaling problem he have got 0 so we will give the min delay 
     //printf("\n\n\nERROR-Task delay ticks == 0 found in task %s task id = %d\n\n\n\n", 
     //       SANB_TaskName(SANB_TaskIdSelf()), SANB_TaskIdSelf());
     msec =1;

    } 

    struct timespec time1, remain; // 2 sec max delay
    time1.tv_sec=msec/1000;
    time1.tv_nsec=(msec - (time1.tv_sec*1000))*1000000;

    nanosleep(&time1,&remain);
}



#endif
