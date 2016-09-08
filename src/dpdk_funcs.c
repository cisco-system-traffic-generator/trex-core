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

#include <stdint.h>
#include <rte_byteorder.h>
#include <rte_ethdev.h>
#include "dpdk/drivers/net/i40e/base/i40e_register.h"
#include "dpdk/drivers/net/i40e/base/i40e_status.h"
#include "dpdk/drivers/net/i40e/base/i40e_osdep.h"
#include "dpdk/drivers/net/i40e/base/i40e_type.h"
#include "dpdk/drivers/net/i40e/i40e_ethdev.h"
#include "dpdk_funcs.h"

void i40e_trex_dump_fdir_regs(struct i40e_hw *hw)
{
    int reg_nums[] = {31, 33, 34, 35, 41, 43};
    int i;
    uint32_t reg;

    for (i =0; i < sizeof (reg_nums)/sizeof(int); i++) {
	reg = I40E_READ_REG(hw,I40E_PRTQF_FD_INSET(reg_nums[i], 0));
        printf("I40E_PRTQF_FD_INSET(%d, 0): 0x%08x\n", reg_nums[i], reg);
	reg = I40E_READ_REG(hw,I40E_PRTQF_FD_INSET(reg_nums[i], 1));
        printf("I40E_PRTQF_FD_INSET(%d, 1): 0x%08x\n", reg_nums[i], reg);
    }
}
    
void i40e_trex_fdir_reg_init(int port_id, int mode)
{
    struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
    
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(12), 0x00000062);
	I40E_WRITE_REG(hw, I40E_GLQF_PIT(2), 0x000024A0);
	I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_UDP, 0), 0);
	I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_TCP, 0), 0);
	I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER, 0), 0);
	I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_UDP, 0), 0);
	I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_TCP, 0), 0);
    I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER, 0), 0);
    switch(mode) {
    case I40E_TREX_INIT_STL:
        // stateless - filter according to IP id or IPv6 identification
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_UDP, 1), 0x00100000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_TCP, 1), 0x00100000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER, 1), 0x00100000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_UDP, 1),   0x0000000000200000ULL);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_TCP, 1),   0x0000000000200000ULL);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER, 1), 0x0000000000200000ULL);
        break;
    case I40E_TREX_INIT_RCV_ALL:
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_UDP, 1), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_TCP, 1), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER, 1), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_UDP, 1), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_TCP, 1), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER, 1), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_SCTP, 0), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_SCTP, 1), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_SCTP, 0), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_SCTP, 1), 0);
        break;
    case I40E_TREX_INIT_STF:
    default:
        // stateful - Filter according to TTL or IPv6 hop limit
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_UDP, 1), 0x00040000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_TCP, 1), 0x00040000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER, 1), 0x00040000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_UDP, 1), 0x00080000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_TCP, 1), 0x00080000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER, 1), 0x00080000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_SCTP, 0), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV4_SCTP, 1), 0x00040000);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_SCTP, 0), 0);
        I40E_WRITE_REG(hw, I40E_PRTQF_FD_INSET(I40E_FILTER_PCTYPE_NONF_IPV6_SCTP, 1), 0x00080000);
        break;
    }
	I40E_WRITE_REG(hw, I40E_GLQF_FD_MSK(0, 34), 0x000DFF00);
	I40E_WRITE_REG(hw, I40E_GLQF_FD_MSK(0,44), 0x000C00FF);
	I40E_WRITE_FLUSH(hw);
}
