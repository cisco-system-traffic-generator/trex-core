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

COsTimeGlobalData  timer_gd;



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






