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

#ifndef __DPDK_FUNCS__
#define __DPDK_FUNCS__

#ifdef __cplusplus
extern "C" {
#endif

enum i40e_trex_init_mode_ {
    I40E_TREX_INIT_STL,
    I40E_TREX_INIT_STF,
    I40E_TREX_INIT_RCV_ALL,
};

void i40e_trex_dump_fdir_regs(struct i40e_hw *hw);

void i40e_trex_fdir_reg_init(uint8_t repid, int mode);

int i40e_trex_get_pf_id(uint8_t repid, uint8_t *pf_id);

int rte_eth_dev_pci_addr(uint8_t repid,char *p,int size);

int rte_eth_fdir_stats_reset(uint8_t repid, uint32_t *stats, uint32_t start, uint32_t len);

int rte_eth_fdir_stats_get(uint8_t repid, uint32_t *stats, uint32_t start, uint32_t len);

int rte_eth_get_fw_ver(uint8_t repid, uint32_t *version);




#ifdef __cplusplus
}
#endif


#endif
