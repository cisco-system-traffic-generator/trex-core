#ifndef TREX_DRIVERS_VIRTUAL_H
#define TREX_DRIVERS_VIRTUAL_H

/*
  TRex team
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

#include "trex_driver_base.h"


// Base for all virtual drivers. No constructor. Should not create object from this type.
class CTRexExtendedDriverVirtBase : public CTRexExtendedDriverBase {
public:
    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, true, true, true, true);
    }
    virtual void update_global_config_fdir(port_cfg_t * cfg) {}

    virtual int get_min_sample_rate(void);
    virtual void get_dpdk_drv_params(CTrexDpdkParams &p);
    virtual void update_configuration(port_cfg_t * cfg);
    virtual int configure_rx_filter_rules(CPhyEthIF * _if);
    virtual int stop_queue(CPhyEthIF * _if, uint16_t q_num);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats)=0;
    virtual void clear_extended_stats(CPhyEthIF * _if);
    virtual int wait_for_stable_link();
    virtual void get_rx_stat_capabilities(uint16_t &flags, uint16_t &num_counters, uint16_t &base_ip_id);

    virtual int set_rcv_all(CPhyEthIF * _if, bool set_on) {return 0;}
    CFlowStatParser *get_flow_stat_parser();
};

class CTRexExtendedDriverVirtio : public CTRexExtendedDriverVirtBase {
public:
    CTRexExtendedDriverVirtio();
    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverVirtio() );
    }
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);

    virtual void update_configuration(port_cfg_t * cfg);

};

class CTRexExtendedDriverVmxnet3 : public CTRexExtendedDriverVirtBase {
public:
    CTRexExtendedDriverVmxnet3();

    static CTRexExtendedDriverBase * create() {
        return ( new CTRexExtendedDriverVmxnet3() );
    }
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void update_configuration(port_cfg_t * cfg);


};

class CTRexExtendedDriverI40evf : public CTRexExtendedDriverVirtBase {
public:
    CTRexExtendedDriverI40evf();
    virtual bool get_extended_stats(CPhyEthIF * _if, CPhyEthIFStats *stats) {
        return get_extended_stats_fixed(_if, stats, 4, 4);
    }
    virtual void update_configuration(port_cfg_t * cfg);
    static CTRexExtendedDriverBase * create() {
        return ( new CTRexExtendedDriverI40evf() );
    }
    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, true, true, false, true);
    }
};

class CTRexExtendedDriverIxgbevf : public CTRexExtendedDriverI40evf {

public:
    CTRexExtendedDriverIxgbevf();
    virtual bool get_extended_stats(CPhyEthIF * _if, CPhyEthIFStats *stats) {
        return get_extended_stats_fixed(_if, stats, 4, 4);
    }

    static CTRexExtendedDriverBase * create() {
        return ( new CTRexExtendedDriverIxgbevf() );
    }
    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, true, true, false, true);
    }
};

class CTRexExtendedDriverBaseE1000 : public CTRexExtendedDriverVirtBase {
    CTRexExtendedDriverBaseE1000();
public:
    static CTRexExtendedDriverBase * create() {
        return ( new CTRexExtendedDriverBaseE1000() );
    }
    // e1000 driver handing us packets with ethernet CRC, so we need to chop them
    virtual void update_configuration(port_cfg_t * cfg);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);

};

class CTRexExtendedDriverAfPacket : public CTRexExtendedDriverVirtBase {
public:
    CTRexExtendedDriverAfPacket();
    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverAfPacket() );
    }
    virtual TRexPortAttr * create_port_attr(tvpid_t tvpid,repid_t repid) {
        return new DpdkTRexPortAttr(tvpid, repid, true, true, true, false);
    }
    virtual void get_dpdk_drv_params(CTrexDpdkParams &p);
    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats);
    virtual void update_configuration(port_cfg_t * cfg);
};



/* wan't verified by us, software mode  */
class CTRexExtendedDriverMlnx4 : public CTRexExtendedDriverVirtBase {
public:
    CTRexExtendedDriverMlnx4();
    static CTRexExtendedDriverBase * create(){
        return ( new CTRexExtendedDriverMlnx4() );
    }

    virtual bool get_extended_stats(CPhyEthIF * _if,CPhyEthIFStats *stats) {
        return get_extended_stats_fixed(_if, stats, 4, 4);
    };

    virtual void update_configuration(port_cfg_t * cfg);
    static std::string mlx4_so_str;
};


#endif /* TREX_DRIVERS_VIRTUAL_H */
