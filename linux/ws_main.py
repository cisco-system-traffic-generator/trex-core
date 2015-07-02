#!/usr/bin/env python
# encoding: utf-8

# Hanoh Haim
# Cisco Systems, Inc.


VERSION='0.0.1'
APPNAME='cxx_test'
import os;
import commands;
import shutil;
import copy;

top = '../'
out = 'build'
b_path ="./build/linux/"


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

def configure(conf):
    conf.load('g++')


main_src = SrcGroup(dir='src',
        src_list=[
             'main.cpp',
             'bp_sim.cpp',
             'bp_gtest.cpp',
             'os_time.cpp',
             'rx_check.cpp',
             'tuple_gen.cpp',
             'platform_cfg.cpp',
             'utl_yaml.cpp',
             'rx_check_header.cpp',
             'nat_check.cpp',
             'timer_wheel_pq.cpp',
             'time_histogram.cpp',
             'utl_json.cpp',
             'utl_cpuu.cpp',
             'msg_manager.cpp',
             'gtest/tuple_gen_test.cpp',
             'gtest/nat_test.cpp',

             'pal/linux/pal_utl.cpp',
             'pal/linux/mbuf.cpp'
             ]);

cmn_src = SrcGroup(dir='src/common',
    src_list=[ 
        'gtest-all.cc',
        'gtest_main.cc',
        'basic_utils.cpp',
        'captureFile.cpp',
        'erf.cpp',
        'pcap.cpp'
        ]);

         
net_src = SrcGroup(dir='src/common/Network/Packet',
        src_list=[
           'CPktCmn.cpp',
           'EthernetHeader.cpp',
           'IPHeader.cpp',
           'TCPHeader.cpp',
           'TCPOptions.cpp',
           'UDPHeader.cpp',
           'MacAddress.cpp',
           'VLANHeader.cpp']);

yaml_src = SrcGroup(dir='yaml-cpp/src/',
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

bp =SrcGroups([
                main_src, 
                cmn_src ,
                net_src ,
                yaml_src
                ]);


cxxflags_base =['-DWIN_UCODE_SIM',
           '-D_BYTE_ORDER',
           '-D_LITTLE_ENDIAN',
           '-DLINUX',
           '-g',
       ];




includes_path =''' ../src/pal/linux/
                   ../src/
                   ../yaml-cpp/include/
              ''';





RELEASE_    = "release"
DEBUG_      = "debug"
PLATFORM_64 = "64"
PLATFORM_32 = "32"


class build_option:

    def __init__(self,platform,debug_mode,is_pie):
      self.mode     = debug_mode;   ##debug,release
      self.platform = platform; #['32','64'] 
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

    def is64Platform (self):
        return ( self.platform == PLATFORM_64);

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
        if self.is64Platform () :
            result+=['-m64'];
        else:
            result+=['-m32'];

        if self.isRelease () :
            result+=['-O2'];
        else:
            result+=['-O0','-DDEBUG','-D_DEBUG','-DSTILE_CPP_ASSERT','-DSTILE_SHIM_ASSERT'];

        if self.isPIE():
            result += ['-fPIE', '-DPATCH_FOR_PIE']

        return result;

    def get_target (self):
        return self.update_executable_name("bp-sim");

    def get_flags (self):
        return self.cxxcomp_flags(cxxflags_base);

    def get_link_flags(self):
        # add here basic flags
        base_flags = ['-lpthread'];
        if self.isPIE():
            base_flags.append('-lstdc++')

        #platform depended flags

        if self.is64Platform():
            base_flags += ['-m64'];
        else:
            base_flags += ['-lrt'];
        if self.isPIE():
            base_flags += ['-pie', '-DPATCH_FOR_PIE']

        return base_flags;

    def get_exe (self,full_path = True):
        return self.toExe(self.get_target(),full_path);


build_types = [
               build_option(debug_mode= DEBUG_, platform = PLATFORM_32, is_pie = False),
               build_option(debug_mode= DEBUG_, platform = PLATFORM_64, is_pie = False),
               build_option(debug_mode= RELEASE_,platform = PLATFORM_32, is_pie = False),
               build_option(debug_mode= RELEASE_,platform = PLATFORM_64, is_pie = False),
              ]



def build_prog (bld, build_obj):
    bld.program(features='cxx cxxprogram', 
                includes =includes_path,
                cxxflags =build_obj.get_flags(),
                stlib = 'stdc++',
                linkflags = build_obj.get_link_flags(),
                source = bp.file_list(top),
                target = build_obj.get_target())


def build_type(bld,build_obj):
    build_prog(bld, build_obj);


def post_build(bld):
    print "copy objects"
    exec_p ="../scripts/"
    for obj in build_types:
        install_single_system(bld, exec_p, obj);

def build(bld):
    bld.add_post_fun(post_build);
    for obj in build_types:
        build_type(bld,obj);


def build_info(bld):
    pass;
      
def install_single_system (bld, exec_p, build_obj):
    o='build/linux/';
    src_file =  os.path.realpath(o+build_obj.get_target())
    if os.path.exists(src_file):
        dest_file = exec_p +build_obj.get_target()
        if not os.path.lexists(dest_file):
            relative_path = os.path.relpath(src_file, exec_p)
            os.symlink(relative_path, dest_file);




