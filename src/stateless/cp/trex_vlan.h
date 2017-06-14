/*
  Itay Marom
  Cisco Systems, Inc.
*/

/*
  Copyright (c) 2016-2016 Cisco Systems, Inc.

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

#ifndef __TREX_VLAN_H__
#define __TREX_VLAN_H__
/**
 * manage VLAN configuration
 * 
 */

class VLANConfig {
    
public:

    /**
     * different VLAN modes
     */
    enum mode_e {
        NONE,
        SINGLE,
        QINQ
    };
    

    VLANConfig() {
        clear_vlan();
    }
    
    /**
     * sets single VLAN tag
     *  
     */
    void set_vlan(uint16_t vlan) {
        m_inner_vlan  = vlan;
        m_outer_vlan  = 0;
        m_mode        = SINGLE;
    }
    

    /**
     * set QinQ VLAN tagging
     */
    void set_vlan(uint16_t inner_vlan, uint16_t outer_vlan) {
        m_inner_vlan  = inner_vlan;
        m_outer_vlan  = outer_vlan;
        m_mode        = QINQ;
    }
    
    
    /**
     * remove any VLAN configuration
     * 
     * @author imarom (6/11/2017)
     */
    void clear_vlan() {
        m_inner_vlan  = 0;
        m_outer_vlan  = 0;
        m_mode        = NONE;
    }
    
    
    /**
     * given a stack of VLAN ids - return true if the configuration 
     * matches 
     * 
     */
    bool in_vlan(const std::vector<uint16_t> &vlan_ids) const {
        switch (m_mode) {
        case NONE:
            return(vlan_ids.size() == 0);

        case SINGLE:
            return( (vlan_ids.size() == 1) && (vlan_ids[0] == m_inner_vlan) );

        case QINQ:
            return( (vlan_ids.size() == 2) && (vlan_ids[0] == m_outer_vlan) && (vlan_ids[1] == m_inner_vlan) );

        default:
            assert(0);
        }
    }
    
    
    /**
     * dump to JSON
     * 
     */
    Json::Value to_json() const {
        Json::Value output;
        switch (m_mode) {
        case NONE:
            output["mode"] = "none";
            break;

        case SINGLE:
            output["mode"] = "single";
            output["vlan"] = m_inner_vlan;
            break;

        case QINQ:
            output["mode"]         = "qinq";
            output["inner_vlan"]   = m_inner_vlan;
            output["outer_vlan"]   = m_outer_vlan;
            break;

        }

        return output;
    }
    
    
public:
    mode_e            m_mode;
    uint16_t          m_inner_vlan;
    uint16_t          m_outer_vlan;
};

#endif /* __TREX_VLAN_H__ */
