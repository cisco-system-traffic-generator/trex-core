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


#include "os_time.h"
#include <stdio.h>
hr_time_t start_time;

#ifdef WIN32

#include <windows.h>
uint32_t os_get_time_msec(){
    return (GetTickCount());
}
uint32_t os_get_time_freq(){
    return (1000);
}


typedef union {
		struct {
		uint32_t low;
		uint32_t high;
		} h;
		hr_time_t x;
} timer_hl_t;

//hr_time_t os_get_hr_freq(void);

hr_time_t    os_get_hr_freq(void){
	return (3000000000);
}


hr_time_t os_get_hr_tick_64(void) {
	uint32_t _low,_high;
	__asm {
        mov ecx,0       ;select Counter0        
        
        _emit 0x0F      ;RDPMC - get beginning value of Counter0 to edx:eax
        _emit 0x31
        
        mov _high,edx       ;save beginning value
        mov _low,eax
	}
	
    timer_hl_t x;

    x.h.low  = _low;
    x.h.high = _high;
    
	return x.x;
}

uint32_t os_get_hr_tick_32(void) {
	uint32_t _low,_high;
	__asm {
        mov ecx,0       ;select Counter0        
        
        _emit 0x0F      ;RDPMC - get beginning value of Counter0 to edx:eax
        _emit 0x31
        
        mov _high,edx       ;save beginning value
        mov _low,eax
	}
	return _low;
}

#else


#include <time.h>
#include <sys/times.h>
#include <unistd.h>

// Returns time in milliseconds...
uint32_t SANB_tickGet()
{
    struct tms buffer; // we don't really use that
    clock_t ticks =  times(&buffer);
    return (uint32_t)ticks;
}

// ... so rate is 1000.
int SANB_sysClkRateGet()
{
    int rate = sysconf(_SC_CLK_TCK);
    if (rate == -1) 
    {
        fprintf(stderr,"SANB_sysClkRateGet, sysconf FAILED, something very basic is wrong....!\n");
    }
    return rate;
}

uint32_t os_get_time_msec(){
    return (SANB_tickGet());
}

uint32_t os_get_time_freq(){
    return (SANB_sysClkRateGet());
}



#endif 


