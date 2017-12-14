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
#include "dpdk/drivers/net/i40e/base/virtchnl.h"
#include "dpdk/drivers/net/i40e/i40e_ethdev.h"
#include "dpdk/drivers/bus/pci/rte_bus_pci.h"

#include "dpdk_funcs.h"

typedef uint8_t repid_t; /* DPDK port id  */


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
    
void i40e_trex_fdir_reg_init(repid_t repid, int mode)
{
    struct rte_eth_dev *dev = &rte_eth_devices[repid];
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

// fill stats array with fdir rules match count statistics
// Notice that we read statistics from start to start + len, but we fill the stats are
//  starting from 0 with len values
void
i40e_trex_fdir_stats_get(struct rte_eth_dev *dev, uint32_t *stats, uint32_t start, uint32_t len)
{
    int i;
    struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

    for (i = 0; i < len; i++) {
        stats[i] = I40E_READ_REG(hw, I40E_GLQF_PCNT(i + start));
    }
}

void
i40e_trex_fdir_stats_reset(struct rte_eth_dev *dev, uint32_t *stats, uint32_t start, uint32_t len)
{
    int i;
    struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

    for (i = 0; i < len; i++) {
        if (stats) {
            stats[i] = I40E_READ_REG(hw, I40E_GLQF_PCNT(i + start));
        }
        I40E_WRITE_REG(hw, I40E_GLQF_PCNT(i + start), 0xffffffff);
    }
}

int
i40e_trex_get_fw_ver(struct rte_eth_dev *dev, uint32_t *nvm_ver)
{
    struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

    *nvm_ver = hw->nvm.version;
    return 0;
}


int rte_eth_dev_pci_addr(repid_t repid,char *p,int size){

    struct rte_devargs * lp=rte_eth_devices[repid].device->devargs;
    struct rte_pci_addr *pci_addr = NULL;
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(repid, &dev_info);
    pci_addr = &(dev_info.pci_dev->addr);
    if (pci_addr) {
        rte_pci_device_name(pci_addr,p, size);
        return (0);
    }
    return(-1);
}


// return in stats, statistics starting from start, for len counters.
int
rte_eth_fdir_stats_get(repid_t repid, uint32_t *stats, uint32_t start, uint32_t len)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(repid, -EINVAL);

	dev = &rte_eth_devices[repid];

    // Only xl710 support this
    i40e_trex_fdir_stats_get(dev, stats, start, len);

    return 0;
}

// zero statistics counters, starting from start, for len counters.
int
rte_eth_fdir_stats_reset(repid_t repid, uint32_t *stats, uint32_t start, uint32_t len)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(repid, -EINVAL);

	dev = &rte_eth_devices[repid];

    // Only xl710 support this
    i40e_trex_fdir_stats_reset(dev, stats, start, len);

    return 0;
}

int
rte_eth_get_fw_ver(repid_t repid, uint32_t *version)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(repid, -EINVAL);

	dev = &rte_eth_devices[repid];

    // Only xl710 support this
    return i40e_trex_get_fw_ver(dev, version);
}

