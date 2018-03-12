#!/usr/bin/env python
# encoding: utf-8
#
# Hanoh Haim
# Cisco Systems, Inc.

VERSION='0.0.1'
APPNAME='cxx_test'
import os;
import shutil;
import copy;
import re
import uuid
import subprocess
import platform
from waflib import Logs
from waflib.Configure import conf
from waflib import Build
import sys

from distutils.version import StrictVersion

# use hostname as part of cache filename
Build.CACHE_SUFFIX = '_%s_cache.py' % platform.node()

# these variables are mandatory ('/' are converted automatically)
top = '../'
out = 'build_dpdk'
march = os.uname()[4]


b_path ="./build/linux_dpdk/"

so_path = '../scripts/so'

C_VER_FILE      = "version.c"
H_VER_FILE      = "version.h"

BUILD_NUM_FILE  = "../VERSION"
USERS_ALLOWED_TO_RELEASE = ['hhaim']


REQUIRED_CC_VERSION = "4.7.0"
SANITIZE_CC_VERSION = "4.9.0"

GCC6_DIR = '/usr/local/gcc-6.2/bin'
GCC7_DIR = '/usr/local/gcc-7.2/bin'


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
      self.dir = dir;
      self.src_list = src_list;
      self.group_list = None;
      assert (type(src_list)==list)
      assert (type(dir)==str)



    def file_list (self,top):
        ' return  the long list of the files '
        res=''
        for file in self.src_list:
            res= res + top+'/'+self.dir+'/'+file+'  ';

        return res;

    def __str__ (self):
        return (self.file_list(''));

    def __repr__ (self):
        return (self.file_list(''));



class SrcGroups:
    ' group of source groups '

    def __init__(self,list_group):
      self.list_group = list_group;
      assert (type(list_group)==list)


    def file_list (self,top):
          ' return  the long list of the files '
          res=''
          for o in self.list_group:
              res += o.file_list(top);
          return res;

    def __str__ (self):
          return (self.file_list(''));

    def __repr__ (self):
          return (self.file_list(''));


def options(opt):
    opt.load('compiler_cxx')
    opt.load('compiler_c')
    opt.add_option('--pkg-dir', '--pkg_dir', dest='pkg_dir', default=False, action='store', help="Destination folder for 'pkg' option.")
    opt.add_option('--pkg-file', '--pkg_file', dest='pkg_file', default=False, action='store', help="Destination filename for 'pkg' option.")
    opt.add_option('--publish-commit', '--publish_commit', dest='publish_commit', default=False, action='store', help="Specify commit id for 'publish_both' option (Please make sure it's good!)")
    opt.add_option('--no-mlx', dest='no_mlx', default=(True if march == 'aarch64' else False), action='store', help="don't use mlx5 dpdk driver. use with ./b configure --no-mlx. no need to run build with it")
    opt.add_option('--with-ntacc', dest='with_ntacc', default=False, action='store_true', help="Use Napatech dpdk driver. Use with ./b configure --with-ntacc.")
    opt.add_option('--no-ver', action = 'store_true', help = "Don't update version file.")
    opt.add_option('--private', dest='private', action = 'store_true', help = "private publish, do not replace latest/be_latest image with this image")

    co = opt.option_groups['configure options']
    co.add_option('--sanitized', dest='sanitized', default=False, action='store_true',
                   help='for GCC {0}+ use address sanitizer to catch memory errors'.format(SANITIZE_CC_VERSION))
    
    co.add_option('--gcc6', dest='gcc6', default=False, action='store_true',
                   help='use GCC 6.2 instead of the machine version')

    co.add_option('--gcc7', dest='gcc7', default=False, action='store_true',
                   help='use GCC 7.2 instead of the machine version')


def check_ibverbs_deps(bld):
    if 'LDD' not in bld.env or not len(bld.env['LDD']):
        bld.fatal('Please run configure. Missing key LDD.')
    cmd = '%s %s/external_libs/ibverbs/libibverbs.so' % (bld.env['LDD'][0], top)
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
    fedora_install = 'Fedora install:\nsudo yum install %s\n' % fedora
    ubuntu_install = 'Ubuntu install:\nsudo apt install %s\n' % ubuntu
    try:
        if platform.linux_distribution()[0].capitalize() == 'Ubuntu':
            msg += ubuntu_install
        elif platform.linux_distribution()[0].capitalize() == 'Fedora':
            msg += fedora_install
        else:
            raise
    except:
        msg += 'Could not determine Linux distribution.\n%s\n%s' % (ubuntu_install, fedora_install)
    return msg


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

    ctx.end_msg('Found needed version %s' % ofed_ver_show)
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
    
    
def configure(conf):

    if conf.options.gcc6 and conf.options.gcc7:
        conf.fatal('--gcc6 and --gcc7 and mutual exclusive')

    if conf.options.gcc6:
        configure_gcc(conf, GCC6_DIR)
    elif conf.options.gcc7:
        configure_gcc(conf, GCC7_DIR)
    else:
        configure_gcc(conf)


    conf.find_program('ldd')
    conf.check_cxx(lib = 'z', errmsg = missing_pkg_msg(fedora = 'zlib-devel', ubuntu = 'zlib1g-dev'))
    no_mlx          = conf.options.no_mlx
    with_ntacc      = conf.options.with_ntacc
    with_sanitized  = conf.options.sanitized
    
    configure_sanitized(conf, with_sanitized)
            
    conf.env.NO_MLX = no_mlx
    if not no_mlx:
        ofed_ok = conf.check_ofed(mandatory = False)
        if ofed_ok:
            conf.check_cxx(lib = 'ibverbs', errmsg = 'Could not find library ibverbs, will use internal version.', mandatory = False)
        else:
            Logs.pprint('YELLOW', 'Warning: will use internal version of ibverbs. If you need to use Mellanox NICs, install OFED:\n' +
                                  'https://trex-tgn.cisco.com/trex/doc/trex_manual.html#_mellanox_connectx_4_support')

    conf.env.WITH_NTACC = with_ntacc
    if with_ntacc:
        ntapi_ok = conf.check_ntapi(mandatory = False)
        if not ntapi_ok:
          Logs.pprint('RED', 'Cannot find NTAPI. If you need to use Napatech NICs, install the Napatech driver:\n' +
                                  'https://www.napatech.com/downloads/')
          raise Exception("Cannot find libntapi");

          

def configure_gcc (conf, explicit_path = None):
    # use the system path
    if explicit_path is None:
        conf.load('gcc')
        conf.load('g++')
        return

    if not os.path.exists(explicit_path):
        conf.fatal('unable to find specific GCC installtion dir: {0}'.format(explicit_path))

    saved = conf.environ['PATH']
    try:
        conf.environ['PATH'] = explicit_path
        conf.load('gcc')
        conf.load('g++')
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
             'bp_sim_tcp.cpp',
             'utl_mbuf.cpp',
             'utl_dbl_human.cpp',
             'utl_counter.cpp',
             'utl_policer.cpp',
             'astf/astf_template_db.cpp',
             'stt_cp.cpp',
             'trex_global.cpp',
             'bp_sim.cpp',
             'trex_platform.cpp',
             'flow_stat_parser.cpp',
             'utl_term_io.cpp',
             'utl_port_map.cpp',
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
             'utl_cpuu.cpp',
             'utl_ip.cpp',
             'utl_yaml.cpp',
             'nat_check.cpp',
             'nat_check_flow_table.cpp',
             'msg_manager.cpp',
             'trex_port_attr.cpp',
             'publisher/trex_publisher.cpp',
             'pal/linux_dpdk/pal_utl.cpp',
             'pal/linux_dpdk/mbuf.cpp',
             'pal/common/common_mbuf.cpp',
             'h_timer.cpp',
             'astf/astf_db.cpp',
             'bp_sim_stf.cpp',
             'utl_sync_barrier.cpp',
             'trex_build_info.cpp',
             'dpdk_drv_filter.cpp'
             ]);

cmn_src = SrcGroup(dir='src/common',
    src_list=[
        'basic_utils.cpp',
        'captureFile.cpp',
        'erf.cpp',
        'pcap.cpp',
        'base64.cpp',
        'n_uniform_prob.cpp'
        ]);

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
           'VLANHeader.cpp']);

# JSON package
json_src = SrcGroup(dir='external_libs/json',
        src_list=[
            'jsoncpp.cpp'
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
        ]);



# common code for STL / ASTF / STF
stx_src = SrcGroup(dir='src/stx/common/',
                           src_list=['trex_stx.cpp',
                                     'trex_pkt.cpp',
                                     'trex_capture.cpp',
                                     'trex_port.cpp',
                                     'trex_dp_port_events.cpp',
                                     'trex_dp_core.cpp',
                                     'trex_messaging.cpp',
                                     
                                     'trex_rx_core.cpp',
                                     'trex_rx_port_mngr.cpp',
                                     'trex_rx_tx.cpp',
                                     
                                     'trex_rpc_cmds_common.cpp'
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
                                    
                                    'trex_stl_messaging.cpp',
                                    
                                    'trex_stl_rpc_cmds.cpp'
                                    
                                    ])



# ASTF batch
astf_batch_src = SrcGroup(dir='src/stx/astf_batch/',
                           src_list=['trex_astf_batch.cpp'])


# ASTF
astf_src = SrcGroup(dir='src/stx/astf/',
                         src_list=['trex_astf.cpp',
                                   'trex_astf_port.cpp',
                                   'trex_astf_rpc_cmds.cpp',
                                   'trex_astf_dp_core.cpp'
                         ])

# JSON package
json_src = SrcGroup(dir='external_libs/json',
                    src_list=[
                        'jsoncpp.cpp'
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
            'tag.cpp']);


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
                 'drivers/net/enic/base/vnic_cq.c',
                 'drivers/net/enic/base/vnic_dev.c',
                 'drivers/net/enic/base/vnic_intr.c',
                 'drivers/net/enic/base/vnic_rq.c',
                 'drivers/net/enic/base/vnic_rss.c',
                 'drivers/net/enic/base/vnic_wq.c',
                 'drivers/net/enic/enic_clsf.c',
                 'drivers/net/enic/enic_flow.c',
                 'drivers/net/enic/enic_ethdev.c',
                 'drivers/net/enic/enic_main.c',
                 'drivers/net/enic/enic_res.c',
                 'drivers/net/enic/enic_rxtx.c',

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
                 'drivers/net/ixgbe/rte_pmd_ixgbe.c',

                 #i40e
                 'drivers/net/i40e/base/i40e_adminq.c',
                 'drivers/net/i40e/base/i40e_common.c',
                 'drivers/net/i40e/base/i40e_dcb.c',
                 'drivers/net/i40e/base/i40e_diag.c',
                 'drivers/net/i40e/base/i40e_hmc.c',
                 'drivers/net/i40e/base/i40e_lan_hmc.c',
                 'drivers/net/i40e/base/i40e_nvm.c',
                 'drivers/net/i40e/i40e_ethdev_vf.c',
                 'drivers/net/i40e/i40e_pf.c',
                 'drivers/net/i40e/i40e_rxtx.c',
                 'drivers/net/i40e/i40e_flow.c',
                 'drivers/net/i40e/i40e_rxtx_vec_sse.c',
                 'drivers/net/i40e/i40e_fdir.c',
                 'drivers/net/i40e/i40e_tm.c',      # add
                 'drivers/net/i40e/rte_pmd_i40e.c', # add
                 'drivers/net/i40e/i40e_ethdev.c',

                 #virtio
                 'drivers/net/virtio/virtio_rxtx_simple_sse.c',

                 #vmxnet3
                 'drivers/net/vmxnet3/vmxnet3_ethdev.c',
                 'drivers/net/vmxnet3/vmxnet3_rxtx.c',

                 #af_packet
                 'drivers/net/af_packet/rte_eth_af_packet.c',

                 #libs
                 'lib/librte_eal/common/arch/x86/rte_cpuflags.c',
                 'lib/librte_eal/common/arch/x86/rte_spinlock.c',
                 'lib/librte_eal/common/arch/x86/rte_cycles.c',
                 'lib/librte_eal/common/arch/x86/rte_memcpy.c',
                 ])


dpdk_src_aarch64 = SrcGroup(dir='src/dpdk/',
        src_list=[
                 #virtio
                 'drivers/net/virtio/virtio_rxtx_simple_neon.c',

                 #libs
                 'lib/librte_eal/common/arch/arm/rte_cpuflags.c',
                 'lib/librte_eal/common/arch/arm/rte_cycles.c',

                 ])


dpdk_src = SrcGroup(dir='src/dpdk/',
                src_list=[
                 '../dpdk_funcs.c',
                 'drivers/bus/pci/pci_common.c',
                 'drivers/bus/pci/pci_common_uio.c',
                 'drivers/bus/pci/linux/pci.c',
                 'drivers/bus/pci/linux/pci_uio.c',
                 'drivers/bus/pci/linux/pci_vfio.c',
                 'drivers/bus/vdev/vdev.c',

                 'drivers/mempool/ring/rte_mempool_ring.c',
                 'drivers/mempool/stack/rte_mempool_stack.c',


                 # drivers
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

                 #virtio
                 'drivers/net/virtio/virtio_ethdev.c',
                 'drivers/net/virtio/virtio_pci.c',
                 'drivers/net/virtio/virtio_rxtx.c',
                 'drivers/net/virtio/virtio_rxtx_simple.c',
                 'drivers/net/virtio/virtqueue.c',
                 'drivers/net/virtio/virtio_user_ethdev.c',
                 'drivers/net/virtio/virtio_user/vhost_kernel.c',
                 'drivers/net/virtio/virtio_user/vhost_kernel_tap.c',
                 'drivers/net/virtio/virtio_user/vhost_user.c',
                 'drivers/net/virtio/virtio_user/virtio_user_dev.c',

                 #libs
                 'lib/librte_cfgfile/rte_cfgfile.c',

                 'lib/librte_eal/common/eal_common_bus.c',
                 'lib/librte_eal/common/eal_common_cpuflags.c',
                 'lib/librte_eal/common/eal_common_dev.c',
                 'lib/librte_eal/common/eal_common_devargs.c',
                 'lib/librte_eal/common/eal_common_errno.c',
                 'lib/librte_eal/common/eal_common_hexdump.c',
                 'lib/librte_eal/common/eal_common_launch.c',
                 'lib/librte_eal/common/eal_common_lcore.c',
                 'lib/librte_eal/common/eal_common_log.c',
                 'lib/librte_eal/common/eal_common_memory.c',
                 'lib/librte_eal/common/eal_common_memzone.c',
                 'lib/librte_eal/common/eal_common_options.c',
                 'lib/librte_eal/common/eal_common_string_fns.c',
                 'lib/librte_eal/common/eal_common_tailqs.c',
                 'lib/librte_eal/common/eal_common_thread.c',
                 'lib/librte_eal/common/eal_common_timer.c',

                 'lib/librte_eal/common/malloc_elem.c',
                 'lib/librte_eal/common/malloc_heap.c',
                 'lib/librte_eal/common/rte_keepalive.c',
                 'lib/librte_eal/common/rte_malloc.c',
                 'lib/librte_eal/common/rte_service.c',
                 'lib/librte_eal/linuxapp/eal/eal.c',
                 'lib/librte_eal/linuxapp/eal/eal_alarm.c',
                 'lib/librte_eal/linuxapp/eal/eal_debug.c',
                 'lib/librte_eal/linuxapp/eal/eal_hugepage_info.c',
                 'lib/librte_eal/linuxapp/eal/eal_interrupts.c',
                 'lib/librte_eal/linuxapp/eal/eal_lcore.c',
                 'lib/librte_eal/linuxapp/eal/eal_log.c',
                 'lib/librte_eal/linuxapp/eal/eal_memory.c',
                 'lib/librte_eal/linuxapp/eal/eal_thread.c',
                 'lib/librte_eal/linuxapp/eal/eal_timer.c',
                 'lib/librte_eal/linuxapp/eal/eal_vfio_mp_sync.c',
                 'lib/librte_eal/linuxapp/eal/eal_vfio.c',
                 'lib/librte_ether/rte_ethdev.c',
                 'lib/librte_ether/rte_flow.c',
                 'lib/librte_ether/ethdev_profile.c',
                 'lib/librte_hash/rte_cuckoo_hash.c',
                 'lib/librte_kvargs/rte_kvargs.c',
                 'lib/librte_mbuf/rte_mbuf.c',
                 'lib/librte_mbuf/rte_mbuf_ptype.c',
                 'lib/librte_mempool/rte_mempool.c',
                 'lib/librte_mempool/rte_mempool_ops.c',
                 'lib/librte_net/rte_net.c',
                 'lib/librte_net/rte_net_crc.c',
                 'lib/librte_ring/rte_ring.c',
                 'lib/librte_pci/rte_pci.c',
            ]);

ntacc_dpdk_src = SrcGroup(dir='src/dpdk',
                src_list=[

                 'drivers/net/ntacc/filter_ntacc.c',
                 'drivers/net/ntacc/rte_eth_ntacc.c',
                 'drivers/net/ntacc/nt_compat.c',
            ]);

mlx5_dpdk_src = SrcGroup(dir='src/dpdk/',
                src_list=[

                 'drivers/net/mlx5/mlx5_mr.c',
                 'drivers/net/mlx5/mlx5_ethdev.c',
                 'drivers/net/mlx5/mlx5_mac.c',
                 'drivers/net/mlx5/mlx5_rxmode.c',
                 'drivers/net/mlx5/mlx5_rxtx.c',
                 'drivers/net/mlx5/mlx5_stats.c',
                 'drivers/net/mlx5/mlx5_txq.c',
                 'drivers/net/mlx5/mlx5.c',
                 #'drivers/net/mlx5/mlx5_fdir.c', # remove 
                 'drivers/net/mlx5/mlx5_flow.c',
                 'drivers/net/mlx5/mlx5_rss.c',
                 'drivers/net/mlx5/mlx5_rxq.c',
                 'drivers/net/mlx5/mlx5_trigger.c',
                 'drivers/net/mlx5/mlx5_vlan.c',

                 'drivers/net/mlx5/mlx5_rxtx_vec.c',
                 'drivers/net/mlx5/mlx5_socket.c',

            ]);

mlx4_dpdk_src = SrcGroup(dir='src/dpdk/',
                src_list=[
                 'drivers/net/mlx4/mlx4.c',
                 'drivers/net/mlx4/mlx4_ethdev.c',
                 'drivers/net/mlx4/mlx4_flow.c',
                 'drivers/net/mlx4/mlx4_intr.c',
                 'drivers/net/mlx4/mlx4_mr.c',
                 'drivers/net/mlx4/mlx4_rxq.c',
                 'drivers/net/mlx4/mlx4_rxtx.c',
                 'drivers/net/mlx4/mlx4_txq.c',
                 'drivers/net/mlx4/mlx4_utils.c',
            ]);

if march == 'x86_64':
    bp_dpdk = SrcGroups([
                  dpdk_src,
                  dpdk_src_x86_64
                  ]);
elif march == 'aarch64':
    bp_dpdk = SrcGroups([
                  dpdk_src,
                  dpdk_src_aarch64
                  ]);

ntacc_dpdk =SrcGroups([
                ntacc_dpdk_src
                ]);
mlx5_dpdk =SrcGroups([
                mlx5_dpdk_src
                ]);

mlx4_dpdk =SrcGroups([
                mlx4_dpdk_src
                ]);

bpf = SrcGroups([
                bpf_src,
                bpfjit_src]);

# this is the library dp going to falcon (and maybe other platforms)
bp =SrcGroups([
                main_src,
                cmn_src ,
                net_src ,
                yaml_src,
                json_src,
                rpc_server_src,
                
                stx_src,
                stf_src,
                stateless_src,
                astf_batch_src,
                astf_src,
                
                version_src
                ]);

l2fwd_main_src = SrcGroup(dir='src',
        src_list=[
             'l2fwd/main.c'
             ]);


l2fwd =SrcGroups([
                l2fwd_main_src]);


# common flags for both new and old configurations
common_flags = ['-DWIN_UCODE_SIM',
                '-D_BYTE_ORDER',
                '-D_LITTLE_ENDIAN',
                '-DLINUX',
                '-g',
                '-Wno-format',
                '-Wno-deprecated-declarations',
                '-DRTE_DPDK',
                '-D__STDC_LIMIT_MACROS',
                '-D__STDC_FORMAT_MACROS',
                '-D__STDC_CONSTANT_MACROS'
                #'-D_GLIBCXX_USE_CXX11_ABI=0', # see libstdc++ ABI changes for string and list
                #'-DTREX_PERF', # used when using TRex and PERF for performance measurement
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
                    '-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE3,RTE_CPUFLAG_SSE,RTE_CPUFLAG_SSE2,RTE_CPUFLAG_SSSE3,RTE_CPUFLAG_SSE4_1,RTE_CPUFLAG_SSE4_2,RTE_CPUFLAG_AES,RTE_CPUFLAG_PCLMULQDQ,RTE_CPUFLAG_AVX',
                   ]

    common_flags_old = common_flags + [
                      '-march=corei7',
                      '-DUCS_210',
                      '-mtune=generic',
                      '-DRTE_MACHINE_CPUFLAG_SSE',
                      '-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE',
                      ];

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


dpdk_includes_path_x86_64 ='''
                        ../src/dpdk/lib/librte_eal/common/include/arch/x86
                       '''

dpdk_includes_path_aarch64 ='''
                        ../src/dpdk/lib/librte_eal/common/include/arch/arm
                       '''

dpdk_includes_path =''' ../src/
                        ../src/pal/linux_dpdk/
                        ../src/pal/linux_dpdk/dpdk1711_'''+ march +'''/
                        ../src/dpdk/drivers/
                        ../src/dpdk/drivers/net/
                        ../src/dpdk/drivers/net/af_packet/
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
                         
                        ../src/dpdk/lib/
                        ../src/dpdk/lib/librte_cfgfile/
                        ../src/dpdk/lib/librte_compat/
                        ../src/dpdk/lib/librte_distributor/
                        ../src/dpdk/lib/librte_eal/
                        ../src/dpdk/lib/librte_eal/common/
                        ../src/dpdk/lib/librte_eal/common/include/
                        ../src/dpdk/lib/librte_eal/common/include/arch/

                        ../src/dpdk/lib/librte_eal/common/include/generic/
                        ../src/dpdk/lib/librte_eal/linuxapp/
                        ../src/dpdk/lib/librte_eal/linuxapp/eal/
                        ../src/dpdk/lib/librte_eal/linuxapp/eal/include/
                        ../src/dpdk/lib/librte_eal/linuxapp/eal/include/exec-env/
                        ../src/dpdk/lib/librte_eal/linuxapp/igb_uio/
                        ../src/dpdk/lib/librte_ether/
                        ../src/dpdk/lib/librte_hash/
                        ../src/dpdk/lib/librte_kvargs/
                        ../src/dpdk/lib/librte_mbuf/
                        ../src/dpdk/lib/librte_mempool/
                        ../src/dpdk/lib/librte_ring/
                        ../src/dpdk/lib/librte_pci/
                        ../src/dpdk/lib/librte_net/
                        ../src/dpdk/lib/librte_port/
                        ../src/dpdk/
                        
                        ../src/dpdk/drivers/bus/pci/
                        ../src/dpdk/drivers/bus/vdev/
                        ../src/dpdk/drivers/bus/pci/linux/
                    ''';

# Include arch specific folder before generic folders
if march == 'x86_64':
    dpdk_includes_path = dpdk_includes_path_x86_64 + dpdk_includes_path
elif march == 'aarch64':
    dpdk_includes_path = dpdk_includes_path_aarch64 + dpdk_includes_path


includes_path = '''
                   ../src/
                   ../src/pal/common/
                   ../src/pal/linux_dpdk/
                   ../src/stx/
                   ../src/stx/common/
                   ../external_libs/yaml-cpp/include/
                   ../external_libs/zmq-''' + march + '''/include/
                   ../external_libs/json/
                   ../external_libs/bpf/
                  ''';


dpdk_includes_verb_path =''


bpf_includes_path = '../external_libs/bpf ../external_libs/bpf/bpfjit'


if march != 'aarch64':
    DPDK_FLAGS=['-D_GNU_SOURCE', '-DPF_DRIVER', '-DX722_SUPPORT', '-DX722_A0_SUPPORT', '-DVF_DRIVER', '-DINTEGRATED_VF', '-include', '../src/pal/linux_dpdk/dpdk1711_x86_64/rte_config.h'];
else:
    DPDK_FLAGS=['-D_GNU_SOURCE', '-DPF_DRIVER', '-DVF_DRIVER', '-DINTEGRATED_VF', '-DRTE_FORCE_INTRINSICS', '-include', '../src/pal/linux_dpdk/dpdk1711_aarch64/rte_config.h'];

client_external_libs = [
        'simple_enum',
        'jsonrpclib-pelix-0.2.5',
        'pyyaml-3.11',
        'pyzmq-14.5.0',
        'scapy-2.3.1',
        'texttable-0.8.4',
        'simpy-3.0.10',
        'trex-openssl',
        ]

rpath_linkage = ['so']

RELEASE_    = "release"
DEBUG_      = "debug"
PLATFORM_x86 = "x86"
PLATFORM_x86_64 = "x86_64"
PLATFORM_aarch64 = "aarch64"


class build_option:

    def __init__(self,debug_mode,is_pie):
      self.mode     = debug_mode;   ##debug,release
      self.platform = march  # aarch64 or x86_64
      self.is_pie = is_pie
      
    def __str__(self):
       s=self.mode+","+self.platform;
       return (s);

    def lib_name(self,lib_name_p,full_path):
        if full_path:
            return b_path+lib_name_p;
        else:
            return lib_name_p;
    #private functions
    def toLib (self,name,full_path = True):
        lib_n = "lib"+name+".a";
        return (self.lib_name(lib_n,full_path));

    def toExe(self,name,full_path = True):
        return (self.lib_name(name,full_path));

    def isIntelPlatform (self):
        return ( self.platform == PLATFORM_x86 or self.platform == PLATFORM_x86_64)

    def isArmPlatform (self):
        return ( self.platform == PLATFORM_aarch64)

    def is64Platform (self):
        return ( self.platform == PLATFORM_x86_64 or self.platform == PLATFORM_aarch64)

    def isRelease (self):
        return ( self.mode  == RELEASE_);

    def isPIE (self):
        return self.is_pie

    def update_executable_name (self,name):
        return self.update_name(name,"-")

    def update_non_exe_name (self,name):
        return self.update_name(name,"_")

    def update_name (self,name,delimiter):
        trg = copy.copy(name);
        if self.is64Platform():
            trg += delimiter + "64";
        else:
            trg += delimiter + "32";

        if self.isRelease () :
            trg += "";
        else:
            trg +=  delimiter + "debug";

        if self.isPIE():
            trg += delimiter + "o"
        return trg;

    def get_target (self):
        return self.update_executable_name("_t-rex");

    def get_target_l2fwd (self):
        return self.update_executable_name("l2fwd");

    def get_dpdk_target (self):
        return self.update_executable_name("dpdk");

    def get_ntacc_target (self):
        return self.update_executable_name("ntacc");

    def get_mlx5_target (self):
        return self.update_executable_name("mlx5");

    def get_mlx4_target (self):
        return self.update_executable_name("mlx4");

    def get_ntaccso_target (self):
        return self.update_executable_name("libntacc")+'.so';

    def get_mlx5so_target (self):
        return self.update_executable_name("libmlx5")+'.so';

    def get_mlx4so_target (self):
        return self.update_executable_name("libmlx4")+'.so';

    def get_bpf_target (self):
        return self.update_executable_name("bpf");

    def get_bpfso_target (self):
        return self.update_executable_name("libbpf") + '.so';

     
    def get_mlx5_flags(self):
        flags=[]
        if self.isRelease () :
            flags += ['-DNDEBUG'];
        else:
            flags += ['-UNDEBUG'];
        return (flags)

    def get_mlx4_flags(self):
        flags=[]
        if self.isRelease () :
            flags += ['-DNDEBUG'];
        else:
            flags += ['-UNDEBUG'];
        return (flags)

    def get_common_flags (self):
        if self.isPIE():
            flags = copy.copy(common_flags_old)
        else:
            flags = copy.copy(common_flags_new);

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

        flags += ['-Wall',
                  '-Werror',
                  '-Wno-literal-suffix',
                  '-Wno-sign-compare',
                  '-Wno-strict-aliasing']
        if self.isIntelPlatform():
            flags += [
                      '-Wno-aligned-new'
                     ]

        if is_sanitized:
            flags += ['-fsanitize=address', '-fsanitize=leak', '-fno-omit-frame-pointer']
            
        return (flags)

        
    def get_c_flags (self, is_sanitized):
        flags = self.get_common_flags()
        if  self.isRelease () :
            flags += ['-DNDEBUG'];

        if is_sanitized:
            flags += ['-fsanitize=address', '-fsanitize=leak', '-fno-omit-frame-pointer']
            
        # for C no special flags yet
        return (flags)

        
    def get_link_flags(self, is_sanitized):
        base_flags = ['-rdynamic'];
        if self.is64Platform() and self.isIntelPlatform():
            base_flags += ['-m64']
        base_flags += ['-lrt'];

            
        if is_sanitized:
            base_flags += ['-fsanitize=address', '-fsanitize=leak']
        return base_flags;



build_types = [
               build_option(debug_mode= DEBUG_, is_pie = False),
               build_option(debug_mode= RELEASE_, is_pie = False),
               build_option(debug_mode= DEBUG_, is_pie = True),
               build_option(debug_mode= RELEASE_, is_pie = True),
              ]


def build_prog (bld, build_obj):

    #rte_libs =[
    #         'dpdk'];

    #rte_libs1 = rte_libs+rte_libs+rte_libs;

    #for obj in rte_libs:
    #    bld.read_shlib( name=obj , paths=[top+rte_lib_path] )

    # add electric fence only for debug image
    debug_file_list='';
    #if not build_obj.isRelease ():
    #    debug_file_list +=ef_src.file_list(top)

    cflags    = build_obj.get_c_flags(bld.env.SANITIZED)
    cxxflags  = build_obj.get_cxx_flags(bld.env.SANITIZED)
    linkflags = build_obj.get_link_flags(bld.env.SANITIZED)
    
    bld.objects(
      features='c ',
      includes = dpdk_includes_path,
      cflags   = (cflags + DPDK_FLAGS ),
      source   = bp_dpdk.file_list(top),
      target=build_obj.get_dpdk_target()
      );

    if bld.env.NO_MLX == False:
        bld.shlib(
          features='c',
          includes = dpdk_includes_path+dpdk_includes_verb_path,
          cflags   = (cflags + DPDK_FLAGS + build_obj.get_mlx5_flags() ),
            use =['ibverbs','mlx5'],
          source   = mlx5_dpdk.file_list(top),
          target   = build_obj.get_mlx5_target()
        )

        bld.shlib(
        features='c',
        includes = dpdk_includes_path+dpdk_includes_verb_path,
        cflags   = (cflags + DPDK_FLAGS + build_obj.get_mlx4_flags() ),
            use =['ibverbs', 'mlx4'],
        source   = mlx4_dpdk.file_list(top),
        target   = build_obj.get_mlx4_target()
       )

    if bld.env.WITH_NTACC == True:
        bld.shlib(
          features='c',
          includes = dpdk_includes_path+dpdk_includes_verb_path,
          cflags   = (cflags + DPDK_FLAGS +
            ['-I/opt/napatech3/include',
             '-DNAPATECH3_LIB_PATH=\"/opt/napatech3/lib\"']),
          use =['ntapi'],

          source   = ntacc_dpdk.file_list(top),
          target   = build_obj.get_ntacc_target()
        )

    # build the BPF as a shared library
    bld.shlib(features = 'c',
              includes = bpf_includes_path,
              cflags   = cflags + ['-DSLJIT_CONFIG_AUTO=1'],
              source   = bpf.file_list(top),
              target   = build_obj.get_bpf_target())


    inc_path = dpdk_includes_path + includes_path
    cxxflags_ext = ['',]

    bld.program(features='cxx cxxprogram',
                includes =inc_path,
                cxxflags = ( cxxflags + ['-std=gnu++11',]),
                linkflags = linkflags ,
                lib=['pthread','dl', 'z'],
                use =[build_obj.get_dpdk_target(), build_obj.get_bpf_target(), 'zmq'],
                source = bp.file_list(top) + debug_file_list,
                rpath = rpath_linkage,
                target = build_obj.get_target())



def build_type(bld,build_obj):
    build_prog(bld, build_obj);


def post_build(bld):

    print("*** generating softlinks ***")
    exec_p ="../scripts/"
    for obj in build_types:
        install_single_system(bld, exec_p, obj);


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

        
    global dpdk_includes_verb_path;
    bld.add_pre_fun(pre_build)
    bld.add_post_fun(post_build);

    # ZMQ
    zmq_lib_path='external_libs/zmq-' + march + '/'
    bld.read_shlib( name='zmq' , paths=[top + zmq_lib_path] )

    if bld.env.NO_MLX == False:
        if bld.env['LIB_IBVERBS']:
            Logs.pprint('GREEN', 'Info: Using external libverbs.')
            bld.read_shlib(name='ibverbs')
            bld.read_shlib(name='mlx5')
            bld.read_shlib(name='mlx4')
        else:
            Logs.pprint('GREEN', 'Info: Using internal libverbs.')
            ibverbs_lib_path='external_libs/ibverbs/'
            dpdk_includes_verb_path =' \n ../external_libs/ibverbs/include/ \n'
            bld.read_shlib( name='ibverbs' , paths=[top+ibverbs_lib_path] )
            bld.read_shlib( name='mlx5',paths=[top+ibverbs_lib_path])
            bld.read_shlib( name='mlx4',paths=[top+ibverbs_lib_path])
            check_ibverbs_deps(bld)

    for obj in build_types:
        build_type(bld,obj);


def build_info(bld):
    pass;


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

        if not os.path.lexists(full_link):
            rel_path = os.path.relpath(src, where)
            print('{0} --> {1}'.format(name, rel_path))

            os.symlink(rel_path, full_link)


def install_single_system (bld, exec_p, build_obj):

    o = 'build_dpdk/linux_dpdk/'

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
    do_create_link(src = os.path.realpath(o + build_obj.get_mlx4so_target()),
                   name = build_obj.get_mlx4so_target(),
                   where = so_path)

    # BPF
    do_create_link(src   = os.path.realpath(o + build_obj.get_bpfso_target()),
                   name  = build_obj.get_bpfso_target(),
                   where = so_path)



def pre_build(bld):
    if not bld.options.no_ver:
        print("update version files")
        create_version_files()


def write_file (file_name,s):
    f=open(file_name,'w')
    f.write(s)
    f.close()


def get_build_num ():
    s='';
    if os.path.isfile(BUILD_NUM_FILE):
        f=open(BUILD_NUM_FILE,'r');
        s+=f.readline().rstrip();
        f.close();
    return s;

def create_version_files ():
    git_sha="N/A"
    try:
      r=getstatusoutput("git log --pretty=format:'%H' -n 1")
      if r[0]==0:
          git_sha=r[1]
    except :
        pass;


    s =''
    s +="#ifndef __TREX_VER_FILE__           \n"
    s +="#define __TREX_VER_FILE__           \n"
    s +="#ifdef __cplusplus                  \n"
    s +=" extern \"C\" {                        \n"
    s +=" #endif                             \n";
    s +='#define  VERSION_USER  "%s"          \n' % os.environ.get('USER', 'unknown')
    s +='extern const char * get_build_date(void);  \n'
    s +='extern const char * get_build_time(void);  \n'
    s +='#define VERSION_UIID      "%s"       \n' % uuid.uuid1()
    s +='#define VERSION_GIT_SHA   "%s"       \n' % git_sha
    s +='#define VERSION_BUILD_NUM "%s"       \n' % get_build_num()
    s +="#ifdef __cplusplus                  \n"
    s +=" }                        \n"
    s +=" #endif                             \n";
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
    o='build_dpdk/linux_dpdk/';
    src_file =  os.path.realpath(o+build_obj.get_target())
    print(src_file)
    if os.path.exists(src_file):
        dest_file = exec_p +build_obj.get_target()
        print(dest_file)
        os.system("cp %s %s " %(src_file,dest_file));
        os.system("chmod +x %s " %(dest_file));

def _copy_single_system1 (bld, exec_p, build_obj):
    o='../scripts/';
    src_file =  os.path.realpath(o+build_obj.get_target()[1:])
    print(src_file)
    if os.path.exists(src_file):
        dest_file = exec_p +build_obj.get_target()[1:]
        os.system("cp %s %s " %(src_file,dest_file));
        os.system("chmod +x %s " %(dest_file));


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
            'scapy_daemon_server',
            'master_daemon.py',
            'trex-console',
            'daemon_server'
            ];

files_dir=['cap2','avl','cfg','ko','automation', 'external_libs', 'python-lib','stl','exp','astf']


class Env(object):
    @staticmethod
    def get_env(name) :
        s= os.environ.get(name);
        if s == None:
            print("You should define $ %s" % name)
            raise Exception("Env error");
        return (s);

    @staticmethod
    def get_release_path () :
        s= Env().get_env('TREX_LOCAL_PUBLISH_PATH');
        s +=get_build_num ()+"/"
        return  s;

    @staticmethod
    def get_remote_release_path () :
        s= Env().get_env('TREX_REMOTE_PUBLISH_PATH');
        return  s;

    @staticmethod
    def get_local_web_server () :
        s= Env().get_env('TREX_WEB_SERVER');
        return  s;

    # extral web
    @staticmethod
    def get_trex_ex_web_key() :
        s= Env().get_env('TREX_EX_WEB_KEY');
        return  s;

    @staticmethod
    def get_trex_ex_web_path() :
        s= Env().get_env('TREX_EX_WEB_PATH');
        return  s;

    @staticmethod
    def get_trex_ex_web_user() :
        s= Env().get_env('TREX_EX_WEB_USER');
        return  s;

    @staticmethod
    def get_trex_ex_web_srv() :
        s= Env().get_env('TREX_EX_WEB_SRV');
        return  s;

    @staticmethod
    def get_trex_regression_workspace():
        return Env().get_env('TREX_REGRESSION_WORKSPACE')


def check_release_permission():
    if os.getenv('USER') not in USERS_ALLOWED_TO_RELEASE:
        raise Exception('You are not allowed to release TRex. Please contact Hanoch.')

# build package in parent dir. can provide custom name and folder with --pkg-dir and --pkg-file
def pkg(self):
    build_num = get_build_num()
    pkg_dir = self.options.pkg_dir
    if not pkg_dir:
        pkg_dir = os.pardir
    pkg_file = self.options.pkg_file
    if not pkg_file:
        pkg_file = '%s.tar.gz' % build_num
    tmp_path = os.path.join(pkg_dir, '_%s' % pkg_file)
    dst_path = os.path.join(pkg_dir, pkg_file)
    build_path = os.path.join(os.pardir, build_num)

    # clean old dir if exists
    os.system('rm -rf %s' % build_path)
    release(self, build_path + '/')
    os.system("cp %s/%s.tar.gz %s" % (build_path, build_num, tmp_path))
    os.system("mv %s %s" % (tmp_path, dst_path))

    # clean new dir
    os.system('rm -rf %s' % build_path)


def release(bld, custom_dir = None):
    """ release to local folder  """
    if custom_dir:
        exec_p = custom_dir
    else:
        check_release_permission()
        exec_p = Env().get_release_path()
    print("copy images and libs")
    os.system(' mkdir -p '+exec_p);

    for obj in build_types:
        copy_single_system(bld,exec_p,obj)
        copy_single_system1(bld,exec_p,obj)

    for obj in files_list:
        src_file =  '../scripts/'+obj
        dest_file = exec_p +'/'+obj
        os.system("cp %s %s " %(src_file,dest_file));

    for obj in files_dir:
        src_file =  '../scripts/'+obj+'/'
        dest_file = exec_p +'/'+obj+'/'
        os.system("cp -rv %s %s " %(src_file,dest_file));
        os.system("chmod 755 %s " %(dest_file));

    # copy .SO objects and resolve symbols
    os.system('cp -rL ../scripts/so {0}'.format(exec_p))

    rel=get_build_num ()

    # create client package
    os.system('mkdir -p %s/trex_client/external_libs' % exec_p)
    for ext_lib in client_external_libs:
        os.system('cp ../scripts/external_libs/%s %s/trex_client/external_libs/ -r' % (ext_lib, exec_p))
    os.system('cp ../scripts/automation/trex_control_plane/stf %s/trex_client/ -r' % exec_p)
    os.system('cp ../scripts/automation/trex_control_plane/stl %s/trex_client/ -r' % exec_p)
    with open('%s/trex_client/stl/examples/stl_path.py' % exec_p) as f:
        stl_path_content = f.read()
    if 'STL_PROFILES_PATH' not in stl_path_content:
        raise Exception('Could not find STL_PROFILES_PATH in stl/examples/stl_path.py')
    stl_path_content = re.sub('STL_PROFILES_PATH.*?\n', "STL_PROFILES_PATH = os.path.join(os.pardir, 'profiles')\n", stl_path_content)
    with open('%s/trex_client/stl/examples/stl_path.py' % exec_p, 'w') as f:
        f.write(stl_path_content)
    os.system('cp ../scripts/stl %s/trex_client/stl/profiles -r' % exec_p)
    shutil.make_archive(os.path.join(exec_p, 'trex_client_%s' % rel), 'gztar', exec_p, 'trex_client')
    os.system('rm -r %s/trex_client' % exec_p)

    os.system('cd %s/..;tar --exclude="*.pyc" -zcvf %s/%s.tar.gz %s' %(exec_p,os.getcwd(),rel,rel))
    os.system("mv %s/%s.tar.gz %s" % (os.getcwd(),rel,exec_p));


def publish(bld, custom_source = None):
    check_release_permission()
    exec_p = Env().get_release_path()
    rel=get_build_num ()

    release_name ='%s.tar.gz' % (rel);
    if custom_source:
        from_ = custom_source
    else:
        from_ = exec_p+'/'+release_name;
    os.system("rsync -av %s %s:%s/%s " %(from_,Env().get_local_web_server(),Env().get_remote_release_path (), release_name))
    if not bld.options.private:
      os.system("ssh %s 'cd %s;rm be_latest; ln -P %s be_latest'  " %(Env().get_local_web_server(),Env().get_remote_release_path (),release_name))
      os.system("ssh %s 'cd %s;rm latest; ln -P %s latest'  " %(Env().get_local_web_server(),Env().get_remote_release_path (),release_name))


def publish_ext(bld, custom_source = None):
    check_release_permission()
    exec_p = Env().get_release_path()
    rel=get_build_num ()

    release_name ='%s.tar.gz' % (rel);
    if custom_source:
        from_ = custom_source
    else:
        from_ = exec_p+'/'+release_name;
    cmd='rsync -avz --progress -e "ssh -i %s" --rsync-path=/usr/bin/rsync %s %s@%s:%s/release/%s' % (Env().get_trex_ex_web_key(),from_, Env().get_trex_ex_web_user(),Env().get_trex_ex_web_srv(),Env().get_trex_ex_web_path() ,release_name)
    print(cmd)
    os.system( cmd )

    if not bld.options.private:
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
def show(bld):
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

def test (bld):
    r=getstatusoutput("git log --pretty=format:'%H' -n 1")
    if r[0]==0:
        print(r[1])













