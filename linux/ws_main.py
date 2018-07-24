#!/usr/bin/env python
# encoding: utf-8

# Hanoh Haim
# Cisco Systems, Inc.


VERSION='0.0.1'
APPNAME='cxx_test'

import os
import shutil
import copy

from distutils.version import StrictVersion
from waflib import Logs

top = '../'
out = 'build'
b_path ="./build/linux/"
march = os.uname()[4]

REQUIRED_CC_VERSION = "4.7.0"
SANITIZE_CC_VERSION = "4.9.0"

GCC6_DIR = '/usr/local/gcc-6.2/bin'
GCC7_DIR = '/usr/local/gcc-7.2/bin'


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

    co = opt.option_groups['configure options']
    co.add_option('--sanitized', dest='sanitized', default=False, action='store_true',
                   help='for GCC {0}+ use address sanitizer to catch memory errors'.format(SANITIZE_CC_VERSION))

    co.add_option('--gcc6', dest='gcc6', default=False, action='store_true',
                   help='use GCC 6.2 instead of the machine version')

    co.add_option('--gcc7', dest='gcc7', default=False, action='store_true',
                   help='use GCC 7.2 instead of the machine version')



def verify_cc_version (env, min_ver = REQUIRED_CC_VERSION):
    ver = StrictVersion('.'.join(env['CC_VERSION']))

    return (ver >= min_ver, ver, min_ver)

    
def configure(conf):
    if conf.options.gcc6 and conf.options.gcc7:
        conf.fatal('--gcc6 and --gcc7 and mutual exclusive')

    # start from clean
    if 'RPATH' in os.environ:
        conf.env.RPATH = os.environ['RPATH'].split(':')
    else:
        conf.env.RPATH = []

    if conf.options.gcc6:
        configure_gcc(conf, GCC6_DIR)
    elif conf.options.gcc7:
        configure_gcc(conf, GCC7_DIR)
    else:
        configure_gcc(conf)

    
    # first verify CC version
    rc = verify_cc_version(conf.env, REQUIRED_CC_VERSION)
    if not rc[0]:
        print("\nMachine GCC version too low '{0}' - required at least '{1}'".format(rc[1], rc[2]))
        print( "\n*** please set a compiler using CXX / AR enviorment variables ***\n")
        exit(-1)
    
        
    # handle sanitized process if needed
    configure_sanitized(conf)
  
            

def configure_gcc (conf, explicit_path = None):
    # use the system path
    if explicit_path is None:
        conf.load('g++')
        return

    if not os.path.exists(explicit_path):
        conf.fatal('unable to find specific GCC installtion dir: {0}'.format(explicit_path))

    saved = conf.environ['PATH']
    try:
        conf.environ['PATH'] = explicit_path
        conf.load('g++')
    finally:
        conf.environ['PATH'] = saved




def configure_sanitized (conf):

    # first we turn off SANITIZED
    conf.env.SANITIZED = False
    
    # if sanitized is required - check GCC version for sanitizing
    conf.start_msg('Build sanitized images (GCC >= {0})'.format(SANITIZE_CC_VERSION))    
    
    # not required
    if not conf.options.sanitized:
        conf.end_msg('no', 'YELLOW')

    else:
        rc = verify_cc_version(conf.env, SANITIZE_CC_VERSION)
        if not rc[0]:
            conf.fatal('--sanitized is supported only with GCC {0}+ - current {1}'.format(rc[2], rc[1]))
        else:
            conf.end_msg('yes', 'GREEN')
            conf.env.SANITIZED = True

    
    
        
bp_sim_main = SrcGroup(dir='src',
        src_list=['main.cpp'])

bp_sim_gtest = SrcGroup(dir='src',
        src_list=[
             'bp_gtest.cpp',
             'gtest/bp_timer_gtest.cpp',
             'gtest/bp_tcp_gtest.cpp',
             'gtest/tuple_gen_test.cpp',
             'gtest/client_cfg_test.cpp',
             'gtest/nat_test.cpp',
             'gtest/trex_stateless_gtest.cpp'
             ])

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
            '44bsd/sim_cs_tcp.cpp',
            '44bsd/sch_rampup.cpp',
            '44bsd/udp.cpp',

            'astf/astf_template_db.cpp',
            'stt_cp.cpp',
            'bp_sim_tcp.cpp',
            'inet_pton.cpp',
            'trex_global.cpp',
            'bp_sim.cpp',
            'trex_platform.cpp',
            'bp_sim_stf.cpp',
            'os_time.cpp',
            'rx_check.cpp',
            'tuple_gen.cpp',
            'platform_cfg.cpp',
            'tw_cfg.cpp',
            'rx_check_header.cpp',
            'nat_check.cpp',
            'nat_check_flow_table.cpp',
            'pkt_gen.cpp',
            'timer_wheel_pq.cpp',
            'time_histogram.cpp',
            'msg_manager.cpp',
            'trex_port_attr.cpp',
            'publisher/trex_publisher.cpp',
            'stateful_rx_core.cpp',
            'flow_stat_parser.cpp',
            'trex_watchdog.cpp',
            'trex_client_config.cpp',
            'pal/linux/pal_utl.cpp',
            'pal/linux/mbuf.cpp',
            'pal/common/common_mbuf.cpp',
            'sim/trex_sim_stateless.cpp',
            'sim/trex_sim_stateful.cpp',
            'sim/trex_sim_astf.cpp',
            'h_timer.cpp',
            'astf/astf_db.cpp',
            'astf/astf_json_validator.cpp',
            'trex_build_info.cpp',

            'utils/utl_counter.cpp',
            'utils/utl_cpuu.cpp',
            'utils/utl_dbl_human.cpp',
            'utils/utl_ip.cpp',
            'utils/utl_mbuf.cpp',
            'utils/utl_policer.cpp',
            'utils/utl_port_map.cpp',
            'utils/utl_sync_barrier.cpp',
            'utils/utl_yaml.cpp',
            ]);

cmn_src = SrcGroup(dir='src/common',
    src_list=[ 
        'gtest-all.cc',
        'gtest_main.cc',
        'basic_utils.cpp',
        'captureFile.cpp',
        'erf.cpp',
        'pcap.cpp',
        'base64.cpp',
        'sim_event_driven.cpp',
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


# STX - common code
stx_src = SrcGroup(dir='src/stx/common',
                           src_list=['trex_stx.cpp',
                                     'trex_pkt.cpp',
                                     'trex_rx_packet_parser.cpp',
                                     'trex_capture.cpp',
                                     'trex_port.cpp',
                                     'trex_dp_port_events.cpp',
                                     'trex_dp_core.cpp',
                                     'trex_messaging.cpp',

                                     'trex_rx_core.cpp',
                                     'trex_rx_port_mngr.cpp',
                                     'trex_rx_tx.cpp',
                                     'trex_stack_base.cpp',
                                     'trex_stack_linux_based.cpp',
                                     'trex_stack_legacy.cpp',

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


# stubs
stubs = SrcGroup(dir='/src/stub/',
        src_list=['zmq_stub.c',
                  'bpf_stub.c'])


bp =SrcGroups([
                bp_sim_main,
                bp_sim_gtest,
                main_src, 
                cmn_src ,
                stubs,
                net_src ,
                yaml_src,
                json_src,
                
                stx_src,
                stf_src,
                stateless_src,
                astf_batch_src,
                astf_src,
                
                rpc_server_src
                ]);


cxxflags_base =['-DWIN_UCODE_SIM',
                '-DTREX_SIM',
                '-D_BYTE_ORDER',
                '-D_LITTLE_ENDIAN',
                '-DLINUX',
                '-D__STDC_LIMIT_MACROS',
                '-D__STDC_FORMAT_MACROS',
                #'-DGLIBCXX_USE_CXX11_ABI=0',
                '-g',
                '-Wno-deprecated-declarations',
                '-std=c++0x',
       ];




includes_path =''' ../src/pal/linux/
                   ../src/pal/common/
                   ../src/
                   ../src/utils/
                   
                   ../src/rpc-server/
                   
                   ../src/stx/
                   ../src/stx/common/
                   ../src/stx/common/rx/
                   
                   ../external_libs/json/
                   ../external_libs/zmq-'''+ march +'''/include/
                   ../external_libs/yaml-cpp/include/
                   ../external_libs/bpf/
                   ../external_libs/valijson/include/
              ''';





RELEASE_    = "release"
DEBUG_      = "debug"
PLATFORM_64 = "64"
PLATFORM_32 = "32"
PLATFORM_x86 = "x86"
PLATFORM_x86_64 = "x86_64"
PLATFORM_aarch64 = "aarch64"


class build_option:

    def __init__(self, name, src, debug_mode, is_pie, use = [], flags = [], rpath = []):
      self.mode     = debug_mode;   ##debug,release
      self.platform = march  # aarch64 or x86_64
      self.is_pie = is_pie
      self.name = name
      self.src = src
      self.use = use
      self.flags = flags
      self.rpath = rpath

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
            trg += delimiter + "pie"
        return trg;

    def cxxcomp_flags (self,flags):
        result = copy.copy(flags);

        if self.isIntelPlatform():
            if self.is64Platform () :
                result+=['-m64'];
            else:
                result+=['-m32'];

        if self.isRelease () :
            result+=['-O3'];
        else:
            result+=['-O0','-DDEBUG','-D_DEBUG','-DSTILE_CPP_ASSERT','-DSTILE_SHIM_ASSERT'];

        if self.isPIE():
            result += ['-fPIE', '-DPATCH_FOR_PIE']

        result += self.flags

        return result;

    def get_use_libs (self):
        return self.use

    def get_target (self):
        return self.update_executable_name(self.name);

    def get_flags (self, is_sanitized = False):
        flags = self.cxxcomp_flags(cxxflags_base);
        if is_sanitized:
            flags += ['-fsanitize=address', '-fsanitize=leak', '-fno-omit-frame-pointer']
        
        return flags
        

    def get_src (self):
        return self.src.file_list(top)

    def get_rpath (self):
        return self.rpath

    def get_link_flags(self, is_sanitized):
        
        # add here basic flags
        base_flags = [];
        if self.isPIE():
            base_flags.append('-lstdc++')

        #platform depended flags

        if self.isIntelPlatform():
            if self.is64Platform():
                base_flags += ['-m64']
            else:
                base_flags += ['-m32']
                base_flags += ['-lrt']

        if self.isPIE():
            base_flags += ['-pie', '-DPATCH_FOR_PIE']

        if is_sanitized:
            base_flags += ['-fsanitize=address', '-fsanitize=leak']
            
        return base_flags;


build_types = [
               build_option(name = "bp-sim", src = bp, use = [''],debug_mode= DEBUG_, is_pie = False,
                            flags = ['-Wall', '-Werror', '-Wno-sign-compare', '-Wno-strict-aliasing'],
                            rpath = ['so']),

               build_option(name = "bp-sim", src = bp, use = [''],debug_mode= RELEASE_, is_pie = False,
                            flags = ['-Wall', '-Werror', '-Wno-sign-compare', '-Wno-strict-aliasing'],
                            rpath = ['so']),

              ]


def build_prog (bld, build_obj):
    
    # determine if sanitized image should be built
    is_sanitized = bld.env.SANITIZED
    
    cxxflags  = build_obj.get_flags(is_sanitized)+['-std=gnu++11',]
    linkflags = build_obj.get_link_flags(is_sanitized)
    
        
    bld.program(features='cxx cxxprogram', 
                includes =  includes_path,
                cxxflags =  cxxflags,
                linkflags = linkflags,
                source = build_obj.get_src(),
                use = build_obj.get_use_libs(),
                lib = ['pthread', 'z', 'dl'],
                rpath  = bld.env.RPATH + build_obj.get_rpath(),
                target = build_obj.get_target())


def build_type(bld,build_obj):
    build_prog(bld, build_obj);


def post_build(bld):
    print("updating softlinks")

    exec_p ="../scripts/"

    for obj in build_types:
        install_single_system(bld, exec_p, obj);

def build(bld):
    if bld.env.SANITIZED and bld.cmd == 'build':
        Logs.warn("\n******* building sanitized binaries *******\n")
        
    bld.add_post_fun(post_build);
    for obj in build_types:
        build_type(bld,obj);

    

def build_info(bld):
    pass;

def compare_link(link_path, dst_path):
    return os.path.abspath(os.readlink(link_path)) == os.path.abspath(dst_path)

def install_single_system (bld, exec_p, build_obj):
    o=bld.out_dir+'/linux/';
    src_file =  os.path.realpath(o+build_obj.get_target())
    if os.path.exists(src_file):
        dest_file = exec_p +build_obj.get_target()
        relative_path = os.path.relpath(src_file, exec_p)
        if os.path.islink(dest_file):
            if compare_link(dest_file, relative_path):
                return
            os.unlink(dest_file)
        if not os.path.lexists(dest_file):
            print('{0} --> {1}'.format(build_obj.get_target(), relative_path))
            os.symlink(relative_path, dest_file);




