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

    VLANConfig() {
        clear_vlan();
    }
    
    /**
     * sets single VLAN tag
     *  
     */
    void set_vlan(uint16_t vlan) {
        m_tags.clear();
        m_tags.push_back(vlan);
    }
    

    /**
     * set QinQ VLAN tagging
     */
    void set_vlan(uint16_t inner_vlan, uint16_t outer_vlan) {
        m_tags.clear();
        m_tags.push_back(outer_vlan);
        m_tags.push_back(inner_vlan);
    }
    
    
    /**
     * remove any VLAN configuration
     * 
     * @author imarom (6/11/2017)
     */
    void clear_vlan() {
        m_tags.clear();
    }
    
    
    /**
     * given a stack of VLAN ids - return true if the configuration 
     * matches 
     * 
     */
    bool in_vlan(const std::vector<uint16_t> &tags) const {
        /* consider VLAN 0 as empty VLAN */
        
        return ( (m_tags == tags) || ( m_tags.empty() && is_def_vlan(tags)) );
    }
    
    
    /**
     * return true if VLAN tagging represents 
     * the default VLAN
     */
    bool is_def_vlan(const std::vector<uint16_t> &tags) const {
        return ( (tags.size() == 1) && (tags[0] == 0) );
    }
    
    
    /**
     * return the count of the VLAN tags 
     * (0, 1 or 2) 
     * 
     */
    uint8_t count() const {
        return m_tags.size();
    }
    
    
    /**
     * dump to JSON
     * 
     */
    Json::Value to_json() const {
        Json::Value output;
        
        output["tags"] = Json::arrayValue;
        for (uint16_t tag : m_tags) {
            output["tags"].append(tag);
        }
        
        return output;
    }
    
    
public:
    std::vector<uint16_t> m_tags;
};

#endif /* __TREX_VLAN_H__ */
