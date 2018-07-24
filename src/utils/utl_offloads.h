#ifndef UTL_OFFLOADS_H
#define UTL_OFFLOADS_H

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

#include <rte_ethdev.h>

/* Check that requested offloads are supported */

void check_offloads(const struct rte_eth_dev_info *dev_info, const struct rte_eth_conf *m_port_conf);


#endif /* UTL_OFFLOADS_H */
