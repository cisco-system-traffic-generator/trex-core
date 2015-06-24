#ifndef GLOBAL_IO_MODE_H
#define GLOBAL_IO_MODE_H
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
#include <stdio.h>

class CTrexGlobalIoMode {
public:
    CTrexGlobalIoMode(){
        m_g_disable_first=false;
    }

    void Reset(){
        m_g_mode=gNORMAL;
        m_pp_mode=ppTABLE;
        m_ap_mode=apENABLE;
        m_l_mode=lENABLE;
        m_rc_mode=rcENABLE;
    }
    /* 0 - disable -> normal 
       h - help -> normal 
       1 - -> normal 
       2 - pp
       3 - ap
       4 - ll
       5 - rx
    */
    enum Chars{
        ccHELP='h',
        ccGDISABLE='d',
        ccGNORAML='n',
        ccGPP='p',
        ccGAP='a',
        ccGL='l',
        ccGRC='r'
    };

    enum CliDumpMode {
      cdDISABLE=0, // no print at all 
      cdNORMAL=1,  // normal 
      cdSHORT =2   // short only all ports info 
    };


    enum Global {
      gDISABLE=0, // no print at all 
      gHELP=1,    // help
      gNORMAL=2   // normal 
    };

    typedef uint8_t Global_t;

    enum PerPortCountersMode {
        ppDISABLE=0, 
        ppTABLE  =1,    
        ppSTANDARD=2,
        ppLAST    =3
    };
    typedef uint8_t PerPortCountersMode_t;

    enum AllPortCountersMode {
        apDISABLE=0, 
        apENABLE =1,    
        apLAST   =2
    };
    typedef uint8_t AllPortCountersMode_t;

    enum LatecnyMode {
        lDISABLE =0, 
        lENABLE  =1,    
        lENABLE_Extended=2,
        lLAST           =3
    };
    typedef uint8_t LatecnyMode_t;

    enum RxCheckMode {
        rcDISABLE  =0, 
        rcENABLE  =1,  
        rcENABLE_Extended=2,
        rcLAST           =3
    };
    typedef uint8_t RxCheckMode_t;

    Global_t                m_g_mode;
    bool                    m_g_disable_first;
    PerPortCountersMode_t   m_pp_mode;
    AllPortCountersMode_t   m_ap_mode;
    LatecnyMode_t           m_l_mode;
    RxCheckMode_t           m_rc_mode;

public:
    void set_mode(CliDumpMode  mode);
    /* return true if we need to terminate */
    bool handle_io_modes();

    void Dump(FILE *fd);
    void DumpHelp(FILE *fd);
};


#endif
