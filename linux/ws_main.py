#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006-2012 (ita)
#/******                         
# * NAME
# *   stile_main.cpp
# *   
# * AUTHOR
# *   Thomas Nagy, 2006-2012 (ita)
# *   
# * COPYRIGHT
# *   Copyright (c) 2006-2012 by cisco Systems, Inc.
# *   All rights reserved.
# *   
# * DESCRIPTION
# *   
# ****/
# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='cxx_test'
import os;
import commands;
import shutil;
import copy;


# these variables are mandatory ('/' are converted automatically)
top = '../'
out = 'build'
b_path ="./build/linux/"

#######################################
# utility for group source code 
###################################

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


#######################################





#I put v3 as v4 contain temp fix for falcon

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

         

# CFT core files
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

# this is the library dp going to falcon (and maybe other platforms)
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

    # handles shim options
    def get_target_shim (self):
        trg = "mcpshim";
        return self.update_non_exe_name(trg);

    def get_flags_shim (self):
        flags = ['-DWIN_UCODE_SIM', '-DLINUX' ,'-g'];
        return self.cxxcomp_flags(flags);


    def get_lib_shim(self,full_path = True):
        return self.toLib(self.get_target_shim(),full_path);

    def shim_print (self):
        print "\tshim: target name is "+self.get_target_shim();
        print "\tshim: compile flags are "+str(self.get_flags_shim());
        print "\tshim: lib full pathname is " +self.get_lib_shim();
        print "\n";

    # handle falcon stub options
    def get_target_falcon_stub (self):
        trg = "falcon_stub";
        return self.update_non_exe_name(trg);

    def get_flags_falcon_stub (self):
        return self.cxxcomp_flags(cxxflags_base);

    def get_lib_falcon_stub (self):
        return self.toLib(self.get_target_falcon_stub());

    def falcon_stub_print (self):
        
        print "\tfalcon_stub: target name is "+self.get_target_falcon_stub();
        print "\tfalcon_stub: compile flags are "+str(self.get_flags_falcon_stub());
        print "\tfalcon_stub: lib full pathname is " +self.get_lib_falcon_stub();
        print "\n";

    # handle stile-dp options
    def get_target_stile_dp (self):
        trg = "stile_dp";
        return self.update_non_exe_name(trg);

    def get_flags_stile_dp (self):
        return self.cxxcomp_flags(cxxflags_base);

    def get_lib_stile_dp(self, full_path = True):
        return self.toLib(self.get_target_stile_dp(),full_path);

    def stile_dp_print (self):
       
        print "\tstile_dp: target name is "+self.get_target_stile_dp();
        print "\tstile_dp: compile flags are "+str(self.get_flags_stile_dp());
        print "\tstile_dp: lib full pathname is " +self.get_lib_stile_dp();
        print "\n";

    # handle stile-away main program options
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

    def stileaway_print (self):
       
        print "\ttarget name is "+self.get_target();
        print "\tcompile flags are "+str(self.get_flags());
        print "\tlink flags are " + str(self.get_link_flags());
        print "\texe full pathname is " +self.get_exe();
        print "\n";

    # handle pi options
    def get_target_pi(self):
        if self.is64Platform():
            return "stile64";
        else:
            return "stile32";
        
    def get_lib_pi_path(self):
        if self.isPIE():
            return cntrl_lib64bit_pie_path

        if self.is64Platform():
            return cntrl_lib64bit_path;
        else:
            return cntrl_lib32bit_path;

    def get_lib_pi(self,full_path = True):
        lib_name_p = "lib"+self.get_target_pi()+".a"
        if full_path:
            return self.get_lib_pi_path() + lib_name_p;
        else:
            return lib_name_p;

    def stile_pi_print(self):

        print "\tstile_cp: target name is " + self.get_target_pi();
        print "\tstile_cp: lib full pathname is " +self.get_lib_pi();
        print "\n";


    def get_target_stile_cp (self):
         if self.is64Platform():
            cp = "stile_cp_64";
         else:
            cp = "stile_cp_32";
	 
	 if self.isPIE():
	    cp += "_pie"
	 return cp

    # handle release options

    def get_flags_release(self):
        return self.cxxcomp_flags(cxxflags_base);

    def get_release_link_flags(self,include_lib=True):
        # add here basic flags
        base_flags = ['-lpthread'];
        if self.isPIE():
            base_flags += ['-lstdc++']

        if include_lib:
            #those are the linked libraries
            base_flags += ["-L"+release_lib,
                           "-l"+self.get_target_stile_dp(),
                           "-l"+self.get_target_stile_cp(),
                           "-l"+self.get_target_shim(),
                           "-l"+self.get_target_falcon_stub(),
                           ];

        #platform depended flags
        if self.is64Platform():
            base_flags += ['-m64'];
        else:
            base_flags += ['-lrt'];
        return base_flags;

    def get_target_release (self):
        return self.update_non_exe_name("stile-away-pkg");
    def get_target_falcon_test(self):
        return self.update_non_exe_name("stile-away-falcon-stand-alone");

    def release_print(self):

        print "\trelease: target name is " + self.get_target_release();
        print "\trelease: link flags are " + str(self.get_release_link_flags());
        print "\n";
    cp_output_lib   = ['libstile_cp_32.a', 'libstile_cp_64.a', 'libstile_cp_32.a', 'libstile_cp_64.a', 'libstile_cp_64_pie.a'];
    
    def calculate_index (self):
        index = 0;
        if self.isRelease():
           index += 2;
        if self.is64Platform():
            index += 1;
	if self.isPIE():
	    index += 1;
        return index;
    def get_install_cp(self):
        return self.cp_output_lib[self.calculate_index()];


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
    for obj in build_types:
        print str(obj);
        obj.stileaway_print();


      
def install_single_system (bld, exec_p, build_obj):
    o='build/linux/';
    src_file =  os.path.realpath(o+build_obj.get_target())
    if os.path.exists(src_file):
        dest_file = exec_p +build_obj.get_target()
        if not os.path.lexists(dest_file):
            relative_path = os.path.relpath(src_file, exec_p)
            os.symlink(relative_path, dest_file);

#
# need to fix this
def install1(bld):
    print "copy images and libs"
    exec_p ="./"
 
    for obj in build_types:
        install_single_system(bld,exec_p,obj);

def release(bld):
    print "copy images and libs"
    exec_p ="/auto/proj-pcube-b/apps/PL-b/tools//bp_sim/v1.0/"
    os.system(' mkdir -p '+exec_p);
    
    for obj in build_types:
        install_single_system(bld,exec_p,obj);
    



