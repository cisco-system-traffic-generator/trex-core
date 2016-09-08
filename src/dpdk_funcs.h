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

enum i40e_trex_init_mode_ {
    I40E_TREX_INIT_STL,
    I40E_TREX_INIT_STF,
    I40E_TREX_INIT_RCV_ALL,
};

void i40e_trex_dump_fdir_regs(struct i40e_hw *hw);
void i40e_trex_fdir_reg_init(int port_id, int mode);

#endif
