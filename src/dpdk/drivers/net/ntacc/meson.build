# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

if not is_linux
	build = false
	reason = 'only supported on Linux'
endif

if get_option('ntacc_external_buffers') == true
  cflags += '-DUSE_EXTERNAL_BUFFER'
endif

if get_option('ntacc_use_sw_stat') == true
  cflags += '-DUSE_SW_STAT'
endif

deps += ['bus_pci']
sources = files('rte_eth_ntacc.c', 'filter_ntacc.c', 'nt_compat.c')

cmd = run_command('sh', '-c', 'echo $NAPATECH3_PATH')
PATH = cmd.stdout().strip()
INC_VAR = PATH + '/include'
LIB_VAR = '-DNAPATECH3_LIB_PATH="' + PATH + '/lib"'

cflags += LIB_VAR

includes += include_directories(INC_VAR)

allow_experimental_apis = true
