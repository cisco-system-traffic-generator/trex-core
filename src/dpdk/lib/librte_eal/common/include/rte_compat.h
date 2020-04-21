/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Neil Horman <nhorman@tuxdriver.com>.
 * All rights reserved.
 */

#ifndef _RTE_COMPAT_H_
#define _RTE_COMPAT_H_

#ifndef ALLOW_EXPERIMENTAL_API

#define __rte_experimental \
__attribute__((deprecated("Symbol is not yet part of stable ABI"), \
section(".text.experimental")))

#else

#define __rte_experimental \
__attribute__((section(".text.experimental")))

#endif

#endif /* _RTE_COMPAT_H_ */
