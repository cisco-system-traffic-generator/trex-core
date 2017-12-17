#ifndef __TIMER_TYPES_
#define __TIMER_TYPES_

/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

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


/* timers types */
typedef enum { ttTCP_FLOW =0x17,
               ttTCP_APP  =0x18,
               ttUDP_FLOW =0x21,
               ttUDP_APP  =0x22,
               ttGen      =0x23

} ctx_time_types_t;


#endif


