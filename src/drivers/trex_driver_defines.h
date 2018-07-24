#ifndef TREX_DRIVER_DEFINES_H
#define TREX_DRIVER_DEFINES_H

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


#define RX_CHECK_MIX_SAMPLE_RATE 8
#define RX_CHECK_MIX_SAMPLE_RATE_1G 2


#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define TX_WTHRESH_1G 1  /**< Default values of TX write-back threshold reg. */
#define TX_PTHRESH_1G 1 /**< Default values of TX prefetch threshold reg. */


#define RX_DESC_NUM_DROP_Q 64
#define RX_DESC_NUM_DATA_Q 4096
#define RX_DESC_NUM_DROP_Q_MLX 8
#define RX_DESC_NUM_DATA_Q_VM 512
#define TX_DESC_NUM 1024


#endif /* TREX_DRIVER_DEFINES_H */
