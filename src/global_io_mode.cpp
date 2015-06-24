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
#include "global_io_mode.h"
#include "utl_term_io.h"
#include <stdlib.h>


void CTrexGlobalIoMode::set_mode(CliDumpMode  mode){
    switch (mode) {
    case  cdDISABLE:
        m_g_mode=gDISABLE;
        m_g_disable_first=false;
        break;
    case  cdNORMAL:
        Reset();
        break;
    case cdSHORT:
        m_g_mode=gNORMAL;
        m_pp_mode=ppDISABLE;
        m_ap_mode=apENABLE;
        m_l_mode=lDISABLE;
        m_rc_mode=rcDISABLE;
        break;
    }
}


bool CTrexGlobalIoMode::handle_io_modes(void){
    int c=utl_termio_try_getch();
    if (c) {
        if (c==3) {
            return true;
        }
        switch (c)  {
        case ccHELP:
            if (m_g_mode==gHELP) {
                m_g_mode=gNORMAL;
            }else{
                m_g_mode=gHELP;
            }
            break;
        case ccGDISABLE:
            if (m_g_mode==gDISABLE) {
                m_g_mode=gNORMAL;
            }else{
                m_g_mode=gDISABLE;
                m_g_disable_first=true;
            }
            break;
        case ccGNORAML:
            Reset();
            break;
        case ccGPP:
            m_g_mode=gNORMAL;
            m_pp_mode++;
            if (m_pp_mode==ppLAST) {
                m_pp_mode = ppDISABLE;
            }
            break;
        case ccGAP:
            m_g_mode=gNORMAL;
            m_ap_mode++;
            if (m_ap_mode == apLAST) {
                m_ap_mode = apDISABLE;
            }
            break;
        case ccGL:
            m_g_mode=gNORMAL;
            m_l_mode++;
            if (m_l_mode == lLAST) {
                m_l_mode = lDISABLE;
            }
            break;
        case ccGRC:
            m_g_mode=gNORMAL;
            m_rc_mode++;
            if (m_rc_mode == rcLAST) {
                m_rc_mode = rcDISABLE;
            }
            break;
        }
    }
    return false;
}

void CTrexGlobalIoMode::Dump(FILE *fd){
    fprintf(fd,"\033[2J");
    fprintf(fd,"\033[2H");
    fprintf(fd," global: %d \n",(int)m_g_mode);
    fprintf(fd," pp    : %d \n",(int)m_pp_mode);
    fprintf(fd," ap    : %d \n",(int)m_ap_mode);
    fprintf(fd," l     : %d \n",(int)m_l_mode);
    fprintf(fd," rc    : %d \n",(int)m_rc_mode);
}

void CTrexGlobalIoMode::DumpHelp(FILE *fd){
        fprintf(fd,"Help for Interactive Commands - Trex \n" );
        fprintf(fd,"  d  : Toggle, Disable all -> Noraml \n");
        fprintf(fd,"  n  : Default mode all in Normal mode \n");
        fprintf(fd,"  h  : Toggle, Help->Normal  \n");
        fprintf(fd,"\n");
        fprintf(fd,"  p  : Per ports    Toggle mode, disable -> table -> normal \n");
        fprintf(fd,"  a  : Global ports Toggle mode, disable -> enable \n");
        fprintf(fd,"  l  : Latency      Toggle mode, disable -> enable -> enhanced  \n");
        fprintf(fd,"  r  : Rx check  Toggle mode, disable -> enable -> enhanced  \n");
        fprintf(fd,"  Press h or 1 to go back to Normal mode \n");
}



