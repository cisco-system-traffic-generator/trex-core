#!/usr/bin/env python
# encoding: utf-8
#
# Hanoh Haim
# Cisco Systems, Inc.

VERSION='0.0.1'
APPNAME='cxx_test'
import os
import shutil
import copy
import re
import uuid
import subprocess
import platform
from waflib import Logs
from waflib.Configure import conf
from waflib import Build
import sys
import compile_bird

from distutils.version import StrictVersion

# use hostname as part of cache filename
Build.CACHE_SUFFIX = '_%s_cache.py' % platform.node()

# these variables are mandatory ('/' are converted automatically)
top = '../'
out = 'build_dpdk'
march = os.uname()[4]


b_path ="./build/linux_dpdk/"

so_path = '../scripts/so/' + march
bird_path = '../scripts/bird'

C_VER_FILE      = "version.c"
H_VER_FILE      = "version.h"

H_DPDK_CONFIG   = "dpdk_config.h"

BUILD_NUM_FILE  = "../VERSION"
USERS_ALLOWED_TO_RELEASE = ['hhaim']

clang_flags = ['-Wno-null-conversion',
               '-Wno-sign-compare',
               '-Wno-strict-aliasing',
               '-Wno-address-of-packed-member',
               '-Wno-inconsistent-missing-override',
               '-Wno-deprecated',
               '-Wno-unused-private-field',
               '-Wno-unused-value',
               '-Wno-expansion-to-defined',
               '-Wno-pointer-sign',
               '-Wno-macro-redefined']

gcc_flags = ['-Wall',
             '-Werror',
             '-Wno-literal-suffix',
             '-Wno-sign-compare',
             '-Wno-strict-aliasing',
             '-Wno-address-of-packed-member']



REQUIRED_CC_VERSION = "4.7.0"
SANITIZE_CC_VERSION = "4.9.0"

GCC6_DIRS = ['/usr/local/gcc-6.2/bin', '/opt/rh/devtoolset-6/root/usr/bin']
GCC7_DIRS = ['/usr/local/gcc-7.4/bin', '/opt/rh/devtoolset-7/root/usr/bin']
GCC8_DIRS = ['/usr/local/gcc-8.3/bin']

MAX_PKG_SIZE = 290 # MB


#######################################
# utility for group source code
###################################

orig_system = os.system

def verify_system(cmd):
    ret = orig_system(cmd)
    if ret:
        raise Exception('Return code %s on command: system("%s")' % (ret, cmd))

os.system = verify_system


class SrcGroup:
    ' group of source by directory '

    def __init__(self,dir,src_list):
      self.dir = dir
      self.src_list = src_list
      self.group_list = None
      assert (type(src_list)==list)
      assert (type(dir)==str)



    def file_list (self,top):
        ' return  the long list of the files '
        res=''
        for file in self.src_list:
            res= res + top+'/'+self.dir+'/'+file+'  '

        return res

    def __str__ (self):
        return (self.file_list(''))

    def __repr__ (self):
        return (self.file_list(''))



class SrcGroups:
    ' group of source groups '

    def __init__(self,list_group):
      self.list_group = list_group
      assert (type(list_group)==list)


    def file_list (self,top):
          ' return  the long list of the files '
          res=''
          for o in self.list_group:
              res += o.file_list(top)
          return res

    def __str__ (self):
          return (self.file_list(''))

    def __repr__ (self):
          return (self.file_list(''))


def options(opt):
    opt.load('compiler_cxx')
    opt.load('compiler_c')
    opt.add_option('--pkg-dir', '--pkg_dir', dest='pkg_dir', default=False, action='store', help="Destination folder for 'pkg' option.")
    opt.add_option('--pkg-file', '--pkg_file', dest='pkg_file', default=False, action='store', help="Destination filename for 'pkg' option.")
    opt.add_option('--publish-commit', '--publish_commit', dest='publish_commit', default=False, action='store', help="Specify commit id for 'publish_both' option (Please make sure it's good!)")
    opt.add_option('--no-bnxt', dest='no_bnxt', default=False, action='store_true', help="don't use bnxt dpdk driver. use with ./b configure --no-bnxt. no need to run build with it")
    opt.add_option('--no-mlx', dest='no_mlx', default=(True if march == 'aarch64' else False), action='store', help="don't use mlx4/mlx5 dpdk driver. use with ./b configure --no-mlx. no need to run build with it")
    opt.add_option('--with-ntacc', dest='with_ntacc', default=False, action='store_true', help="Use Napatech dpdk driver. Use with ./b configure --with-ntacc.")    
    opt.add_option('--with-bird', default=False, action='store_true', help="Build Bird server. Use with ./b configure --with-bird.")
    opt.add_option('--new-memory', default=False, action='store_true', help="Build by new DPDK memory subsystem.")
    opt.add_option('--no-ver', action = 'store_true', help = "Don't update version file.")
    opt.add_option('--no-old', action = 'store_true', help = "Don't build old targets.")
    opt.add_option('--private', dest='private', action = 'store_true', help = "private publish, do not replace latest/be_latest image with this image")
    opt.add_option('--tap', dest='tap', default=False, action = 'store_true', help = "Add tap dpdk driver for Azure use-cases")
    opt.add_option('--no-ofed-check', dest='no_ofed_check', default=False, action = 'store_true', help = "Skip ofed check in case of Azure")


    co = opt.option_groups['configure options']
    co.add_option('--sanitized', dest='sanitized', default=False, action='store_true',
                   help='for GCC {0}+ use address sanitizer to catch memory errors'.format(SANITIZE_CC_VERSION))
    
    co.add_option('--gcc6', dest='gcc6', default=False, action='store_true',
                   help='use GCC 6.2 instead of the machine version')

    co.add_option('--gcc7', dest='gcc7', default=False, action='store_true',
                   help='use GCC 7.4 instead of the machine version')

    co.add_option('--gcc8', dest='gcc8', default=False, action='store_true',
                   help='use GCC 8.3 instead of the machine version')


def check_ibverbs_deps(bld):
    if 'LDD' not in bld.env or not len(bld.env['LDD']):
        bld.fatal('Please run configure. Missing key LDD.')
    cmd = '%s %s/external_libs/ibverbs/%s/libibverbs.so' % (bld.env['LDD'][0], top, march)
    ret, out = getstatusoutput(cmd)
    if ret or not out:
        bld.fatal("Command of checking libraries '%s' failed.\nReturn status: %s\nOutput: %s" % (cmd, ret, out))
    if '=> not found' in out:
        Logs.pprint('YELLOW', 'Could not find dependency libraries of libibverbs.so:')
        for line in out.splitlines():
            if '=> not found' in line:
                Logs.pprint('YELLOW', line)
        dumy_libs_path = os.path.abspath(top + 'scripts/dumy_libs')
        Logs.pprint('YELLOW', 'Adding rpath of %s' % dumy_libs_path)
        rpath_linkage.append(dumy_libs_path)


def missing_pkg_msg(fedora, ubuntu):
    msg = 'not found\n'
    fedora_install = 'Fedora/CentOS install:\nsudo yum install %s\n' % fedora
    ubuntu_install = 'Ubuntu install:\nsudo apt install %s\n' % ubuntu
    unknown_install = 'Could not determine Linux distribution.\n%s\n%s' % (fedora_install, ubuntu_install)
    try:
        dist = platform.linux_distribution(full_distribution_name=False)[0].capitalize()
    except:
        return msg + unknown_install

    if dist == 'Ubuntu':
        msg += ubuntu_install
    elif dist in ('Fedora', 'Centos'):
        msg += fedora_install
    else:
        msg += unknown_install
    return msg


@conf
def configure_dummy_mlx5 (ctx):
    ctx.start_msg('Configuring dummy MLX5 autoconf')
    autoconf_file = 'src/dpdk/drivers/common/mlx5/mlx5_autoconf.h'
    autoconf_path = os.path.join(top, autoconf_file)
    os.system('rm -rf %s' % autoconf_path)
    dummy_file_data = '''
#ifndef HAVE_IBV_MLX5_MOD_SWP
#define HAVE_IBV_MLX5_MOD_SWP 1
#endif /* HAVE_IBV_MLX5_MOD_SWP */

/* HAVE_IBV_DEVICE_COUNTERS_SET_V42 is not defined. */

#ifndef HAVE_IBV_DEVICE_COUNTERS_SET_V45
#define HAVE_IBV_DEVICE_COUNTERS_SET_V45 1
#endif /* HAVE_IBV_DEVICE_COUNTERS_SET_V45 */

#ifndef HAVE_MLX5DV_DEVX_UAR_OFFSET
#define HAVE_MLX5DV_DEVX_UAR_OFFSET 1
#endif /* HAVE_MLX5DV_DEVX_UAR_OFFSET */

#ifndef HAVE_IBV_RELAXED_ORDERING
#define HAVE_IBV_RELAXED_ORDERING 1
#endif /* HAVE_IBV_RELAXED_ORDERING */

#ifndef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
#define HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT 1
#endif /* HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT */

#ifndef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
#define HAVE_IBV_DEVICE_TUNNEL_SUPPORT 1
#endif /* HAVE_IBV_DEVICE_TUNNEL_SUPPORT */

#ifndef HAVE_IBV_MLX5_MOD_MPW
#define HAVE_IBV_MLX5_MOD_MPW 1
#endif /* HAVE_IBV_MLX5_MOD_MPW */

#ifndef HAVE_IBV_MLX5_MOD_CQE_128B_COMP
#define HAVE_IBV_MLX5_MOD_CQE_128B_COMP 1
#endif /* HAVE_IBV_MLX5_MOD_CQE_128B_COMP */

#ifndef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
#define HAVE_IBV_MLX5_MOD_CQE_128B_PAD 1
#endif /* HAVE_IBV_MLX5_MOD_CQE_128B_PAD */

#ifndef HAVE_IBV_FLOW_DV_SUPPORT
#define HAVE_IBV_FLOW_DV_SUPPORT 1
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */

#ifndef HAVE_IBV_DEVICE_MPLS_SUPPORT
#define HAVE_IBV_DEVICE_MPLS_SUPPORT 1
#endif /* HAVE_IBV_DEVICE_MPLS_SUPPORT */

#ifndef HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING
#define HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING 1
#endif /* HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING */

/* HAVE_IBV_WQ_FLAG_RX_END_PADDING is not defined. */

#ifndef HAVE_MLX5DV_DR_DEVX_PORT
#define HAVE_MLX5DV_DR_DEVX_PORT 1
#endif /* HAVE_MLX5DV_DR_DEVX_PORT */

#ifndef HAVE_IBV_DEVX_OBJ
#define HAVE_IBV_DEVX_OBJ 1
#endif /* HAVE_IBV_DEVX_OBJ */

#ifndef HAVE_IBV_FLOW_DEVX_COUNTERS
#define HAVE_IBV_FLOW_DEVX_COUNTERS 1
#endif /* HAVE_IBV_FLOW_DEVX_COUNTERS */

#ifndef HAVE_MLX5_DR_CREATE_ACTION_DEFAULT_MISS
#define HAVE_MLX5_DR_CREATE_ACTION_DEFAULT_MISS 1
#endif /* HAVE_MLX5_DR_CREATE_ACTION_DEFAULT_MISS */

#ifndef HAVE_IBV_DEVX_ASYNC
#define HAVE_IBV_DEVX_ASYNC 1
#endif /* HAVE_IBV_DEVX_ASYNC */

#ifndef HAVE_IBV_DEVX_QP
#define HAVE_IBV_DEVX_QP 1
#endif /* HAVE_IBV_DEVX_QP */

#ifndef HAVE_MLX5DV_PP_ALLOC
#define HAVE_MLX5DV_PP_ALLOC 1
#endif /* HAVE_MLX5DV_PP_ALLOC */

#ifndef HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR
#define HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR 1
#endif /* HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR */

#ifndef HAVE_IBV_DEVX_EVENT
#define HAVE_IBV_DEVX_EVENT 1
#endif /* HAVE_IBV_DEVX_EVENT */

#ifndef HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER
#define HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER 1
#endif /* HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER */

#ifndef HAVE_MLX5DV_MMAP_GET_NC_PAGES_CMD
#define HAVE_MLX5DV_MMAP_GET_NC_PAGES_CMD 1
#endif /* HAVE_MLX5DV_MMAP_GET_NC_PAGES_CMD */

#ifndef HAVE_MLX5DV_DR
#define HAVE_MLX5DV_DR 1
#endif /* HAVE_MLX5DV_DR */

#ifndef HAVE_MLX5DV_DR_ESWITCH
#define HAVE_MLX5DV_DR_ESWITCH 1
#endif /* HAVE_MLX5DV_DR_ESWITCH */

#ifndef HAVE_MLX5DV_DR_VLAN
#define HAVE_MLX5DV_DR_VLAN 1
#endif /* HAVE_MLX5DV_DR_VLAN */

#ifndef HAVE_IBV_VAR
#define HAVE_IBV_VAR 1
#endif /* HAVE_IBV_VAR */

/* HAVE_MLX5_OPCODE_ENHANCED_MPSW is not defined. */

/* HAVE_MLX5_OPCODE_SEND_EN is not defined. */

/* HAVE_MLX5_OPCODE_WAIT is not defined. */

/* HAVE_MLX5_OPCODE_ACCESS_ASO is not defined. */

#ifndef HAVE_SUPPORTED_40000baseKR4_Full
#define HAVE_SUPPORTED_40000baseKR4_Full 1
#endif /* HAVE_SUPPORTED_40000baseKR4_Full */

#ifndef HAVE_SUPPORTED_40000baseCR4_Full
#define HAVE_SUPPORTED_40000baseCR4_Full 1
#endif /* HAVE_SUPPORTED_40000baseCR4_Full */

#ifndef HAVE_SUPPORTED_40000baseSR4_Full
#define HAVE_SUPPORTED_40000baseSR4_Full 1
#endif /* HAVE_SUPPORTED_40000baseSR4_Full */

#ifndef HAVE_SUPPORTED_40000baseLR4_Full
#define HAVE_SUPPORTED_40000baseLR4_Full 1
#endif /* HAVE_SUPPORTED_40000baseLR4_Full */

#ifndef HAVE_SUPPORTED_56000baseKR4_Full
#define HAVE_SUPPORTED_56000baseKR4_Full 1
#endif /* HAVE_SUPPORTED_56000baseKR4_Full */

#ifndef HAVE_SUPPORTED_56000baseCR4_Full
#define HAVE_SUPPORTED_56000baseCR4_Full 1
#endif /* HAVE_SUPPORTED_56000baseCR4_Full */

#ifndef HAVE_SUPPORTED_56000baseSR4_Full
#define HAVE_SUPPORTED_56000baseSR4_Full 1
#endif /* HAVE_SUPPORTED_56000baseSR4_Full */

#ifndef HAVE_SUPPORTED_56000baseLR4_Full
#define HAVE_SUPPORTED_56000baseLR4_Full 1
#endif /* HAVE_SUPPORTED_56000baseLR4_Full */

#ifndef HAVE_ETHTOOL_LINK_MODE_25G
#define HAVE_ETHTOOL_LINK_MODE_25G 1
#endif /* HAVE_ETHTOOL_LINK_MODE_25G */

#ifndef HAVE_ETHTOOL_LINK_MODE_50G
#define HAVE_ETHTOOL_LINK_MODE_50G 1
#endif /* HAVE_ETHTOOL_LINK_MODE_50G */

#ifndef HAVE_ETHTOOL_LINK_MODE_100G
#define HAVE_ETHTOOL_LINK_MODE_100G 1
#endif /* HAVE_ETHTOOL_LINK_MODE_100G */

#ifndef HAVE_IFLA_NUM_VF
#define HAVE_IFLA_NUM_VF 1
#endif /* HAVE_IFLA_NUM_VF */

#ifndef HAVE_IFLA_EXT_MASK
#define HAVE_IFLA_EXT_MASK 1
#endif /* HAVE_IFLA_EXT_MASK */

#ifndef HAVE_IFLA_PHYS_SWITCH_ID
#define HAVE_IFLA_PHYS_SWITCH_ID 1
#endif /* HAVE_IFLA_PHYS_SWITCH_ID */

#ifndef HAVE_IFLA_PHYS_PORT_NAME
#define HAVE_IFLA_PHYS_PORT_NAME 1
#endif /* HAVE_IFLA_PHYS_PORT_NAME */

#ifndef HAVE_RDMA_NL_NLDEV
#define HAVE_RDMA_NL_NLDEV 1
#endif /* HAVE_RDMA_NL_NLDEV */

#ifndef HAVE_RDMA_NLDEV_CMD_GET
#define HAVE_RDMA_NLDEV_CMD_GET 1
#endif /* HAVE_RDMA_NLDEV_CMD_GET */

#ifndef HAVE_RDMA_NLDEV_CMD_PORT_GET
#define HAVE_RDMA_NLDEV_CMD_PORT_GET 1
#endif /* HAVE_RDMA_NLDEV_CMD_PORT_GET */

#ifndef HAVE_RDMA_NLDEV_ATTR_DEV_INDEX
#define HAVE_RDMA_NLDEV_ATTR_DEV_INDEX 1
#endif /* HAVE_RDMA_NLDEV_ATTR_DEV_INDEX */

#ifndef HAVE_RDMA_NLDEV_ATTR_DEV_NAME
#define HAVE_RDMA_NLDEV_ATTR_DEV_NAME 1
#endif /* HAVE_RDMA_NLDEV_ATTR_DEV_NAME */

#ifndef HAVE_RDMA_NLDEV_ATTR_PORT_INDEX
#define HAVE_RDMA_NLDEV_ATTR_PORT_INDEX 1
#endif /* HAVE_RDMA_NLDEV_ATTR_PORT_INDEX */

#ifndef HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX
#define HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX 1
#endif /* HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX */

#ifndef HAVE_MLX5_DR_FLOW_DUMP
#define HAVE_MLX5_DR_FLOW_DUMP 1
#endif /* HAVE_MLX5_DR_FLOW_DUMP */

#ifndef HAVE_DEVLINK
#define HAVE_DEVLINK 1
#endif /* HAVE_DEVLINK */

#ifndef HAVE_DEVLINK_DUMMY
#define HAVE_DEVLINK_DUMMY 1
#endif /* HAVE_DEVLINK_DUMMY */


#ifndef HAVE_MLX5_DR_CREATE_ACTION_ASO
#define HAVE_MLX5_DR_CREATE_ACTION_ASO 1
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO */

#ifndef HAVE_INFINIBAND_VERBS_H
#define HAVE_INFINIBAND_VERBS_H 1
#endif /* HAVE_INFINIBAND_VERBS_H */

#ifndef HAVE_MLX5_DR_CREATE_ACTION_DEST_ARRAY
#define HAVE_MLX5_DR_CREATE_ACTION_DEST_ARRAY 1
#endif /* HAVE_MLX5_DR_CREATE_ACTION_DEST_ARRAY */

#ifndef HAVE_MLX5DV_DR_MEM_RECLAIM
#define HAVE_MLX5DV_DR_MEM_RECLAIM 1
#endif /* HAVE_MLX5DV_DR_MEM_RECLAIM */

#ifndef HAVE_MLX5_DR_CREATE_ACTION_FLOW_SAMPLE
#define HAVE_MLX5_DR_CREATE_ACTION_FLOW_SAMPLE 1
#endif /* HAVE_MLX5_DR_CREATE_ACTION_FLOW_SAMPLE */


        #define ETHTOOL_LINK_MODE_25000baseCR_Full_BIT 31
        #define ETHTOOL_LINK_MODE_25000baseKR_Full_BIT 32
        #define ETHTOOL_LINK_MODE_25000baseSR_Full_BIT 33
        #define ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT 34
        #define ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT 35
        #define ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT 36
        #define ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT 37
        #define ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT  38 
        #define ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT 39


        '''
    
    f = open(autoconf_path, "w")
    f.write(dummy_file_data)
    f.close()

    ctx.end_msg('done', 'GREEN')


@conf
def configure_tap (ctx):
    ctx.start_msg('Configuring tap autoconf')
    autoconf_file = 'src/dpdk/drivers/net/tap/tap_autoconf.h'
    autoconf_path = os.path.join(top, autoconf_file)
    os.system('rm -rf %s' % autoconf_path)
    has_sym_args = [
        [ 'HAVE_TC_FLOWER', 'linux/pkt_cls.h',
        'enum', 'TCA_FLOWER_UNSPEC' ],

        [ 'HAVE_TC_VLAN_ID', 'linux/pkt_cls.h',
        'enum', 'TCA_FLOWER_KEY_VLAN_PRIO' ],

        [ 'HAVE_TC_BPF', 'linux/pkt_cls.h',
        'enum', 'TCA_BPF_UNSPEC' ],

        [ 'HAVE_TC_BPF_FD', 'linux/pkt_cls.h',
        'enum','TCA_BPF_FD' ],

        [ 'HAVE_TC_ACT_BPF', 'linux/tc_act/tc_bpf.h',
        'enum','TCA_ACT_BPF_UNSPEC' ],

        [ 'HAVE_TC_ACT_BPF_FD', 'linux/tc_act/tc_bpf.h',
        'enum','TCA_ACT_BPF_FD' ],
    ]
    autoconf_script = 'src/dpdk/auto-config-h.sh'
    autoconf_command = os.path.join(top, autoconf_script)
    for arg in has_sym_args:
        result, output = getstatusoutput("%s %s '%s' '%s' '%s' '%s' > /dev/null" %
            (autoconf_command, autoconf_path, arg[0], arg[1], arg[2], arg[3]))
        if result != 0:
            ctx.end_msg('failed\n%s\n' % output, 'YELLOW')
            break
    if result == 0:
        ctx.end_msg('done', 'GREEN')

@conf
def configure_mlx5 (ctx):
    ctx.start_msg('Configuring MLX5 autoconf')
    autoconf_file = 'src/dpdk/drivers/common/mlx5/mlx5_autoconf.h'
    autoconf_path = os.path.join(top, autoconf_file)
    os.system('rm -rf %s' % autoconf_path)
    # input array for meson symbol search:
    # [ "MACRO to define if found", "header for the search",
    #   "symbol type to search" "symbol name to search" ]
    has_sym_args = [
        [ 'HAVE_IBV_MLX5_MOD_SWP', 'infiniband/mlx5dv.h',
        'type', 'struct mlx5dv_sw_parsing_caps' ],
        [ 'HAVE_IBV_DEVICE_COUNTERS_SET_V42', 'infiniband/verbs.h',
        'type', 'struct ibv_counter_set_init_attr' ],
        [ 'HAVE_IBV_DEVICE_COUNTERS_SET_V45', 'infiniband/verbs.h',
        'type', 'struct ibv_counters_init_attr' ],
        [ 'HAVE_MLX5DV_DEVX_UAR_OFFSET', 'infiniband/mlx5dv.h',
        'field', 'struct mlx5dv_devx_uar.mmap_off'],

        [ 'HAVE_IBV_RELAXED_ORDERING', 'infiniband/verbs.h',
        'enum','IBV_ACCESS_RELAXED_ORDERING '],

        [ 'HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX' ],
        [ 'HAVE_IBV_DEVICE_TUNNEL_SUPPORT', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS' ],
        [ 'HAVE_IBV_MLX5_MOD_MPW', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED' ],
        [ 'HAVE_IBV_MLX5_MOD_CQE_128B_COMP', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP' ],
        [ 'HAVE_IBV_MLX5_MOD_CQE_128B_PAD', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD' ],
        [ 'HAVE_IBV_FLOW_DV_SUPPORT', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_create_flow_action_packet_reformat' ],
        [ 'HAVE_IBV_DEVICE_MPLS_SUPPORT', 'infiniband/verbs.h',
        'enum', 'IBV_FLOW_SPEC_MPLS' ],
        [ 'HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING', 'infiniband/verbs.h',
        'enum', 'IBV_WQ_FLAGS_PCI_WRITE_END_PADDING' ],
        [ 'HAVE_IBV_WQ_FLAG_RX_END_PADDING', 'infiniband/verbs.h',
        'enum', 'IBV_WQ_FLAG_RX_END_PADDING' ],
        [ 'HAVE_MLX5DV_DR_DEVX_PORT', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_query_devx_port' ],
        [ 'HAVE_IBV_DEVX_OBJ', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_devx_obj_create' ],
        [ 'HAVE_IBV_FLOW_DEVX_COUNTERS', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_FLOW_ACTION_COUNTERS_DEVX' ],
        [ 'HAVE_MLX5_DR_CREATE_ACTION_DEFAULT_MISS', 'infiniband/mlx5dv.h',
        'enum','MLX5DV_FLOW_ACTION_DEFAULT_MISS' ],

        [ 'HAVE_IBV_DEVX_ASYNC', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_devx_obj_query_async' ],
        [ 'HAVE_IBV_DEVX_QP', 'infiniband/mlx5dv.h',
        'func','mlx5dv_devx_qp_query' ],
        [ 'HAVE_MLX5DV_PP_ALLOC', 'infiniband/mlx5dv.h',
        'func','mlx5dv_pp_alloc' ],



        [ 'HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_dr_action_create_dest_devx_tir' ],
        [ 'HAVE_IBV_DEVX_EVENT', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_devx_get_event' ],
        [ 'HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_dr_action_create_flow_meter' ],
        [ 'HAVE_MLX5DV_MMAP_GET_NC_PAGES_CMD', 'infiniband/mlx5dv.h',
        'enum', 'MLX5_MMAP_GET_NC_PAGES_CMD' ],
        [ 'HAVE_MLX5DV_DR', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_DR_DOMAIN_TYPE_NIC_RX' ],
        [ 'HAVE_MLX5DV_DR_ESWITCH', 'infiniband/mlx5dv.h',
        'enum', 'MLX5DV_DR_DOMAIN_TYPE_FDB' ],
        [ 'HAVE_MLX5DV_DR_VLAN', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_dr_action_create_push_vlan' ],
        [ 'HAVE_IBV_VAR', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_alloc_var' ],

        [ 'HAVE_MLX5_OPCODE_ENHANCED_MPSW', 'infiniband/mlx5dv.h',
        'enum','MLX5_OPCODE_ENHANCED_MPSW' ],
        [ 'HAVE_MLX5_OPCODE_SEND_EN', 'infiniband/mlx5dv.h',
        'enum','MLX5_OPCODE_SEND_EN' ],
        [ 'HAVE_MLX5_OPCODE_WAIT', 'infiniband/mlx5dv.h',
        'enum', 'MLX5_OPCODE_WAIT' ],
        [ 'HAVE_MLX5_OPCODE_ACCESS_ASO', 'infiniband/mlx5dv.h',
        'enum','MLX5_OPCODE_ACCESS_ASO' ],


        [ 'HAVE_SUPPORTED_40000baseKR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_40000baseKR4_Full' ],
        [ 'HAVE_SUPPORTED_40000baseCR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_40000baseCR4_Full' ],
        [ 'HAVE_SUPPORTED_40000baseSR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_40000baseSR4_Full' ],
        [ 'HAVE_SUPPORTED_40000baseLR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_40000baseLR4_Full' ],
        [ 'HAVE_SUPPORTED_56000baseKR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_56000baseKR4_Full' ],
        [ 'HAVE_SUPPORTED_56000baseCR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_56000baseCR4_Full' ],
        [ 'HAVE_SUPPORTED_56000baseSR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_56000baseSR4_Full' ],
        [ 'HAVE_SUPPORTED_56000baseLR4_Full', 'linux/ethtool.h',
        'define', 'SUPPORTED_56000baseLR4_Full' ],
        [ 'HAVE_ETHTOOL_LINK_MODE_25G', 'linux/ethtool.h',
        'enum', 'ETHTOOL_LINK_MODE_25000baseCR_Full_BIT' ],
        [ 'HAVE_ETHTOOL_LINK_MODE_50G', 'linux/ethtool.h',
        'enum', 'ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT' ],
        [ 'HAVE_ETHTOOL_LINK_MODE_100G', 'linux/ethtool.h',
        'enum', 'ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT' ],
        [ 'HAVE_IFLA_NUM_VF', 'linux/if_link.h',
        'enum', 'IFLA_NUM_VF' ],
        [ 'HAVE_IFLA_EXT_MASK', 'linux/if_link.h',
        'enum', 'IFLA_EXT_MASK' ],
        [ 'HAVE_IFLA_PHYS_SWITCH_ID', 'linux/if_link.h',
        'enum', 'IFLA_PHYS_SWITCH_ID' ],
        [ 'HAVE_IFLA_PHYS_PORT_NAME', 'linux/if_link.h',
        'enum', 'IFLA_PHYS_PORT_NAME' ],
        [ 'HAVE_RDMA_NL_NLDEV', 'rdma/rdma_netlink.h',
        'enum', 'RDMA_NL_NLDEV' ],
        [ 'HAVE_RDMA_NLDEV_CMD_GET', 'rdma/rdma_netlink.h',
        'enum', 'RDMA_NLDEV_CMD_GET' ],
        [ 'HAVE_RDMA_NLDEV_CMD_PORT_GET', 'rdma/rdma_netlink.h',
        'enum', 'RDMA_NLDEV_CMD_PORT_GET' ],
        [ 'HAVE_RDMA_NLDEV_ATTR_DEV_INDEX', 'rdma/rdma_netlink.h',
        'enum', 'RDMA_NLDEV_ATTR_DEV_INDEX' ],
        [ 'HAVE_RDMA_NLDEV_ATTR_DEV_NAME', 'rdma/rdma_netlink.h',
        'enum', 'RDMA_NLDEV_ATTR_DEV_NAME' ],
        [ 'HAVE_RDMA_NLDEV_ATTR_PORT_INDEX', 'rdma/rdma_netlink.h',
        'enum', 'RDMA_NLDEV_ATTR_PORT_INDEX' ],
        [ 'HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX', 'rdma/rdma_netlink.h',
        'enum', 'RDMA_NLDEV_ATTR_NDEV_INDEX' ],
        [ 'HAVE_MLX5_DR_FLOW_DUMP', 'infiniband/mlx5dv.h',
        'func', 'mlx5dv_dump_dr_domain'],
        [ 'HAVE_DEVLINK', 'linux/devlink.h',
        'define', 'DEVLINK_GENL_NAME' ],
        [ 'HAVE_MLX5_DR_CREATE_ACTION_ASO', 'infiniband/mlx5dv.h',
        'func','mlx5dv_dr_action_create_aso' ],
        [ 'HAVE_INFINIBAND_VERBS_H', 'infiniband/verbs.h',
        'define', 'INFINIBAND_VERBS_H' ],
        [ 'HAVE_MLX5_DR_CREATE_ACTION_DEST_ARRAY', 'infiniband/mlx5dv.h',
        'func','mlx5dv_dr_action_create_dest_array'],
        [ 'HAVE_MLX5DV_DR_MEM_RECLAIM', 'infiniband/mlx5dv.h',
        'func','mlx5dv_dr_domain_set_reclaim_device_memory'],
        [ 'HAVE_MLX5_DR_CREATE_ACTION_FLOW_SAMPLE', 'infiniband/mlx5dv.h',
        'func','mlx5dv_dr_action_create_flow_sampler'],

    ]
    autoconf_script = 'src/dpdk/auto-config-h.sh'
    autoconf_command = os.path.join(top, autoconf_script)
    for arg in has_sym_args:
        result, output = getstatusoutput("%s %s '%s' '%s' '%s' '%s' > /dev/null" %
            (autoconf_command, autoconf_path, arg[0], arg[1], arg[2], arg[3]))
        if result != 0:
            ctx.end_msg('failed\n%s\n' % output, 'YELLOW')
            break
    if result == 0:
        ctx.end_msg('done', 'GREEN')

@conf
def check_ofed(ctx):
    ctx.start_msg('Checking for OFED')
    ofed_info='/usr/bin/ofed_info'

    ofed_ver_re = re.compile('.*[-](\d)[.](\d)[-].*')

    ofed_ver= 42
    ofed_ver_show= '4.2'

    if not os.path.isfile(ofed_info):
        ctx.end_msg('not found', 'YELLOW')
        return False

    ret, out = getstatusoutput(ofed_info)
    if ret:
        ctx.end_msg("Can't run %s to verify version:\n%s" % (ofed_info, out), 'YELLOW')
        return False

    lines = out.splitlines()
    if len(lines) < 2:
        ctx.end_msg('Expected several output lines from %s, got:\n%s' % (ofed_info, out), 'YELLOW')
        return False

    m= ofed_ver_re.match(str(lines[0]))
    if m:
        ver=int(m.group(1))*10+int(m.group(2))
        if ver < ofed_ver:
          ctx.end_msg("installed OFED version is '%s' should be at least '%s' and up - try with ./b configure --no-mlx flag" % (lines[0],ofed_ver_show),'YELLOW')
          return False
    else:
        ctx.end_msg("not found valid  OFED version '%s' " % (lines[0]),'YELLOW')
        return False

    if not os.path.isfile("/usr/include/infiniband/mlx5dv.h"):
       ctx.end_msg("ERROR, OFED should be installed using '$./mlnxofedinstall --with-mft --with-mstflint --dpdk --upstream-libs' try to reinstall or see manual", 'YELLOW')
       return False

    ctx.end_msg('Found version %s.%s' % (m.group(1), m.group(2)))
    return True

@conf
def check_ntapi(ctx):
    ctx.start_msg('Checking for NTAPI')
    ntapi_lib='/opt/napatech3/lib/libntapi.so'

    if not os.path.isfile(ntapi_lib):
        ctx.end_msg('not found', 'YELLOW')
        return False

    ctx.end_msg('Found needed NTAPI library')
    return True

    
def verify_cc_version (env, min_ver = REQUIRED_CC_VERSION):
    ver = StrictVersion('.'.join(env['CC_VERSION']))

    return (ver >= min_ver, ver, min_ver)
    

@conf
def get_ld_search_path(ctx):
    ctx.start_msg('Getting LD search path')
    cmd = 'ld --verbose'
    ret, out = getstatusoutput(cmd)
    if ret != 0:
        ctx.end_msg('failed', 'YELLOW')
        ctx.fatal('\nCommand: %s\n\nOutput: %s' % (cmd, out))

    path_arr = []
    regex_str = '\s*SEARCH_DIR\("=?([^"]+)"\)\s*'
    regex = re.compile(regex_str)
    for line in out.splitlines():
        if line.startswith('SEARCH_DIR'):
            for elem in line.split(';'):
                if not elem.strip():
                    continue
                m = regex.match(elem)
                if m is None:
                    ctx.end_msg('failed', 'YELLOW')
                    Logs.pprint('NORMAL', '\nCommand: %s\n\nOutput: %s' % (cmd, out))
                    ctx.fatal('Regex (%s0 does not match output (%s).' % (regex_str, elem))
                else:
                    path_arr.append(m.group(1))

    if not path_arr:
        ctx.end_msg('failed', 'YELLOW')
        Logs.pprint('NORMAL', '\nCommand: %s\n\nOutput: %s' % (cmd, out))
        ctx.fatal('Could not find SEARCH_DIR in output')

    ctx.env.LD_SEARCH_PATH = path_arr
    ctx.end_msg('ok', 'GREEN')

def check_version_glibc(lib_so):
    max_num =0
    for so in lib_so:
        cmd = 'strings {}'.format(so)
        s, lines = getstatusoutput(cmd)
        if s ==0: 
            ls= lines.split('\n')  
            for line in ls:
                prefix = 'GLIBCXX_'
                if line.startswith(prefix):
                    fix = line[len(prefix):]
                    d=fix.split('.')
                    if len(d) == 3:
                        num = int(d[0])*1000 +int(d[1])*100+int(d[2])
                        if num >max_num:
                            max_num = num 
    return max_num


def configure(conf):

    conf.find_program('strings')
    so = ['/usr/lib/x86_64-linux-gnu/libstdc++.so.6','/usr/lib64/libstdc++.so.6']
    our_so =  '../scripts/so/x86_64/libstdc++.so.6'
    d_so =  '../scripts/so/x86_64/_libstdc++.so.6'
    ver = check_version_glibc(so)
    if ver > 3424:
        Logs.pprint('YELLOW', 'you have newer {} libstdc++.so remove the old one, do not commit this \n'.format(ver))
        try:
          os.system("mv %s %s " %(our_so,d_so))
        except Exception as ex:
          pass 

    conf.load('clang_compilation_database',tooldir=['../external_libs/waf-tools'])

    if int(conf.options.gcc6) + int(conf.options.gcc7) + int(conf.options.gcc8) > 1:
        conf.fatal('--gcc6, --gcc7 and --gcc8 are mutually exclusive')

    if conf.options.gcc6:
        if march == 'ppc64le':
            conf.fatal('Power9 CPU not supported by GCC6')
        configure_gcc(conf, GCC6_DIRS)
    elif conf.options.gcc7:
        if march == 'ppc64le':
            conf.fatal('Power9 CPU not supported by GCC7')
        conf.environ['AR']="x86_64-pc-linux-gnu-gcc-ar"
        configure_gcc(conf, GCC7_DIRS)
    elif conf.options.gcc8:
        conf.environ['AR']="x86_64-pc-linux-gnu-gcc-ar"
        configure_gcc(conf, GCC8_DIRS)
    else:
        configure_gcc(conf)


    conf.find_program('ldd')
    conf.check_cxx(lib = 'z', errmsg = missing_pkg_msg(fedora = 'zlib-devel', ubuntu = 'zlib1g-dev'))
    no_mlx          = conf.options.no_mlx
    no_bnxt         = conf.options.no_bnxt
    with_ntacc      = conf.options.with_ntacc
    with_bird       = conf.options.with_bird
    with_sanitized  = conf.options.sanitized
    new_memory      = conf.options.new_memory
    
    configure_sanitized(conf, with_sanitized)
            
    conf.env.NO_MLX = no_mlx
    conf.env.TAP = conf.options.tap

    if no_mlx != 'all':
        if conf.options.no_ofed_check:
            ofed_ok = True
        else:
            ofed_ok = conf.check_ofed(mandatory = False)

        conf.env.OFED_OK = ofed_ok
        conf.check_cxx(lib = 'mnl', mandatory = False, errmsg = 'not found, will use internal version')

        if ofed_ok:
            conf.configure_mlx5(mandatory = False)
            conf.get_ld_search_path(mandatory = True)
            conf.check_cxx(lib = 'ibverbs', errmsg = 'Could not find library ibverbs, will use internal version.', mandatory = False)
        else:
            conf.configure_dummy_mlx5(mandatory = False)
            Logs.pprint('YELLOW', 'Warning: will use internal version of ibverbs. If you need to use Mellanox NICs, install OFED:\n' +
                                  'https://trex-tgn.cisco.com/trex/doc/trex_manual.html#_mellanox_connectx_4_support')
        if no_mlx != 'mlx5':
            Logs.pprint('YELLOW', 'Building mlx5 PMD')

    conf.env.NO_BNXT = no_bnxt
    if not no_bnxt:
        Logs.pprint('YELLOW', 'Building bnxt PMD')

    if conf.env.TAP:
        conf.configure_tap(mandatory = False)

    conf.env.WITH_NTACC = with_ntacc
    conf.env.WITH_BIRD = with_bird

    if with_ntacc:
        ntapi_ok = conf.check_ntapi(mandatory = False)
        if not ntapi_ok:
            Logs.pprint('RED', 'Cannot find NTAPI. If you need to use Napatech NICs, install the Napatech driver:\n' +
                                  'https://www.napatech.com/downloads/')
            raise Exception("Cannot find libntapi");

    conf.env.DPDK_NEW_MEMORY = new_memory
    if new_memory:
        conf.check_cxx(lib = 'numa', errmsg = missing_pkg_msg(fedora = 'libnuma-devel', ubuntu = 'libnuma-dev'))
        s = '#ifndef RTE_EAL_NUMA_AWARE_HUGEPAGES\n'
        s += '#define RTE_EAL_NUMA_AWARE_HUGEPAGES 1\n'
        s += '#endif\n'
        write_file(os.path.join(conf.bldnode.abspath(), H_DPDK_CONFIG), s)

def search_in_paths(paths):
    for path in paths:
        if os.path.exists(path):
            return path


def load_compiler(conf):
    if 'clang' in conf.environ.get('CXX', ''):
        conf.load('clang++')
        conf.load('clang')
    else:
        conf.load('g++')
        conf.load('gcc')
    return



def configure_gcc(conf, explicit_paths = None):
    # use the system path
    if explicit_paths is None:
        load_compiler(conf)
        return

    if type(explicit_paths) is not list:
        explicit_paths = [explicit_paths]

    explicit_path = search_in_paths(explicit_paths)
    if not explicit_path:
        conf.fatal('unable to find GCC in installation dir(s): {0}'.format(explicit_paths))

    saved = conf.environ['PATH']
    try:
        conf.environ['PATH'] = explicit_path
        load_compiler(conf)
    finally:
        conf.environ['PATH'] = saved 



def configure_sanitized (conf, with_sanitized):

    # first we turn off SANITIZED
    conf.env.SANITIZED = False

    # if sanitized is required - check GCC version for sanitizing
    conf.start_msg('Build sanitized images (GCC >= {0})'.format(SANITIZE_CC_VERSION))    

    # not required
    if not with_sanitized:
        conf.end_msg('no', 'YELLOW')

    else:
        rc = verify_cc_version(conf.env, SANITIZE_CC_VERSION)
        if not rc[0]:
            conf.fatal('--sanitized is supported only with GCC {0}+ - current {1}'.format(rc[2], rc[1]))
        else:
            conf.end_msg('yes', 'GREEN')
            conf.env.SANITIZED = True



def getstatusoutput(cmd):
    """    Return (status, output) of executing cmd in a shell. Taken from Python3 subprocess.getstatusoutput"""
    try:
        data = subprocess.check_output(cmd, shell=True, universal_newlines=True, stderr=subprocess.STDOUT)
        status = 0
    except subprocess.CalledProcessError as ex:
        data = ex.output
        status = ex.returncode
    if data[-1:] == '\n':
        data = data[:-1]
    return status, data

main_src = SrcGroup(dir='src',
        src_list=[
             '44bsd/tcp_output.cpp',
             '44bsd/tcp_timer.cpp',
             '44bsd/tcp_debug.cpp',
             '44bsd/tcp_subr.cpp',
             '44bsd/flow_table.cpp',
             '44bsd/tcp_input.cpp',
             '44bsd/tcp_usrreq.cpp',
             '44bsd/tcp_socket.cpp',
             '44bsd/tcp_dpdk.cpp',
             '44bsd/sch_rampup.cpp',
             '44bsd/tick_cmd_clock.cpp',
             '44bsd/udp.cpp',

             'tunnels/gtp_man.cpp',

             'hdrh/hdr_time.c',
             'hdrh/hdr_encoding.c',
             'hdrh/hdr_histogram.c',
             'hdrh/hdr_histogram_log.c',

             'bp_sim_tcp.cpp',
             'astf/astf_template_db.cpp',
             'stt_cp.cpp',
             'trex_global.cpp',
             'trex_modes.cpp',
             'bp_sim.cpp',
             'trex_platform.cpp',
             'flow_stat_parser.cpp',
             'global_io_mode.cpp',
             'main_dpdk.cpp',
             'dpdk_port_map.cpp',
             'trex_watchdog.cpp',
             'trex_client_config.cpp',
             'debug.cpp',
             'inet_pton.cpp',
             'pkt_gen.cpp',
             'tw_cfg.cpp',
             'platform_cfg.cpp',
             'pre_test.cpp',
             'tuple_gen.cpp',
             'rx_check.cpp',
             'rx_check_header.cpp',
             'stateful_rx_core.cpp',
             'timer_wheel_pq.cpp',
             'time_histogram.cpp',
             'os_time.cpp',
             'nat_check.cpp',
             'nat_check_flow_table.cpp',
             'msg_manager.cpp',
             'trex_port_attr.cpp',
             'dpdk_trex_port_attr.cpp',
             'publisher/trex_publisher.cpp',
             'pal/linux_dpdk/pal_utl.cpp',
             'pal/linux_dpdk/mbuf.cpp',
             'pal/common/common_mbuf.cpp',
             'h_timer.cpp',
             'astf/astf_db.cpp',
             'astf/astf_json_validator.cpp',
             'bp_sim_stf.cpp',
             'trex_build_info.cpp',
             'dpdk_drv_filter.cpp',

             'drivers/trex_driver_base.cpp',
             'drivers/trex_driver_bnxt.cpp',
             'drivers/trex_driver_i40e.cpp',
             'drivers/trex_driver_igb.cpp',
             'drivers/trex_driver_ixgbe.cpp',
             'drivers/trex_driver_mlx5.cpp',
             'drivers/trex_driver_ice.cpp',
             'drivers/trex_driver_ntacc.cpp',
             'drivers/trex_driver_vic.cpp',
             'drivers/trex_driver_virtual.cpp',

             'utils/utl_counter.cpp',
             'utils/utl_cpuu.cpp',
             'utils/utl_dbl_human.cpp',
             'utils/utl_ip.cpp',
             'utils/utl_json.cpp',
             'utils/utl_mbuf.cpp',
             'utils/utl_offloads.cpp',
             'utils/utl_policer.cpp',
             'utils/utl_port_map.cpp',
             'utils/utl_sync_barrier.cpp',
             'utils/utl_term_io.cpp',
             'utils/utl_yaml.cpp',
             ])

cmn_src = SrcGroup(dir='src/common',
    src_list=[
        'basic_utils.cpp',
        'captureFile.cpp',
        'erf.cpp',
        'pcap.cpp',
        'base64.cpp',
        'n_uniform_prob.cpp'
        ])

net_src = SrcGroup(dir='src/common/Network/Packet',
        src_list=[
           'CPktCmn.cpp',
           'EthernetHeader.cpp',
           'IPHeader.cpp',
           'IPv6Header.cpp',
           'TCPHeader.cpp',
           'TCPOptions.cpp',
           'UDPHeader.cpp',
           'MacAddress.cpp',
           'VLANHeader.cpp'])

tcp_src = SrcGroup(dir='src/44bsd/netinet',
    src_list=[
        'tcp_output.c',
        'tcp_input.c',

        'tcp_debug.c',
        'tcp_sack.c',
        'tcp_timer.c',
        'tcp_subr.c',

        'cc/cc_newreno.c',
        'cc/cc_cubic.c',
        ]);


# JSON package
json_src = SrcGroup(dir='external_libs/json',
        src_list=[
            'jsoncpp.cpp'
           ])

# MD5 package
md5_src = SrcGroup(dir='external_libs/md5',
    src_list=[
        'md5.cpp'
    ])


# RPC code
rpc_server_src = SrcGroup(dir='src/rpc-server/',
                          src_list=[
                              'trex_rpc_server.cpp',
                              'trex_rpc_req_resp_server.cpp',
                              'trex_rpc_jsonrpc_v2_parser.cpp',
                              'trex_rpc_cmds_table.cpp',
                              'trex_rpc_cmd.cpp',
                              'trex_rpc_zip.cpp',
                          ])


ef_src = SrcGroup(dir='src/common',
    src_list=[
        'ef/efence.cpp',
        'ef/page.cpp',
        'ef/print.cpp'
        ])



# common code for STL / ASTF / STF
stx_src = SrcGroup(dir='src/stx/common/',
    src_list=[
        'trex_capture.cpp',
        'trex_dp_core.cpp',
        'trex_dp_port_events.cpp',
        'trex_latency_counters.cpp',
        'trex_tpg_stats.cpp',
        'trex_messaging.cpp',
        'trex_owner.cpp',
        'trex_pkt.cpp',
        'trex_port.cpp',
        'trex_rpc_cmds_common.cpp',
        'trex_rx_core.cpp',
        'trex_rx_packet_parser.cpp',
        'trex_rx_port_mngr.cpp',
        'trex_rx_tx.cpp',
        'trex_stack_base.cpp',
        'trex_stack_counters.cpp',
        'trex_stack_legacy.cpp',
        'trex_rx_rpc_tunnel.cpp',
        'trex_stack_linux_based.cpp',
        'trex_stx.cpp',
        'trex_vlan_filter.cpp'
    ])


# stateful code
stf_src = SrcGroup(dir='src/stx/stf/',
                    src_list=['trex_stf.cpp'])


# stateless code
stateless_src = SrcGroup(dir='src/stx/stl/',
                          src_list=['trex_stl_stream.cpp',
                                    'trex_stl_stream_vm.cpp',
                                    'trex_stl.cpp',
                                    'trex_stl_port.cpp',
                                    'trex_stl_streams_compiler.cpp',
                                    'trex_stl_vm_splitter.cpp',
                                    'trex_stl_dp_core.cpp',
                                    'trex_stl_fs.cpp',
                                    'trex_stl_tpg.cpp',
                                    'trex_stl_messaging.cpp',
                                    'trex_stl_rpc_cmds.cpp'
                                    ])



# ASTF batch
astf_batch_src = SrcGroup(dir='src/stx/astf_batch/',
                           src_list=['trex_astf_batch.cpp'])


# ASTF
astf_src = SrcGroup(dir='src/stx/astf/',
    src_list=[
        'trex_astf.cpp',
        'trex_astf_dp_core.cpp',
        'trex_astf_messaging.cpp',
        'trex_astf_port.cpp',
        'trex_astf_rpc_cmds.cpp',
        'trex_astf_rx_core.cpp',
        'trex_astf_topo.cpp',
        'trex_astf_mbuf_redirect.cpp'
    ])


yaml_src = SrcGroup(dir='external_libs/yaml-cpp/src/',
        src_list=[
            'aliasmanager.cpp',
            'binary.cpp',
            'contrib/graphbuilder.cpp',
            'contrib/graphbuilderadapter.cpp',
            'conversion.cpp',
            'directives.cpp',
            'emitfromevents.cpp',
            'emitter.cpp',
            'emitterstate.cpp',
            'emitterutils.cpp',
            'exp.cpp',
            'iterator.cpp',
            'node.cpp',
            'nodebuilder.cpp',
            'nodeownership.cpp',
            'null.cpp',
            'ostream.cpp',
            'parser.cpp',
            'regex.cpp',
            'scanner.cpp',
            'scanscalar.cpp',
            'scantag.cpp',
            'scantoken.cpp',
            'simplekey.cpp',
            'singledocparser.cpp',
            'stream.cpp',
            'tag.cpp'])


bpf_src =  SrcGroup(dir='external_libs/bpf/',
        src_list=[
            'bpf_api.c',
            'bpf.c',
            'scanner.c',
            'grammar.c',
            'optimize.c',
            'nametoaddr.c',
            'bpf_filter.c',
            'etherent.c'])

bpfjit_src = SrcGroup(dir='external_libs/bpf/bpfjit',
        src_list=[
            'bpfjit.c',
            'sljitLir.c'
            ])


version_src = SrcGroup(
    dir='linux_dpdk',
    src_list=[
        'version.c',
    ])


dpdk_src_x86_64 = SrcGroup(dir='src/dpdk/',
        src_list=[
                 #enic
                 'drivers/net/enic/enic_ethdev.c',
                 'drivers/net/enic/base/vnic_intr.c',
                 'drivers/net/enic/base/vnic_rq.c',
                 'drivers/net/enic/base/vnic_wq.c',
                 'drivers/net/enic/base/vnic_dev.c',
                 'drivers/net/enic/base/vnic_cq.c',

                 'drivers/net/enic/enic_vf_representor.c',
                 'drivers/net/enic/enic_flow.c',
                 'drivers/net/enic/enic_fm_flow.c',
                 'drivers/net/enic/enic_rxtx.c',
                 'drivers/net/enic/enic_res.c',
                 'drivers/net/enic/enic_main.c',

                 #ICE   
                'drivers/net/ice/base/ice_vlan_mode.c',
                'drivers/net/ice/base/ice_acl.c',
                'drivers/net/ice/base/ice_acl_ctrl.c',
                'drivers/net/ice/base/ice_controlq.c',
                'drivers/net/ice/base/ice_common.c',
                'drivers/net/ice/base/ice_sched.c',
                'drivers/net/ice/base/ice_switch.c',
                'drivers/net/ice/base/ice_nvm.c',
                'drivers/net/ice/base/ice_flex_pipe.c',
                'drivers/net/ice/base/ice_flow.c',
                'drivers/net/ice/base/ice_dcb.c',
                'drivers/net/ice/base/ice_fdir.c',

                'drivers/net/ice/ice_dcf_vf_representor.c',
                'drivers/net/ice/ice_acl_filter.c',
                'drivers/net/ice/ice_dcf.c',
                'drivers/net/ice/ice_dcf_ethdev.c',
                'drivers/net/ice/ice_dcf_parent.c',
                'drivers/net/ice/ice_ethdev.c',
                'drivers/net/ice/ice_rxtx.c',
                'drivers/net/ice/ice_rxtx_vec_sse.c',
                'drivers/net/ice/ice_switch_filter.c',
                'drivers/net/ice/ice_fdir_filter.c',
                'drivers/net/ice/ice_hash.c',
                #'drivers/net/ice/ice_rxtx_vec_avx2.c',
                #'drivers/net/ice/ice_rxtx_vec_avx512.c',
                'drivers/net/ice/ice_generic_flow.c',

                 #ixgbe
                 'drivers/net/ixgbe/base/ixgbe_82598.c',
                 'drivers/net/ixgbe/base/ixgbe_82599.c',
                 'drivers/net/ixgbe/base/ixgbe_api.c',
                 'drivers/net/ixgbe/base/ixgbe_common.c',
                 'drivers/net/ixgbe/base/ixgbe_dcb.c',
                 'drivers/net/ixgbe/base/ixgbe_dcb_82598.c',
                 'drivers/net/ixgbe/base/ixgbe_dcb_82599.c',
                 'drivers/net/ixgbe/base/ixgbe_hv_vf.c',
                 'drivers/net/ixgbe/base/ixgbe_mbx.c',
                 'drivers/net/ixgbe/base/ixgbe_phy.c',
                 'drivers/net/ixgbe/base/ixgbe_vf.c',
                 'drivers/net/ixgbe/base/ixgbe_x540.c',
                 'drivers/net/ixgbe/base/ixgbe_x550.c',
                 'drivers/net/ixgbe/ixgbe_ethdev.c',
                 'drivers/net/ixgbe/ixgbe_fdir.c',
                 'drivers/net/ixgbe/ixgbe_flow.c',
                 'drivers/net/ixgbe/ixgbe_pf.c',
                 'drivers/net/ixgbe/ixgbe_rxtx.c',
                 'drivers/net/ixgbe/ixgbe_rxtx_vec_sse.c',
                 #'drivers/net/ixgbe/ixgbe_ipsec.c',
                 'drivers/net/ixgbe/ixgbe_tm.c',
                 'drivers/net/ixgbe/ixgbe_vf_representor.c',
                 'drivers/net/ixgbe/rte_pmd_ixgbe.c',

                 #i40e
                 'drivers/net/i40e/i40e_rxtx_vec_sse.c',

                 #virtio
                 'drivers/net/virtio/virtio_rxtx_simple_sse.c',

                 #vmxnet3
                 'drivers/net/vmxnet3/vmxnet3_ethdev.c',
                 'drivers/net/vmxnet3/vmxnet3_rxtx.c',

                 #af_packet
                 'drivers/net/af_packet/rte_eth_af_packet.c',

                 #Amazone ENA
                 'drivers/net/ena/ena_ethdev.c',
                 'drivers/net/ena/base/ena_com.c',
                 'drivers/net/ena/base/ena_eth_com.c',

                 #libs
                 'lib/librte_eal/x86/rte_cpuflags.c',
                 'lib/librte_eal/x86/rte_spinlock.c',
                 'lib/librte_eal/x86/rte_cycles.c',
                 'lib/librte_eal/x86/rte_hypervisor.c',
                 
                 #'lib/librte_security/rte_security.c',

                 #failsafe

                 'drivers/net/failsafe/failsafe.c',
                 'drivers/net/failsafe/failsafe_args.c',
                 'drivers/net/failsafe/failsafe_eal.c',
                 'drivers/net/failsafe/failsafe_ops.c',
                 'drivers/net/failsafe/failsafe_rxtx.c',
                 'drivers/net/failsafe/failsafe_ether.c',
                 'drivers/net/failsafe/failsafe_flow.c',
                 'drivers/net/failsafe/failsafe_intr.c',


                 #vdev_netvsc
                 'drivers/net/vdev_netvsc/vdev_netvsc.c',

                 #netvsc
                 'drivers/net/netvsc/hn_ethdev.c',
                 'drivers/net/netvsc/hn_rxtx.c',
                 'drivers/net/netvsc/hn_rndis.c',
                 'drivers/net/netvsc/hn_nvs.c',
                 'drivers/net/netvsc/hn_vf.c',

                 #ip_frag
                 'lib/librte_ip_frag/rte_ipv4_fragmentation.c',
                 'lib/librte_ip_frag/rte_ipv6_fragmentation.c',
                 'lib/librte_ip_frag/rte_ipv4_reassembly.c',
                 'lib/librte_ip_frag/rte_ipv6_reassembly.c',
                 'lib/librte_ip_frag/rte_ip_frag_common.c',
                 'lib/librte_ip_frag/ip_frag_internal.c',

                 #bonding
                 'drivers/net/bonding/rte_eth_bond_api.c',
                 'drivers/net/bonding/rte_eth_bond_pmd.c',
                 'drivers/net/bonding/rte_eth_bond_flow.c',
                 'drivers/net/bonding/rte_eth_bond_args.c',
                 'drivers/net/bonding/rte_eth_bond_8023ad.c',
                 'drivers/net/bonding/rte_eth_bond_alb.c',

                 ])

dpdk_src_x86_64_ext = SrcGroup(dir='src',
        src_list=['drivers/trex_ixgbe_fdir.c',
                  'drivers/trex_i40e_fdir.c']
)


dpdk_src_x86_64_tap = SrcGroup(dir='src/dpdk/',
        src_list=[
                 #tap 
                 'drivers/net/tap/rte_eth_tap.c',
                 'drivers/net/tap/tap_flow.c',
                 'drivers/net/tap/tap_netlink.c',
                 'drivers/net/tap/tap_tcmsgs.c',
                 'drivers/net/tap/tap_bpf_api.c',
                 'drivers/net/tap/tap_intr.c',
                ])


dpdk_src_aarch64 = SrcGroup(dir='src/dpdk/',
        src_list=[
                 #virtio
                 'drivers/net/virtio/virtio_rxtx_simple_neon.c',

                 #libs
                 'lib/librte_eal/common/arch/arm/rte_cpuflags.c',
                 'lib/librte_eal/common/arch/arm/rte_cycles.c',

                 ])


dpdk_src_ppc64le = SrcGroup(dir='src/dpdk/',
        src_list=[
                 #i40e
                 'drivers/net/i40e/i40e_rxtx_vec_altivec.c',

                 #libs
                 'lib/librte_eal/common/arch/ppc_64/rte_cpuflags.c',
                 'lib/librte_eal/common/arch/ppc_64/rte_cycles.c',

                 ])


dpdk_src = SrcGroup(dir='src/dpdk/',
                src_list=[
                 '../dpdk_funcs.c',
                 'drivers/bus/pci/pci_common.c',
                 'drivers/bus/pci/pci_common_uio.c',
                 'drivers/bus/pci/pci_params.c',
                 'drivers/bus/pci/linux/pci.c',
                 'drivers/bus/pci/linux/pci_uio.c',
                 'drivers/bus/pci/linux/pci_vfio.c',
                 'drivers/bus/vdev/vdev.c',
                 'drivers/bus/vdev/vdev_params.c',
                 'drivers/bus/vmbus/vmbus_common.c',
                 'drivers/bus/vmbus/vmbus_channel.c',
                 'drivers/bus/vmbus/vmbus_bufring.c',
                 'drivers/bus/vmbus/vmbus_common_uio.c',
                 'drivers/bus/vmbus/linux/vmbus_bus.c',
                 'drivers/bus/vmbus/linux/vmbus_uio.c',

                 'drivers/mempool/ring/rte_mempool_ring.c',
                 #'drivers/mempool/stack/rte_mempool_stack.c', # requires dpdk/lib/librte_stack/rte_stack.h


                 # drivers
                 #bnxt
                 'drivers/net/bnxt/bnxt_cpr.c',
                 'drivers/net/bnxt/bnxt_ethdev.c',
                 'drivers/net/bnxt/bnxt_filter.c',
                 'drivers/net/bnxt/bnxt_flow.c',
                 'drivers/net/bnxt/bnxt_hwrm.c',
                 'drivers/net/bnxt/bnxt_irq.c',
                 'drivers/net/bnxt/bnxt_ring.c',
                 'drivers/net/bnxt/bnxt_rxq.c',
                 'drivers/net/bnxt/bnxt_rxr.c',
                 'drivers/net/bnxt/bnxt_reps.c',
                 'drivers/net/bnxt/bnxt_stats.c',
                 'drivers/net/bnxt/bnxt_txq.c',
                 'drivers/net/bnxt/bnxt_txr.c',
                 'drivers/net/bnxt/bnxt_util.c',
                 'drivers/net/bnxt/bnxt_vnic.c',
                 'drivers/net/bnxt/rte_pmd_bnxt.c',
                 'drivers/net/bnxt/bnxt_rxtx_vec_sse.c',

                 'drivers/net/bnxt/tf_core/tf_core.c',
                 'drivers/net/bnxt/tf_core/bitalloc.c',
                 'drivers/net/bnxt/tf_core/tf_msg.c',
                 'drivers/net/bnxt/tf_core/rand.c',
                 'drivers/net/bnxt/tf_core/stack.c',
                 'drivers/net/bnxt/tf_core/tf_em_common.c',
                 'drivers/net/bnxt/tf_core/tf_em_internal.c',
                 'drivers/net/bnxt/tf_core/tf_rm.c',
                 'drivers/net/bnxt/tf_core/tf_tbl.c',
                 'drivers/net/bnxt/tf_core/tfp.c',
                 'drivers/net/bnxt/tf_core/tf_session.c',
                 'drivers/net/bnxt/tf_core/tf_device.c',
                 'drivers/net/bnxt/tf_core/tf_device_p4.c',
                 'drivers/net/bnxt/tf_core/tf_identifier.c',
                 'drivers/net/bnxt/tf_core/tf_shadow_tbl.c',
                 'drivers/net/bnxt/tf_core/tf_shadow_tcam.c',
                 'drivers/net/bnxt/tf_core/tf_tcam.c',
                 'drivers/net/bnxt/tf_core/tf_util.c',
                 'drivers/net/bnxt/tf_core/tf_if_tbl.c',
                 'drivers/net/bnxt/tf_core/ll.c',
                 'drivers/net/bnxt/tf_core/tf_global_cfg.c',
                 'drivers/net/bnxt/tf_core/tf_em_host.c',
                 'drivers/net/bnxt/tf_core/tf_shadow_identifier.c',
                 'drivers/net/bnxt/tf_core/tf_hash.c',

                 'drivers/net/bnxt/hcapi/hcapi_cfa_p4.c',

                 'drivers/net/bnxt/tf_ulp/bnxt_ulp.c',
                 'drivers/net/bnxt/tf_ulp/ulp_mark_mgr.c',
                 'drivers/net/bnxt/tf_ulp/ulp_flow_db.c',
                 'drivers/net/bnxt/tf_ulp/ulp_template_db_tbl.c',
                 'drivers/net/bnxt/tf_ulp/ulp_template_db_class.c',
                 'drivers/net/bnxt/tf_ulp/ulp_template_db_act.c',
                 'drivers/net/bnxt/tf_ulp/ulp_utils.c',
                 'drivers/net/bnxt/tf_ulp/ulp_mapper.c',
                 'drivers/net/bnxt/tf_ulp/ulp_matcher.c',
                 'drivers/net/bnxt/tf_ulp/ulp_rte_parser.c',
                 'drivers/net/bnxt/tf_ulp/bnxt_ulp_flow.c',
                 'drivers/net/bnxt/tf_ulp/ulp_port_db.c',
                 'drivers/net/bnxt/tf_ulp/ulp_def_rules.c',
                 'drivers/net/bnxt/tf_ulp/ulp_fc_mgr.c',
                 'drivers/net/bnxt/tf_ulp/ulp_tun.c',
                 'drivers/net/bnxt/tf_ulp/ulp_template_db_wh_plus_act.c',
                 'drivers/net/bnxt/tf_ulp/ulp_template_db_wh_plus_class.c',
                 'drivers/net/bnxt/tf_ulp/ulp_template_db_stingray_act.c',
                 'drivers/net/bnxt/tf_ulp/ulp_template_db_stingray_class.c',

                 #e1000
                 'drivers/net/e1000/base/e1000_base.c',
                 'drivers/net/e1000/base/e1000_80003es2lan.c',
                 'drivers/net/e1000/base/e1000_82540.c',
                 'drivers/net/e1000/base/e1000_82541.c',
                 'drivers/net/e1000/base/e1000_82542.c',
                 'drivers/net/e1000/base/e1000_82543.c',
                 'drivers/net/e1000/base/e1000_82571.c',
                 'drivers/net/e1000/base/e1000_82575.c',
                 'drivers/net/e1000/base/e1000_api.c',
                 'drivers/net/e1000/base/e1000_i210.c',
                 'drivers/net/e1000/base/e1000_ich8lan.c',
                 'drivers/net/e1000/e1000_logs.c',
                 'drivers/net/e1000/base/e1000_mac.c',
                 'drivers/net/e1000/base/e1000_manage.c',
                 'drivers/net/e1000/base/e1000_mbx.c',
                 'drivers/net/e1000/base/e1000_nvm.c',
                 'drivers/net/e1000/base/e1000_osdep.c',
                 'drivers/net/e1000/base/e1000_phy.c',
                 'drivers/net/e1000/base/e1000_vf.c',
                 'drivers/net/e1000/em_ethdev.c',
                 'drivers/net/e1000/em_rxtx.c',
                 'drivers/net/e1000/igb_flow.c',
                 'drivers/net/e1000/igb_ethdev.c',
                 'drivers/net/e1000/igb_pf.c',
                 'drivers/net/e1000/igb_rxtx.c',

                 #memif
                 'drivers/net/memif/memif_socket.c',
                 'drivers/net/memif/rte_eth_memif.c',

                 #virtio
                 'drivers/net/virtio/virtio.c',
                 'drivers/net/virtio/virtio_ethdev.c',
                 'drivers/net/virtio/virtio_pci.c',
                 'drivers/net/virtio/virtio_pci_ethdev.c',
                 'drivers/net/virtio/virtio_rxtx.c',

                 
                 #'drivers/net/virtio/virtio_rxtx_packed.c',

                 'drivers/net/virtio/virtio_rxtx_simple.c',
                 'drivers/net/virtio/virtqueue.c',
                 'drivers/net/virtio/virtio_user_ethdev.c',
                 'drivers/net/virtio/virtio_user/vhost_vdpa.c',
                 'drivers/net/virtio/virtio_user/vhost_kernel.c',
                 'drivers/net/virtio/virtio_user/vhost_kernel_tap.c',
                 'drivers/net/virtio/virtio_user/vhost_user.c',
                 'drivers/net/virtio/virtio_user/virtio_user_dev.c',

                 'drivers/common/iavf/iavf_adminq.c',
                 'drivers/common/iavf/iavf_common.c',
                 'drivers/common/iavf/iavf_impl.c',

                 #libs
                'lib/librte_rcu/rte_rcu_qsbr.c',

                 'lib/librte_cfgfile/rte_cfgfile.c',

                 'lib/librte_eal/common/eal_common_hypervisor.c',
                 'lib/librte_eal/common/eal_common_dynmem.c',
                 'lib/librte_eal/common/eal_common_bus.c',
                 'lib/librte_eal/common/eal_common_class.c',
                 'lib/librte_eal/common/eal_common_cpuflags.c',
                 'lib/librte_eal/common/eal_common_debug.c',
                 'lib/librte_eal/common/eal_common_dev.c',
                 'lib/librte_eal/common/eal_common_devargs.c',
                 'lib/librte_eal/common/eal_common_errno.c',
                 'lib/librte_eal/common/eal_common_fbarray.c',
                 'lib/librte_eal/common/eal_common_hexdump.c',
                 'lib/librte_eal/common/eal_common_launch.c',
                 'lib/librte_eal/common/eal_common_lcore.c',
                 'lib/librte_eal/common/eal_common_log.c',
                 'lib/librte_eal/common/eal_common_memalloc.c',
                 'lib/librte_eal/common/eal_common_memory.c',
                 'lib/librte_eal/common/eal_common_memzone.c',
                 'lib/librte_eal/common/eal_common_mcfg.c',
                 'lib/librte_eal/common/eal_common_options.c',
                 'lib/librte_eal/common/eal_common_proc.c',
                 'lib/librte_eal/common/eal_common_string_fns.c',
                 'lib/librte_eal/common/eal_common_tailqs.c',
                 'lib/librte_eal/common/eal_common_thread.c',
                 'lib/librte_eal/common/eal_common_timer.c',
                 'lib/librte_eal/common/eal_common_config.c',
                 'lib/librte_eal/common/eal_common_trace.c',
                 'lib/librte_eal/common/eal_common_trace_ctf.c',
                 'lib/librte_eal/common/eal_common_trace_points.c',
                 'lib/librte_eal/common/eal_common_trace_utils.c',
                 'lib/librte_eal/common/eal_common_uuid.c',
                 'lib/librte_eal/common/rte_reciprocal.c',

                 'lib/librte_eal/common/hotplug_mp.c',
                 'lib/librte_eal/common/malloc_elem.c',
                 'lib/librte_eal/common/malloc_heap.c',
                 'lib/librte_eal/common/malloc_mp.c',
                 'lib/librte_eal/common/rte_keepalive.c',
                 'lib/librte_eal/common/rte_malloc.c',
                 'lib/librte_eal/common/rte_service.c',
                 'lib/librte_eal/common/rte_random.c',

                'lib/librte_eal/unix/eal_unix_timer.c',
                'lib/librte_eal/unix/eal_unix_memory.c',
                'lib/librte_eal/unix/eal_file.c',
                'lib/librte_eal/unix/rte_thread.c',

                 'lib/librte_eal/linux/eal.c',
                 'lib/librte_eal/linux/eal_alarm.c',
                 'lib/librte_eal/linux/eal_cpuflags.c',
                 'lib/librte_eal/linux/eal_debug.c',
                 'lib/librte_eal/linux/eal_hugepage_info.c',
                 'lib/librte_eal/linux/eal_interrupts.c',
                 'lib/librte_eal/linux/eal_lcore.c',
                 'lib/librte_eal/linux/eal_log.c',
                 'lib/librte_eal/linux/eal_memalloc.c',
                 'lib/librte_eal/linux/eal_memory.c',
                 'lib/librte_eal/linux/eal_thread.c',
                 'lib/librte_eal/linux/eal_timer.c',
                 'lib/librte_eal/linux/eal_vfio_mp_sync.c',
                 'lib/librte_eal/linux/eal_vfio.c',
                 'lib/librte_eal/linux/eal_dev.c',

                 'lib/librte_ethdev/rte_ethdev.c',
                 'lib/librte_ethdev/rte_flow.c',

                 'lib/librte_ethdev/ethdev_trace_points.c',
                 'lib/librte_ethdev/ethdev_private.c',
                 'lib/librte_ethdev/rte_class_eth.c',
                 'lib/librte_ethdev/ethdev_profile.c',
                 'lib/librte_ethdev/rte_mtr.c',
                 'lib/librte_ethdev/rte_tm.c',
                                  
                 'lib/librte_telemetry/telemetry.c',
                 'lib/librte_telemetry/telemetry_data.c',
                 'lib/librte_telemetry/telemetry_legacy.c',


                 'lib/librte_hash/rte_cuckoo_hash.c',
                 'lib/librte_kvargs/rte_kvargs.c',
                 'lib/librte_mbuf/rte_mbuf.c',
                 'lib/librte_mbuf/rte_mbuf_dyn.c',
                 'lib/librte_mbuf/rte_mbuf_ptype.c',
                 'lib/librte_mbuf/rte_mbuf_pool_ops.c',
                 'lib/librte_mempool/rte_mempool.c',
                 'lib/librte_mempool/rte_mempool_ops.c',
                 'lib/librte_mempool/rte_mempool_ops_default.c',
                 'lib/librte_mempool/mempool_trace_points.c',

                 'lib/librte_net/rte_ether.c',
                 'lib/librte_net/rte_net.c',
                 'lib/librte_net/rte_net_crc.c',
                 'lib/librte_net/rte_arp.c',
                 'lib/librte_pci/rte_pci.c',
                 'lib/librte_ring/rte_ring.c',
                 'lib/librte_timer/rte_timer.c',
                 'lib/librte_gso/rte_gso.c',
                 'lib/librte_gso/gso_tunnel_udp4.c',
                 'lib/librte_gso/gso_common.c',
                 'lib/librte_gso/gso_tcp4.c',
                 'lib/librte_gso/gso_tunnel_tcp4.c',
                 'lib/librte_gso/gso_udp4.c',

            ])

ntacc_dpdk_src = SrcGroup(dir='src/dpdk/drivers/net/ntacc',
                src_list=[
                 'filter_ntacc.c',
                 'rte_eth_ntacc.c',
                 'nt_compat.c',
            ])

libmnl_src = SrcGroup(
    dir = 'external_libs/libmnl/src',
    src_list = [
        'socket.c',
        'callback.c',
        'nlmsg.c',
        'attr.c',
    ])

i40e_dpdk_src = SrcGroup(
    dir = 'src/dpdk/drivers/net/i40e',
    src_list = [
        'base/i40e_adminq.c',
        'base/i40e_common.c',
        'base/i40e_dcb.c',
        'base/i40e_diag.c',
        'base/i40e_hmc.c',
        'base/i40e_lan_hmc.c',
        'base/i40e_nvm.c',
        'i40e_hash.c',
        'i40e_ethdev.c',
        'i40e_ethdev_vf.c',
        'i40e_fdir.c',
        'i40e_flow.c',
        'i40e_pf.c',
        'i40e_rxtx.c',
        'i40e_tm.c',
        'i40e_vf_representor.c',
        'rte_pmd_i40e.c',
    ])

mlx5_x86_64_dpdk_src = SrcGroup(
    dir = 'src/dpdk/drivers/',
    src_list = [
        'common/mlx5/mlx5_common.c',
        'common/mlx5/mlx5_malloc.c',
        'common/mlx5/mlx5_common_pci.c',
        'common/mlx5/mlx5_common_mr.c',
        'common/mlx5/mlx5_common_devx.c',
        'common/mlx5/mlx5_common_mp.c',
        'common/mlx5/mlx5_devx_cmds.c',

        'common/mlx5/linux/mlx5_common_verbs.c',
        'common/mlx5/linux/mlx5_common_os.c',
        'common/mlx5/linux/mlx5_glue.c',
        'common/mlx5/linux/mlx5_nl.c',


        'net/mlx5/mlx5_rxq.c',
        'net/mlx5/mlx5.c',
        'net/mlx5/mlx5_flow_dv.c',
        'net/mlx5/mlx5_ethdev.c',
        'net/mlx5/mlx5_mr.c',
        'net/mlx5/mlx5_trigger.c',
        'net/mlx5/mlx5_flow_verbs.c',
        'net/mlx5/mlx5_devx.c',
        'net/mlx5/mlx5_stats.c',
        'net/mlx5/mlx5_flow.c',
        'net/mlx5/mlx5_txpp.c',
        'net/mlx5/mlx5_txq.c',
        'net/mlx5/mlx5_mac.c',
        'net/mlx5/mlx5_flow_meter.c',
        'net/mlx5/mlx5_rss.c',

        'net/mlx5/linux/mlx5_verbs.c',
        'net/mlx5/linux/mlx5_os.c',
        'net/mlx5/linux/mlx5_mp_os.c',
        'net/mlx5/linux/mlx5_ethdev_os.c',
        'net/mlx5/linux/mlx5_flow_os.c',
        'net/mlx5/linux/mlx5_socket.c',
        'net/mlx5/linux/mlx5_vlan_os.c',
        'net/mlx5/mlx5_utils.c',
        'net/mlx5/mlx5_rxtx.c',
        'net/mlx5/mlx5_vlan.c',
        'net/mlx5/mlx5_flow_age.c',
        'net/mlx5/mlx5_rxtx_vec.c',
        'net/mlx5/mlx5_rxmode.c',

    ])

mlx5_ppc64le_dpdk_src = SrcGroup(
    dir = 'src/dpdk/drivers/net/mlx5',
    src_list = [
        'mlx5.c',
        'mlx5_devx_cmds.c',
        'mlx5_ethdev.c',
        'mlx5_flow.c',
        'mlx5_flow_dv.c',
        'mlx5_flow_tcf.c',
        'mlx5_flow_verbs.c',
        'mlx5_glue.c',
        'mlx5_mac.c',
        'mlx5_mp.c',
        'mlx5_mr.c',
        'mlx5_nl.c',
        'mlx5_rss.c',
        'mlx5_rxmode.c',
        'mlx5_rxq.c',
        'mlx5_rxtx.c',
        'mlx5_stats.c',
        'mlx5_trigger.c',
        'mlx5_txq.c',
        'mlx5_vlan.c',
    ])


bnxt_dpdk_src = SrcGroup(dir='src/dpdk/',
                src_list=[

                 'drivers/net/bnxt/bnxt_cpr.c',
                 'drivers/net/bnxt/bnxt_ethdev.c',
                 'drivers/net/bnxt/bnxt_filter.c',
                 'drivers/net/bnxt/bnxt_flow.c',
                 'drivers/net/bnxt/bnxt_hwrm.c',
                 'drivers/net/bnxt/bnxt_irq.c',
                 'drivers/net/bnxt/bnxt_ring.c',
                 'drivers/net/bnxt/bnxt_rxq.c',
                 'drivers/net/bnxt/bnxt_rxr.c',
                 'drivers/net/bnxt/bnxt_stats.c',
                 'drivers/net/bnxt/bnxt_txq.c',
                 'drivers/net/bnxt/bnxt_txr.c',
                 'drivers/net/bnxt/bnxt_util.c',
                 'drivers/net/bnxt/bnxt_vnic.c',
                 'drivers/net/bnxt/bnxt_rxtx_vec_sse.c',
                 'drivers/net/bnxt/rte_pmd_bnxt.c',
            ])

memif_dpdk_src = SrcGroup(
    dir = 'src/dpdk/drivers/net/memif',
    src_list = [
        'memif_socket.c',
        'rte_eth_memif.c',
    ])


libmnl =SrcGroups([
                libmnl_src
                ])

ntacc_dpdk =SrcGroups([
                ntacc_dpdk_src
                ])

i40e_dpdk =SrcGroups([
                i40e_dpdk_src
                ])

mlx5_x86_64_dpdk =SrcGroups([
                mlx5_x86_64_dpdk_src
                ])

mlx5_ppc64le_dpdk =SrcGroups([
                mlx5_ppc64le_dpdk_src
                ])


bnxt_dpdk = SrcGroups([
                bnxt_dpdk_src,
                ])

memif_dpdk = SrcGroups([
                memif_dpdk_src,
                ])

# this is the library dp going to falcon (and maybe other platforms)
bp =SrcGroups([
        main_src,
        cmn_src ,
        net_src ,
        yaml_src,
        json_src,
        md5_src,
        rpc_server_src,

        stx_src,
        stf_src,
        stateless_src,
        astf_batch_src,
        astf_src,

        version_src,
    ])

l2fwd_main_src = SrcGroup(dir='src',
        src_list=[
             'l2fwd/main.c'
             ])


l2fwd =SrcGroups([
                l2fwd_main_src])


# common flags for both new and old configurations
common_flags = ['-DWIN_UCODE_SIM',
                '-D_BYTE_ORDER',
                '-D_LITTLE_ENDIAN',
                '-DLINUX',
                '-g',
                '-Wno-format',
                '-Wno-packed-not-aligned',
                '-Wno-missing-field-initializers',
                '-Wno-deprecated-declarations',
                '-Wno-error=uninitialized',
                '-DRTE_DPDK',
                #'-Wcast-qual',
                '-D__STDC_LIMIT_MACROS',
                '-D__STDC_FORMAT_MACROS',
                '-D__STDC_CONSTANT_MACROS',
                '-D_GNU_SOURCE',
                '-DALLOW_INTERNAL_API',
                '-DABI_VERSION="21.1"',
                '-DALLOW_EXPERIMENTAL_API',
                #'-D_GLIBCXX_USE_CXX11_ABI=0', # see libstdc++ ABI changes for string and list
                #'-DTREX_PERF', # used when using TRex and PERF for performance measurement
                #'-D__DEBUG_FUNC_ENTRY__', # Added by Ido to debug Flow Stats
                #'-D__TREX_RPC_DEBUG__', # debug RPC dialogue
               ]


if march == 'x86_64':
    common_flags_new = common_flags + [
                    '-march=native',
                    '-mssse3', '-msse4.1', '-mpclmul', 
                    '-DRTE_MACHINE_CPUFLAG_SSE',
                    '-DRTE_MACHINE_CPUFLAG_SSE2',
                    '-DRTE_MACHINE_CPUFLAG_SSE3',
                    '-DRTE_MACHINE_CPUFLAG_SSSE3',
                    '-DRTE_MACHINE_CPUFLAG_SSE4_1',
                    '-DRTE_MACHINE_CPUFLAG_SSE4_2',
                    '-DRTE_MACHINE_CPUFLAG_AES',
                    '-DRTE_MACHINE_CPUFLAG_PCLMULQDQ',
                    '-DRTE_MACHINE_CPUFLAG_AVX',
                    #'-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE3,RTE_CPUFLAG_SSE,RTE_CPUFLAG_SSE2,RTE_CPUFLAG_SSSE3,RTE_CPUFLAG_SSE4_1,RTE_CPUFLAG_SSE4_2,RTE_CPUFLAG_AES,RTE_CPUFLAG_PCLMULQDQ,RTE_CPUFLAG_AVX',
                    '-DTREX_USE_BPFJIT',
                    '-D_GNU_SOURCE',
                    '-DALLOW_INTERNAL_API',
                    '-DABI_VERSION="21.1"',
                    '-DALLOW_EXPERIMENTAL_API',

                   ]

    common_flags_old = common_flags + [
                      '-march=corei7',
                      '-DUCS_210',
                      '-mtune=generic',
                      '-DRTE_MACHINE_CPUFLAG_SSE',
                      '-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE',
                      '-DTREX_USE_BPFJIT',
                      '-DALLOW_INTERNAL_API',
                      '-DALLOW_EXPERIMENTAL_API',
                      '-DABI_VERSION="21.1"',

                      ]

elif march == 'aarch64':
    common_flags_new = common_flags + [
                       '-march=native',
                       '-mtune=cortex-a72',
                       '-DRTE_ARCH_64',
                       '-DRTE_FORCE_INTRINSICS',
                       '-DRTE_MACHINE_NEON',
                       '-DRTE_MACHINE_EVTSTRM',
                       '-DRTE_MACHINE_CRC32',
                       '-DRTE_MACHINE_AES',
                       '-DRTE_MACHINE_PMULL',
                       '-DRTE_MACHINE_SHA1',
                       '-DRTE_MACHINE_SHA2',
                       '-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_EVTSTRM,RTE_CPUFLAG_NEON,RTE_CPUFLAG_CRC32,RTE_CPUFLAG_AES,RTE_CPUFLAG_PMULL,RTE_CPUFLAG_SHA1,RTE_CPUFLAG_SHA2',
                       ]
    common_flags_old = common_flags + [
                       '-march=native',
                       '-mtune=cortex-a53',
                       '-DRTE_ARCH_64',
                       '-DRTE_FORCE_INTRINSICS',
                       '-DRTE_MACHINE_NEON',
                       '-DRTE_MACHINE_CRC32',
                       '-DRTE_MACHINE_AES',
                       '-DRTE_MACHINE_PMULL',
                       '-DRTE_MACHINE_SHA1',
                       '-DRTE_MACHINE_SHA2',
                       '-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_NEON,RTE_CPUFLAG_CRC32,RTE_CPUFLAG_AES,RTE_CPUFLAG_PMULL,RTE_CPUFLAG_SHA1,RTE_CPUFLAG_SHA2',
                       ]

elif march == 'ppc64le':
    common_flags_new = common_flags + [
                       '-mcpu=power9',
                       '-DRTE_ARCH_64',
                       '-DRTE_MACHINE_CPUFLAG_PPC64',
                       '-DRTE_MACHINE_CPUFLAG_ALTIVEC',
                       '-DRTE_MACHINE_CPUFLAG_VSX',
                       '-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_PPC64,RTE_CPUFLAG_ALTIVEC,RTE_CPUFLAG_VSX',
                       '-DTREX_USE_BPFJIT',
                       ]
    common_flags_old = common_flags + [
                       '-mcpu=power9',
                       '-DRTE_ARCH_64',
                       '-DRTE_MACHINE_CPUFLAG_PPC64',
                       '-DRTE_MACHINE_CPUFLAG_ALTIVEC',
                       '-DRTE_MACHINE_CPUFLAG_VSX',
                       '-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_PPC64,RTE_CPUFLAG_ALTIVEC,RTE_CPUFLAG_VSX',
                       '-DTREX_USE_BPFJIT',
                       ]

dpdk_includes_path_x86_64 ='''
                        ../src/dpdk/lib/librte_eal/x86/include/
                       '''

dpdk_includes_path_aarch64 ='''
                        ../src/dpdk/lib/librte_eal/arm/include/
                       '''

dpdk_includes_path_ppc64le ='''
                        ../src/dpdk/lib/librte_eal/ppc_64/include/
                       '''

dpdk_includes_path =''' ../src/
                        ../src/pal/linux_dpdk/
                        ../src/pal/linux_dpdk/dpdk_2102_'''+ march +'''/
                        ../src/dpdk/drivers/
                        ../src/dpdk/drivers/common/mlx5/
                        ../src/dpdk/drivers/common/mlx5/linux/
                        ../src/dpdk/drivers/net/mlx5/linux/
                        ../src/dpdk/drivers/net/
                        ../src/dpdk/drivers/net/af_packet/
                        ../src/dpdk/drivers/net/tap/
                        ../src/dpdk/drivers/net/failsafe/
                        ../src/dpdk/drivers/net/vdev_netvsc/
                        ../src/dpdk/drivers/net/e1000/
                        ../src/dpdk/drivers/net/e1000/base/
                        ../src/dpdk/drivers/net/enic/
                        ../src/dpdk/drivers/net/enic/base/
                        ../src/dpdk/drivers/net/i40e/
                        ../src/dpdk/drivers/net/i40e/base/
                        ../src/dpdk/drivers/net/ixgbe/
                        ../src/dpdk/drivers/net/ixgbe/base/
                        ../src/dpdk/drivers/net/mlx4/
                        ../src/dpdk/drivers/net/mlx5/
                        ../src/dpdk/drivers/net/ntacc/
                        ../src/dpdk/drivers/net/virtio/
                        ../src/dpdk/drivers/net/virtio/virtio_user/
                        ../src/dpdk/drivers/net/vmxnet3/
                        ../src/dpdk/drivers/net/vmxnet3/base
                        ../src/dpdk/drivers/net/bnxt/
                        ../src/dpdk/drivers/net/bnxt/hcapi/
                        ../src/dpdk/drivers/net/bnxt/tf_core/
                        ../src/dpdk/drivers/net/bnxt/tf_ulp/
                        ../src/dpdk/drivers/net/memif/
                        ../src/dpdk//drivers/common/iavf/                        

                        ../src/dpdk/drivers/net/ena/
                        ../src/dpdk/drivers/net/ena/base/
                        ../src/dpdk/drivers/net/ena/base/ena_defs/
                         
                        ../src/dpdk/lib/librte_telemetry/
                        ../src/dpdk/lib/librte_rcu/

                        ../src/dpdk/lib/
                        ../src/dpdk/lib/librte_cfgfile/
                        ../src/dpdk/lib/librte_compat/
                        ../src/dpdk/lib/librte_eal/
                        ../src/dpdk/lib/librte_eal/include/                        
                        ../src/dpdk/lib/librte_eal/common/
                        ../src/dpdk/lib/librte_eal/common/include/
                        ../src/dpdk/lib/librte_eal/common/include/arch/

                        ../src/dpdk/lib/librte_eal/common/include/generic/
                        ../src/dpdk/lib/librte_eal/linux/
                        ../src/dpdk/lib/librte_eal/linux/eal/
                        ../src/dpdk/lib/librte_eal/linux/include/
                        ../src/dpdk/lib/librte_eal/linux/eal/include/
                        ../src/dpdk/lib/librte_eal/linux/eal/include/exec-env/
                        ../src/dpdk/lib/librte_ethdev/
                        ../src/dpdk/lib/librte_hash/
                        ../src/dpdk/lib/librte_gso/
                        ../src/dpdk/lib/librte_kvargs/
                        ../src/dpdk/lib/librte_mbuf/
                        ../src/dpdk/lib/librte_mempool/
                        ../src/dpdk/lib/librte_meter/
                        ../src/dpdk/lib/librte_net/
                        ../src/dpdk/lib/librte_pci/
                        ../src/dpdk/lib/librte_port/
                        ../src/dpdk/lib/librte_ring/
                        ../src/dpdk/lib/librte_timer/
                        ../src/dpdk/lib/librte_ip_frag/
                        ../src/dpdk/
                        
                        ../src/dpdk/lib/librte_security/

                        ../src/dpdk/drivers/bus/pci/
                        ../src/dpdk/drivers/bus/vdev/
                        ../src/dpdk/drivers/bus/vmbus/
                        ../src/dpdk/drivers/bus/pci/linux/
                        ../src/dpdk/drivers/bus/vmbus/linux/
                        ../external_libs/dpdk_linux_tap_cross/

                    '''

# Include arch specific folder before generic folders
if march == 'x86_64':
    dpdk_includes_path = dpdk_includes_path_x86_64 + dpdk_includes_path
elif march == 'aarch64':
    dpdk_includes_path = dpdk_includes_path_aarch64 + dpdk_includes_path
elif march == 'ppc64le':
    dpdk_includes_path = dpdk_includes_path_ppc64le + dpdk_includes_path


includes_path = '''
                   ../src/
                   ../src/drivers/
                   ../src/pal/common/
                   ../src/pal/linux_dpdk/
                   ../src/stx/
                   ../src/stx/common/
                   ../src/utils/
                   ../src/hdrh/
                   ../external_libs/yaml-cpp/include/
                   ../external_libs/zmq/''' + march + '''/include/
                   ../external_libs/json/
                   ../external_libs/md5/
                   ../external_libs/bpf/
                   ../external_libs/valijson/include/
                  '''


tcp_includes_path = '../src/44bsd ../src/44bsd/netinet'

bpf_includes_path = '../external_libs/bpf ../external_libs/bpf/bpfjit'


if march == 'x86_64':
    DPDK_FLAGS=['-DTAP_MAX_QUEUES=16','-D_GNU_SOURCE', '-DPF_DRIVER', '-DX722_SUPPORT', '-DX722_A0_SUPPORT', '-DVF_DRIVER', '-DINTEGRATED_VF', '-include', '../src/pal/linux_dpdk/dpdk_2102_x86_64/rte_config.h','-DALLOW_INTERNAL_API','-DABI_VERSION="21.1"']
elif march == 'aarch64':
    DPDK_FLAGS=['-DTAP_MAX_QUEUES=16','-D_GNU_SOURCE', '-DPF_DRIVER', '-DVF_DRIVER', '-DINTEGRATED_VF', '-DRTE_FORCE_INTRINSICS', '-include', '../src/pal/linux_dpdk/dpdk_2102_aarch64/rte_config.h']
elif march == 'ppc64le':
    DPDK_FLAGS=['-DTAP_MAX_QUEUES=16','-D_GNU_SOURCE', '-DPF_DRIVER', '-DX722_SUPPORT', '-DX722_A0_SUPPORT', '-DVF_DRIVER', '-DINTEGRATED_VF', '-include', '../src/pal/linux_dpdk/dpdk_2102_ppc64le/rte_config.h']

client_external_libs = [
        'simple_enum',
        'jsonrpclib-pelix-0.4.1',
        'pyyaml-3.11',
        'pyzmq-ctypes',
        'scapy-2.4.3',
        'texttable-0.8.4',
        'simpy-3.0.10',
        'trex-openssl',
        'dpkt-1.9.1',
        'repoze'
        ]

rpath_linkage = ['so', 'so/' + march]

RELEASE_    = "release"
DEBUG_      = "debug"
PLATFORM_x86 = "x86"
PLATFORM_x86_64 = "x86_64"
PLATFORM_aarch64 = "aarch64"
PLATFORM_ppc64le = "ppc64le"


class build_option:

    def __init__(self,debug_mode,is_pie):
      self.mode     = debug_mode   ##debug,release
      self.platform = march  # aarch64 or x86_64 or ppc64le
      self.is_pie = is_pie
      self.env = None

    def set_env(self,env):
        self.env = env

    def is_clang(self):
        if self.env: 
            if 'clang' in self.env[0]:
                return True
        return False        
      
    def __str__(self):
       s=self.mode+","+self.platform
       return (s)

    def lib_name(self,lib_name_p,full_path):
        if full_path:
            return b_path+lib_name_p
        else:
            return lib_name_p
    #private functions
    def toLib (self,name,full_path = True):
        lib_n = "lib"+name+".a"
        return (self.lib_name(lib_n,full_path))

    def toExe(self,name,full_path = True):
        return (self.lib_name(name,full_path))

    def isIntelPlatform (self):
        return ( self.platform == PLATFORM_x86 or self.platform == PLATFORM_x86_64)

    def isArmPlatform (self):
        return ( self.platform == PLATFORM_aarch64)

    def isPpcPlatform (self):
        return ( self.platform == PLATFORM_ppc64le)

    def is64Platform (self):
        return ( self.platform == PLATFORM_x86_64 or self.platform == PLATFORM_aarch64 or self.platform == PLATFORM_ppc64le)

    def isRelease (self):
        return ( self.mode  == RELEASE_)

    def isPIE (self):
        return self.is_pie

    def update_executable_name (self,name):
        return self.update_name(name,"-")

    def update_non_exe_name (self,name):
        return self.update_name(name,"_")

    def update_name (self,name,delimiter):
        trg = copy.copy(name)
        if self.is64Platform():
            trg += delimiter + "64"
        else:
            trg += delimiter + "32"

        if self.isRelease () :
            trg += ""
        else:
            trg +=  delimiter + "debug"

        if self.isPIE():
            trg += delimiter + "o"
        return trg

    def get_target (self):
        return self.update_executable_name("_t-rex")

    def get_target_l2fwd (self):
        return self.update_executable_name("l2fwd")

    def get_dpdk_target (self):
        return self.update_executable_name("dpdk")

    def get_ntacc_target (self):
        return self.update_executable_name("ntacc")

    def get_mlx5_target (self):
        return self.update_executable_name("mlx5")

    def get_libmnl_target (self):
        return self.update_executable_name("mnl")

    def get_mlx4_target (self):
        return self.update_executable_name("mlx4")

    def get_ntaccso_target (self):
        return self.update_executable_name("libntacc")+'.so'

    def get_mlx5so_target (self):
        return self.update_executable_name("libmlx5")+'.so'

    def get_libmnlso_target (self):
        return self.update_executable_name("libmnl") + '.so'

    def get_mlx4so_target (self):
        return self.update_executable_name("libmlx4")+'.so'

    def get_bpf_target (self):
        return self.update_executable_name("bpf")

    def get_bpfso_target (self):
        return self.update_executable_name("libbpf") + '.so'

    def get_bnxt_target (self):
        return self.update_executable_name("bnxt")

    def get_bnxtso_target (self):
        return self.update_executable_name("libbnxt")+'.so'

    def get_tcp_target(self):
        return self.update_executable_name("tcp")

    def get_mlx5_flags(self):
        flags=[]
        if self.isRelease () :
            flags += ['-DNDEBUG']
        else:
            flags += ['-UNDEBUG','-DRTE_LIBRTE_MLX5_DEBUG']

        flags += ['-std=c11','-D_BSD_SOURCE','-D_DEFAULT_SOURCE','-D_XOPEN_SOURCE=600','-D_FILE_OFFSET_BITS=64']

        return (flags)

    def get_mlx4_flags(self, bld):
        flags=[]
        if self.isRelease () :
            flags += ['-DNDEBUG']
        else:
            flags += ['-UNDEBUG']
        return (flags)

    def get_bnxt_flags(self):
        flags=[]
        if self.isRelease () :
            flags += ['-DNDEBUG']
        else:
            flags += ['-UNDEBUG']
        return (flags)

    def get_common_flags (self):
        if self.isPIE():
            flags = copy.copy(common_flags_old)
        else:
            flags = copy.copy(common_flags_new)

        if self.isIntelPlatform():
            if self.is64Platform():
                flags += ['-m64']
            else:
                flags += ['-m32']

        if self.isRelease():
            flags += ['-O3']
        else:
            flags += ['-O0','-D_DEBUG']

        return (flags)

    def get_cxx_flags (self, is_sanitized):
        flags = self.get_common_flags()

        # support c++ 2011
        flags += ['-std=c++0x']

        if self.is_clang():
            flags += clang_flags
        else:
            flags += gcc_flags

        if (self.isIntelPlatform() or self.isPpcPlatform()) and not self.is_clang():
            flags += [
                      '-Wno-aligned-new'
                     ]

        if is_sanitized:
            flags += [
                '-fsanitize=address',
                '-fsanitize=leak',
                '-fno-omit-frame-pointer',
        ]

        return (flags)

        
    def get_c_flags (self, is_sanitized):
        
        flags = self.get_common_flags()
        
        if  self.isRelease () :
            flags += ['-DNDEBUG']

        if self.is_clang():
           flags += clang_flags

        if is_sanitized:
            flags += [
                '-fsanitize=address',
                '-fsanitize=leak',
                '-fno-omit-frame-pointer',
            ]

        # for C no special flags yet
        return (flags)

        
    def get_link_flags(self, is_sanitized):
        base_flags = ['-rdynamic']
        if self.is64Platform() and self.isIntelPlatform():
            base_flags += ['-m64']
        base_flags += ['-lrt']


        if is_sanitized:
            base_flags += [
                '-fsanitize=address',
                '-fsanitize=leak',
                '-static-libasan',
            ]
        return base_flags



build_types = [
               build_option(debug_mode= DEBUG_, is_pie = False),
               build_option(debug_mode= RELEASE_, is_pie = False),
               build_option(debug_mode= DEBUG_, is_pie = True),
               build_option(debug_mode= RELEASE_, is_pie = True),
              ]


def build_prog (bld, build_obj):

    #rte_libs =[
    #         'dpdk']

    #rte_libs1 = rte_libs+rte_libs+rte_libs

    #for obj in rte_libs:
    #    bld.read_shlib( name=obj , paths=[top+rte_lib_path] )

    # add electric fence only for debug image
    debug_file_list=''
    #if not build_obj.isRelease ():
    #    debug_file_list +=ef_src.file_list(top)
    
    build_obj.set_env(bld.env.CXX)

    cflags    = build_obj.get_c_flags(bld.env.SANITIZED)
    cxxflags  = build_obj.get_cxx_flags(bld.env.SANITIZED)
    linkflags = build_obj.get_link_flags(bld.env.SANITIZED)

    lib_ext = []
    if bld.env.DPDK_NEW_MEMORY == True:
        cxxflags.append('-DDPDK_NEW_MEMORY')
        if H_DPDK_CONFIG not in DPDK_FLAGS:
            DPDK_FLAGS.extend(['-include', H_DPDK_CONFIG])
        lib_ext.append('numa')

    if march == 'x86_64':
        bp_dpdk = SrcGroups([
                    dpdk_src,
                    i40e_dpdk_src,
                    dpdk_src_x86_64,
                    dpdk_src_x86_64_ext
                    ])
        
        if bld.env.TAP:
            bp_dpdk.list_group.append(dpdk_src_x86_64_tap)

        # BPF + JIT
        bpf = SrcGroups([
                    bpf_src,
                    bpfjit_src])

    elif march == 'aarch64':
        bp_dpdk = SrcGroups([
                    dpdk_src,
                    dpdk_src_aarch64
                    ])

        # software BPF
        bpf = SrcGroups([bpf_src])

    elif march == 'ppc64le':
        bp_dpdk = SrcGroups([
                    dpdk_src,
                    i40e_dpdk_src,
                    dpdk_src_ppc64le
                    ])

        # BPF + JIT
        bpf = SrcGroups([
                    bpf_src,
                    bpfjit_src])



    bld.objects(
      features='c ',
      includes = dpdk_includes_path,
      cflags   = (cflags + DPDK_FLAGS ),
      source   = bp_dpdk.file_list(top),
      target=build_obj.get_dpdk_target()
      )

    if bld.env.NO_MLX != 'all':
        if not bld.env.LIB_MNL:
            bld.shlib(
                features='c',
                includes = bld.env.libmnl_path,
                cflags   = (cflags),
                source   = libmnl.file_list(top),
                target   = build_obj.get_libmnl_target()
            )
            bld.env.mlx5_use = [build_obj.get_libmnl_target()]

        if bld.env.NO_MLX != 'mlx5':
            if march == 'x86_64':
                bld.shlib(
                  features='c',
                  includes = dpdk_includes_path +
                             bld.env.dpdk_includes_verb_path +
                             bld.env.libmnl_path,
                  cflags   = (cflags + DPDK_FLAGS + build_obj.get_mlx5_flags() ),
                  use      = ['ibverbs','mlx5'] + bld.env.mlx5_use,
                  source   = mlx5_x86_64_dpdk.file_list(top),
                  target   = build_obj.get_mlx5_target(),
                  **bld.env.mlx5_kw
                )
            elif march == 'ppc64le':
                bld.shlib(
                  features='c',
                  includes = dpdk_includes_path +
                             bld.env.dpdk_includes_verb_path +
                             bld.env.libmnl_path,
                  cflags   = (cflags + DPDK_FLAGS + build_obj.get_mlx5_flags() ),
                  use      = ['ibverbs','mlx5'] + bld.env.mlx5_use,
                  source   = mlx5_ppc64le_dpdk.file_list(top),
                  target   = build_obj.get_mlx5_target(),
                  **bld.env.mlx5_kw
                )

        if False: #bld.env.NO_MLX != 'mlx4':
            bld.shlib(
            features='c',
            includes = dpdk_includes_path +
                       bld.env.dpdk_includes_verb_path,
            cflags   = (cflags + DPDK_FLAGS + build_obj.get_mlx4_flags(bld) ),
            use      = ['ibverbs', 'mlx4'],
            source   = mlx4_dpdk.file_list(top),
            target   = build_obj.get_mlx4_target()
           )

    if bld.env.WITH_NTACC == True:
        bld.shlib(
          features='c',
          includes = dpdk_includes_path +
                     bld.env.dpdk_includes_verb_path,
          cflags   = (cflags + DPDK_FLAGS +
            ['-I/opt/napatech3/include',
             '-DNAPATECH3_LIB_PATH=\"/opt/napatech3/lib\"']),
          use =['ntapi'],
          source   = ntacc_dpdk.file_list(top),
          target   = build_obj.get_ntacc_target())

    # build the BPF as a shared library
    bld.shlib(features = 'c',
              includes = bpf_includes_path,
              cflags   = cflags + ['-DSLJIT_CONFIG_AUTO=1','-DINET6'],
              source   = bpf.file_list(top),
              target   = build_obj.get_bpf_target())

    bld.stlib(features = 'c',
              includes = tcp_includes_path,
              cflags   = cflags + ['-DINET', '-DINET6', '-DTREX_FBSD', '-fno-builtin'],
              source   = tcp_src.file_list(top),
              target   = build_obj.get_tcp_target())

    inc_path = dpdk_includes_path + includes_path
    cxxflags_ext = ['',]

    bld.program(features='cxx cxxprogram',
                includes =inc_path + tcp_includes_path,
                cxxflags = ( cxxflags + ['-std=gnu++11', '-DTREX_FBSD']),
                linkflags = linkflags ,
                lib=['pthread','dl', 'z'] + lib_ext,
                use =[build_obj.get_dpdk_target(), build_obj.get_bpf_target(), 'zmq', build_obj.get_tcp_target()],
                source = bp.file_list(top) + debug_file_list,
                rpath = rpath_linkage,
                target = build_obj.get_target())

    if bld.env.NO_BNXT == False:
        bld.shlib(
          features='c',
          includes = dpdk_includes_path,
          cflags   = (cflags + DPDK_FLAGS + build_obj.get_bnxt_flags() ),
          use =['bnxt'],
          source   = bnxt_dpdk.file_list(top),
          target   = build_obj.get_bnxt_target()
        )




def build_type(bld,build_obj):
    build_prog(bld, build_obj)


def post_build(bld):

    print("*** generating softlinks ***")
    exec_p ="../scripts/"

    if bld.env.WITH_BIRD:
        bird_links_dir = os.path.realpath(bird_path)
        compile_bird.create_links(src_dir=bld.out_dir + '/linux_dpdk/bird',
                                    links_dir=bird_links_dir)

    for obj in build_types:
        if bld.options.no_old and obj.is_pie:
            continue
        install_single_system(bld, exec_p, obj)


def check_build_options(bld):
    err_template = 'Should use %s flag at configuration stage, not in build.'
    if bld.options.sanitized and not bld.env.SANITIZED:
        bld.fatal(err_template % 'sanitized')
    if bld.options.gcc6 and bld.env.CC_VERSION[0] != '6':
        bld.fatal(err_template % 'gcc6')
    if bld.options.gcc7 and bld.env.CC_VERSION[0] != '7':
        bld.fatal(err_template % 'gcc7')


def build(bld):
    check_build_options(bld)
    if bld.env.SANITIZED and bld.cmd == 'build':
        Logs.warn("\n******* building sanitized binaries *******\n")

    bld.env.dpdk_includes_verb_path = ''
    bld.add_pre_fun(pre_build)
    bld.add_post_fun(post_build)

    # ZMQ
    zmq_lib_path='external_libs/zmq/' + march + '/'
    bld.read_shlib( name='zmq' , paths=[top + zmq_lib_path] )

    if bld.env.NO_MLX != 'all':
        if bld.env.LIB_IBVERBS:
            Logs.pprint('GREEN', 'Info: Using external libverbs.')
            if not bld.env.LD_SEARCH_PATH:
                bld.fatal('LD_SEARCH_PATH is empty, run configure')
            from waflib.Tools.ccroot import SYSTEM_LIB_PATHS
            SYSTEM_LIB_PATHS.extend(bld.env.LD_SEARCH_PATH)

            bld.read_shlib(name='ibverbs')
            if bld.env.NO_MLX != 'mlx5':
                bld.read_shlib(name='mlx5')
            #if bld.env.NO_MLX != 'mlx4':
            #    bld.read_shlib(name='mlx4')
        else:
            Logs.pprint('GREEN', 'Info: Using internal libverbs.')
            ibverbs_lib_path='external_libs/ibverbs/' + march
            bld.env.dpdk_includes_verb_path = ' \n ../external_libs/ibverbs/' + march + '/include/ \n'
            bld.read_shlib( name='ibverbs' , paths=[top+ibverbs_lib_path] )
            if bld.env.NO_MLX != 'mlx5':
                bld.read_shlib( name='mlx5',paths=[top+ibverbs_lib_path])
            #if bld.env.NO_MLX != 'mlx4':
            #    bld.read_shlib( name='mlx4',paths=[top+ibverbs_lib_path])
            check_ibverbs_deps(bld)

        if bld.env.LIB_MNL:
            Logs.pprint('GREEN', 'Info: Using external libmnl.')
            bld.env.libmnl_path = ''
            bld.env.mlx5_use = []
            bld.env.mlx5_kw = {'lib': 'mnl'}
        else:
            Logs.pprint('GREEN', 'Info: Using internal libmnl.')
            bld.env.libmnl_path=' \n ../external_libs/libmnl/include/ \n'
            bld.env.mlx5_kw  = {}

    if bld.env.WITH_BIRD:
        bld(rule=build_bird, source='compile_bird.py', target='bird')

    for obj in build_types:
        if bld.options.no_old and obj.is_pie:
            continue
        build_type(bld,obj)


def build_info(bld):
    pass

def build_bird(task):
    bird_build_dir = str(task.get_cwd()) + '/linux_dpdk'
    compile_bird.build_bird(dst = bird_build_dir, is_verbose = Logs.verbose)

def compare_link(link_path, dst_path):
    return os.path.abspath(os.readlink(link_path)) == os.path.abspath(dst_path)

def do_create_link (src, name, where):
    '''
        creates a soft link
        'src'        - path to the source file
        'name'       - link name to be used
        'where'      - where to put the symbolic link
    '''
    # verify that source exists
    if os.path.exists(src):

        full_link = os.path.join(where, name)
        rel_path  = os.path.relpath(src, where)

        if os.path.islink(full_link):
            if compare_link(full_link, rel_path):
                return
            os.unlink(full_link)

        if not os.path.lexists(full_link):
            print('{0} --> {1}'.format(name, rel_path))
            os.symlink(rel_path, full_link)


def install_single_system (bld, exec_p, build_obj):

    o = bld.out_dir+'/linux_dpdk/'

    # executable
    do_create_link(src = os.path.realpath(o + build_obj.get_target()),
                   name = build_obj.get_target(),
                   where = exec_p)

    # SO libraries below

    # NTACC
    do_create_link(src = os.path.realpath(o + build_obj.get_ntaccso_target()),
                   name = build_obj.get_ntaccso_target(),
                   where = so_path)

    # MLX5
    do_create_link(src = os.path.realpath(o + build_obj.get_mlx5so_target()),
                   name = build_obj.get_mlx5so_target(),
                   where = so_path)

    # MLX4
    #do_create_link(src = os.path.realpath(o + build_obj.get_mlx4so_target()),
    #               name = build_obj.get_mlx4so_target(),
    #               where = so_path)

    # MNL
    do_create_link(src   = os.path.realpath(o + build_obj.get_libmnlso_target()),
                   name  = build_obj.get_libmnlso_target(),
                   where = so_path)

    # BPF
    do_create_link(src   = os.path.realpath(o + build_obj.get_bpfso_target()),
                   name  = build_obj.get_bpfso_target(),
                   where = so_path)

    # BNXT
    do_create_link(src = os.path.realpath(o + build_obj.get_bnxtso_target()),
                   name = build_obj.get_bnxtso_target(),
                   where = so_path)


def pre_build(bld):
    if not bld.options.no_ver:
        print("update version files")
        create_version_files()


def write_file (file_name,s):
    with open(file_name,'w') as f:
        f.write(s)


def get_build_num ():
    s=''
    if os.path.isfile(BUILD_NUM_FILE):
        f=open(BUILD_NUM_FILE,'r')
        s+=f.readline().rstrip()
        f.close()
    return s

def create_version_files ():
    git_sha="N/A"
    try:
      r=getstatusoutput("git log --pretty=format:'%H' -n 1")
      if r[0]==0:
          git_sha=r[1]
    except :
        pass


    s =''
    s +="#ifndef __TREX_VER_FILE__           \n"
    s +="#define __TREX_VER_FILE__           \n"
    s +="#ifdef __cplusplus                  \n"
    s +=" extern \"C\" {                        \n"
    s +=" #endif                             \n"
    s +='#define  VERSION_USER  "%s"          \n' % os.environ.get('USER', 'unknown')
    s +='extern const char * get_build_date(void);  \n'
    s +='extern const char * get_build_time(void);  \n'
    s +='#define VERSION_UIID      "%s"       \n' % uuid.uuid1()
    s +='#define VERSION_GIT_SHA   "%s"       \n' % git_sha
    s +='#define VERSION_BUILD_NUM "%s"       \n' % get_build_num()
    s +="#ifdef __cplusplus                  \n"
    s +=" }                        \n"
    s +=" #endif                             \n"
    s +="#endif \n"

    write_file (H_VER_FILE ,s)

    s ='#include "version.h"          \n'
    s +='#define VERSION_UIID1      "%s"       \n' % uuid.uuid1()
    s +="const char * get_build_date(void){ \n"
    s +="    return (__DATE__);       \n"
    s +="}      \n"
    s +=" \n"
    s +="const char * get_build_time(void){ \n"
    s +="    return (__TIME__ );       \n"
    s +="}      \n"

    write_file (C_VER_FILE,s)

def build_test(bld):
    create_version_files ()

def _copy_single_system (bld, exec_p, build_obj):
    o=bld.out_dir+'/linux_dpdk/'
    src_file =  os.path.realpath(o+build_obj.get_target())
    print(src_file)
    if os.path.exists(src_file):
        dest_file = exec_p +build_obj.get_target()
        os.system("cp %s %s " %(src_file,dest_file))
        os.system("strip %s " %(dest_file))
        os.system("chmod +x %s " %(dest_file))

def _copy_single_system1 (bld, exec_p, build_obj):
    o='../scripts/'
    src_file =  os.path.realpath(o+build_obj.get_target()[1:])
    print(src_file)
    if os.path.exists(src_file):
        dest_file = exec_p +build_obj.get_target()[1:]
        os.system("cp %s %s " %(src_file,dest_file))
        os.system("chmod +x %s " %(dest_file))


def copy_single_system (bld, exec_p, build_obj):
    _copy_single_system (bld, exec_p, build_obj)

def copy_single_system1 (bld, exec_p, build_obj):
    _copy_single_system1 (bld, exec_p, build_obj)


files_list=[
            'trex-cfg',
            'bp-sim-64',
            'bp-sim-64-debug',
            't-rex-64-debug-gdb',
            'stl-sim',
            'astf-sim',
            'astf-sim-utl',
            'find_python.sh',
            'run_regression',
            'run_functional_tests',
            'dpdk_nic_bind.py',
            'dpdk_setup_ports.py',
            'doc_process.py',
            'trex_daemon_server',
            'trex-emu',
            'general_daemon_server',
            'master_daemon.py',
            'astf_schema.json',
            'trex-console',
            'daemon_server',
            'ndr'
            ]

pkg_include = ['cap2','avl','cfg','ko','automation', 'external_libs', 'stl','exp','astf','x710_ddp','trex_emu','emu']
pkg_exclude = ['*.pyc', '__pycache__']
pkg_make_dirs = ['generated', 'trex_client/external_libs', 'trex_client/interactive/profiles']


class Env(object):
    @staticmethod
    def get_env(name) :
        s= os.environ.get(name)
        if s == None:
            print("You should define $ %s" % name)
            raise Exception("Env error")
        return (s)

    @staticmethod
    def get_release_path () :
        s= Env().get_env('TREX_LOCAL_PUBLISH_PATH')
        s +=get_build_num ()+"/"
        return  s

    @staticmethod
    def get_remote_release_path () :
        s= Env().get_env('TREX_REMOTE_PUBLISH_PATH')
        return  s

    @staticmethod
    def get_local_web_server () :
        s= Env().get_env('TREX_WEB_SERVER')
        return  s

    # extral web
    @staticmethod
    def get_trex_ex_web_key() :
        s= Env().get_env('TREX_EX_WEB_KEY')
        return  s

    @staticmethod
    def get_trex_ex_web_path() :
        s= Env().get_env('TREX_EX_WEB_PATH')
        return  s

    @staticmethod
    def get_trex_ex_web_user() :
        s= Env().get_env('TREX_EX_WEB_USER')
        return  s

    @staticmethod
    def get_trex_ex_web_srv() :
        s= Env().get_env('TREX_EX_WEB_SRV')
        return  s

    @staticmethod
    def get_trex_regression_workspace():
        return Env().get_env('TREX_REGRESSION_WORKSPACE')


def check_release_permission():
    if os.getenv('USER') not in USERS_ALLOWED_TO_RELEASE:
        raise Exception('You are not allowed to release TRex. Please contact Hanoch.')

# build package in parent dir. can provide custom name and folder with --pkg-dir and --pkg-file
def pkg(ctx):
    build_num = get_build_num()
    pkg_dir = ctx.options.pkg_dir
    if not pkg_dir:
        pkg_dir = os.pardir
    pkg_file = ctx.options.pkg_file
    if not pkg_file:
        pkg_file = '%s.tar.gz' % build_num
    tmp_path = os.path.join(pkg_dir, '_%s' % pkg_file)
    dst_path = os.path.join(pkg_dir, pkg_file)
    build_path = os.path.join(os.pardir, build_num)

    # clean old dir if exists
    os.system('rm -rf %s' % build_path)
    release(ctx, build_path + '/')
    os.system("cp %s/%s.tar.gz %s" % (build_path, build_num, tmp_path))
    os.system("mv %s %s" % (tmp_path, dst_path))

    # clean new dir
    os.system('rm -rf %s' % build_path)
    pkg_size = int(os.path.getsize(dst_path) / 1e6)
    if pkg_size > MAX_PKG_SIZE:
        ctx.fatal('Package size is too large: %sMB (max allowed: %sMB)' % (pkg_size, MAX_PKG_SIZE))

def fix_pkg_include(bld):
    if bld.env.WITH_BIRD:
        pkg_include.append('bird')

def release(ctx, custom_dir = None):
    
    bld = Build.BuildContext()
    bld.load_envs()

    """ release to local folder  """
    if custom_dir:
        exec_p = custom_dir
    else:
        check_release_permission()
        exec_p = Env().get_release_path()
    print("copy images and libs")
    os.system(' mkdir -p '+exec_p)

    # get build context to refer the build output dir
    for obj in build_types:
        copy_single_system(bld,exec_p,obj)
        copy_single_system1(bld,exec_p,obj)

    for obj in files_list:
        src_file =  '../scripts/'+obj
        dest_file = exec_p +'/'+obj
        os.system("cp %s %s " %(src_file,dest_file))
    
    os.system("chmod 755 %s " % (exec_p +'/trex-emu'))
    
    exclude = ' '.join(['--exclude=%s' % exc for exc in pkg_exclude])
    fix_pkg_include(bld)
    for obj in pkg_include:
        src_file =  '../scripts/'+obj+'/'
        dest_file = exec_p +'/'+obj+'/'
        os.system("rsync -r -L -v %s %s %s" % (src_file, dest_file, exclude))
        os.system("chmod 755 %s " %(dest_file))

    for obj in pkg_make_dirs:
        dest_dir = os.path.join(exec_p, obj)
        os.system('mkdir -p %s' % dest_dir)

    # copy .SO objects and resolve symbols
    os.system('cp -rL ../scripts/so {0}'.format(exec_p))

    rel=get_build_num ()

    # create client package
    for ext_lib in client_external_libs:
        os.system('cp ../scripts/external_libs/%s %s/trex_client/external_libs/ -r' % (ext_lib, exec_p))
    os.system('cp ../scripts/automation/trex_control_plane/stf %s/trex_client/ -r' % exec_p)
    os.system('cp ../scripts/automation/trex_control_plane/interactive/ %s/trex_client/ -r' % exec_p)

    os.system('cp ../scripts/stl %s/trex_client/interactive/profiles/ -r' % exec_p)

    shutil.make_archive(os.path.join(exec_p, 'trex_client_%s' % rel), 'gztar', exec_p, 'trex_client')
    os.system('rm -r %s/trex_client' % exec_p)

    os.system('cd %s/..;tar --exclude="*.pyc" -zcvf %s/%s.tar.gz %s' %(exec_p,os.getcwd(),rel,rel))
    os.system("mv %s/%s.tar.gz %s" % (os.getcwd(),rel,exec_p))


def publish(ctx, custom_source = None):
    check_release_permission()
    exec_p = Env().get_release_path()
    rel=get_build_num ()

    release_name ='%s.tar.gz' % (rel)
    if custom_source:
        from_ = custom_source
    else:
        from_ = exec_p+'/'+release_name
    os.system("rsync -av %s %s:%s/%s " %(from_,Env().get_local_web_server(),Env().get_remote_release_path (), release_name))
    if not ctx.options.private:
      os.system("ssh %s 'cd %s;rm be_latest; ln -P %s be_latest'  " %(Env().get_local_web_server(),Env().get_remote_release_path (),release_name))
      os.system("ssh %s 'cd %s;rm latest; ln -P %s latest'  " %(Env().get_local_web_server(),Env().get_remote_release_path (),release_name))


def publish_ext(ctx, custom_source = None):
    check_release_permission()
    exec_p = Env().get_release_path()
    rel=get_build_num ()

    release_name ='%s.tar.gz' % (rel)
    if custom_source:
        from_ = custom_source
    else:
        from_ = exec_p+'/'+release_name
    cmd='rsync -avz --progress -e "ssh -i %s" --rsync-path=/usr/bin/rsync %s %s@%s:%s/release/%s' % (Env().get_trex_ex_web_key(),from_, Env().get_trex_ex_web_user(),Env().get_trex_ex_web_srv(),Env().get_trex_ex_web_path() ,release_name)
    print(cmd)
    os.system( cmd )

    if not ctx.options.private:
      os.system("ssh -i %s -l %s %s 'cd %s/release/;rm be_latest; ln -P %s be_latest'  " %(Env().get_trex_ex_web_key(),Env().get_trex_ex_web_user(),Env().get_trex_ex_web_srv(),Env().get_trex_ex_web_path(),release_name))
      os.system("ssh -i %s -l %s %s 'cd %s/release/;rm latest; ln -P %s latest'  " %(Env().get_trex_ex_web_key(),Env().get_trex_ex_web_user(),Env().get_trex_ex_web_srv(),Env().get_trex_ex_web_path(),release_name))

# publish latest passed regression package (or custom commit from  --publish_commit option) as be_latest to trex-tgn.cisco.com and internal wiki
def publish_both(self):
    check_release_permission()
    packages_dir = Env().get_env('TREX_LOCAL_PUBLISH_PATH') + '/experiment/packages'
    publish_commit = self.options.publish_commit
    if publish_commit:
        package_file = '%s/%s.tar.gz' % (packages_dir, publish_commit)
    else:
        last_passed_commit_file = Env().get_trex_regression_workspace() + '/reports/last_passed_commit'
        with open(last_passed_commit_file) as f:
            last_passed_commit = f.read().strip()
        package_file = '%s/%s.tar.gz' % (packages_dir, last_passed_commit)
    publish(self, custom_source = package_file)
    publish_ext(self, custom_source = package_file)

# print detailed latest passed regression commit + brief info of 5 commits before it
def show(ctx):
    last_passed_commit_file = Env().get_trex_regression_workspace() + '/reports/last_passed_commit'
    with open(last_passed_commit_file) as f:
        last_passed_commit = f.read().strip()

    # last passed nightly
    command = 'timeout 10 git show %s --quiet' % last_passed_commit
    result, output = getstatusoutput(command)
    if result == 0:
        print('Last passed regression commit:\n%s\n' % output)
    else:
        raise Exception('Error getting commit info with command: %s' % command)

    # brief list of 5 commits before passed
    result, output = getstatusoutput('git --version')
    if result != 0 or output.startswith('git version 1'):
        # old format, no color etc.
        command = "timeout 10 git log --no-merges -n 5 --pretty=format:'%%h  %%an  %%ci  %%s' %s^@" % last_passed_commit
    else:
        # new format, with color, padding, truncating etc.
        command = "timeout 10 git log --no-merges -n 5 --pretty=format:'%%C(auto)%%h%%Creset  %%<(10,trunc)%%an  %%ci  %%<(100,trunc)%%s' %s^@ " % last_passed_commit
    result, output = getstatusoutput(command)
    if result == 0:
        print(output)
    else:
        raise Exception('Error getting commits info with command: %s' % command)

def test (ctx):
    r=getstatusoutput("git log --pretty=format:'%H' -n 1")
    if r[0]==0:
        print(r[1])
