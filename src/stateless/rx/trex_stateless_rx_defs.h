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

#ifndef __TREX_STATELESS_RX_DEFS_H__
#define __TREX_STATELESS_RX_DEFS_H__

/**
 * describes the filter type applied to the RX 
 *  RX_FILTER_MODE_HW - only hardware filtered traffic will
 *  reach the RX core
 *  
 */
typedef enum rx_filter_mode_ {
	RX_FILTER_MODE_HW,
	RX_FILTER_MODE_ALL
} rx_filter_mode_e;

#endif /* __TREX_STATELESS_RX_DEFS_H__ */
