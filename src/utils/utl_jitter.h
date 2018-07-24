#ifndef  UTL_JITTER_H
#define  UTL_JITTER_H
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

class CJitter {

public:     
   CJitter(){
       reset();
   }

   void reset(){
       m_old_transit=0.0;
       m_jitter=0.0;
   }

   double calc(double transit){
           double d = transit - m_old_transit;
           m_old_transit = transit;
           if ( d < 0){
              d = -d;
           }
           m_jitter +=(1.0/16.0) * ((double)d - m_jitter);
           return(m_jitter);
        }    
   double get_jitter(){
        return (m_jitter);
   }     

private:
  double m_old_transit;
  double m_jitter;
};

class CJitterUint {

public:     
   CJitterUint(){
       reset();
   }

   void reset(){
       m_old_transit=0;
       m_jitter=0;
   }

   void calc(uint32_t transit){
           int32_t d = transit - m_old_transit;
           m_old_transit = transit;
           if ( d < 0){
              d = -d;
           }
           m_jitter += d - ((m_jitter + 8)>>4);
        }    
   uint32_t get_jitter(){
        return (m_jitter>>4);
   }     

private:
  int32_t m_old_transit; /* time usec maximum up to 30msec */
  int32_t m_jitter;
};

#endif
