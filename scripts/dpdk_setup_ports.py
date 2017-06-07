#! /usr/bin/python
# hhaim 
import sys
import os
python_ver = 'python%s' % sys.version_info[0]
yaml_path = os.path.join('external_libs', 'pyyaml-3.11', python_ver)
if yaml_path not in sys.path:
    sys.path.append(yaml_path)
import yaml
import dpdk_nic_bind
import re
import argparse
import copy
import shlex
import traceback
from collections import defaultdict, OrderedDict
from distutils.util import strtobool
import subprocess
import platform

# exit code is Important should be
# -1 : don't continue 
# 0  : no errors - no need to load mlx share object
# 32  : no errors - mlx share object should be loaded 
MLX_EXIT_CODE = 32

class VFIOBindErr(Exception): pass

PATH_ARR = os.getenv('PATH', '').split(':')
for path in ['/usr/local/sbin', '/usr/sbin', '/sbin']:
    if path not in PATH_ARR:
        PATH_ARR.append(path)
os.environ['PATH'] = ':'.join(PATH_ARR)

def if_list_remove_sub_if(if_list):
    return map(lambda x:x.split('/')[0],if_list)

class ConfigCreator(object):
    mandatory_interface_fields = ['Slot_str', 'Device_str', 'NUMA']
    _2hex_re = '[\da-fA-F]{2}'
    mac_re = re.compile('^({0}:){{5}}{0}$'.format(_2hex_re))
    MAC_LCORE_NUM = 63 # current bitmask is 64 bit

    # cpu_topology - dict: physical processor -> physical core -> logical processing unit (thread)
    # interfaces - array of dicts per interface, should include "mandatory_interface_fields" values
    def __init__(self, cpu_topology, interfaces, include_lcores = [], exclude_lcores = [], only_first_thread = False, zmq_rpc_port = None, zmq_pub_port = None, prefix = None, ignore_numa = False):
        self.cpu_topology = copy.deepcopy(cpu_topology)
        self.interfaces   = copy.deepcopy(interfaces)
        del cpu_topology
        del interfaces
        assert isinstance(self.cpu_topology, dict), 'Type of cpu_topology should be dict, got: %s' % type(self.cpu_topology)
        assert len(self.cpu_topology.keys()) > 0, 'cpu_topology should contain at least one processor'
        assert isinstance(self.interfaces, list), 'Type of interfaces should be list, got: %s' % type(list)
        assert len(self.interfaces) % 2 == 0, 'Should be even number of interfaces, got: %s' % len(self.interfaces)
        assert len(self.interfaces) >= 2, 'Should be at least two interfaces, got: %s' % len(self.interfaces)
        assert isinstance(include_lcores, list), 'include_lcores should be list, got: %s' % type(include_lcores)
        assert isinstance(exclude_lcores, list), 'exclude_lcores should be list, got: %s' % type(exclude_lcores)
        assert len(self.interfaces) >= 2, 'Should be at least two interfaces, got: %s' % len(self.interfaces)
        if only_first_thread:
            for cores in self.cpu_topology.values():
                for core in cores.keys():
                    cores[core] = cores[core][:1]
        include_lcores = [int(x) for x in include_lcores]
        exclude_lcores = [int(x) for x in exclude_lcores]

        self.has_zero_lcore = False
        self.lcores_per_numa = {}
        total_lcores = 0
        for numa, cores in self.cpu_topology.items():
            self.lcores_per_numa[numa] = {'main': [], 'siblings': [], 'all': []}
            for core, lcores in cores.items():
                total_lcores += len(lcores)
                for lcore in list(lcores):
                    if include_lcores and lcore not in include_lcores:
                        cores[core].remove(lcore)
                    if exclude_lcores and lcore in exclude_lcores:
                        cores[core].remove(lcore)
                    if lcore > self.MAC_LCORE_NUM:
                        cores[core].remove(lcore)
                if 0 in lcores:
                    self.has_zero_lcore = True
                    lcores.remove(0)
                    self.lcores_per_numa[numa]['siblings'].extend(lcores)
                else:
                    self.lcores_per_numa[numa]['main'].extend(lcores[:1])
                    self.lcores_per_numa[numa]['siblings'].extend(lcores[1:])
                self.lcores_per_numa[numa]['all'].extend(lcores)

        for interface in self.interfaces:
            for mandatory_interface_field in ConfigCreator.mandatory_interface_fields:
                if mandatory_interface_field not in interface:
                    raise DpdkSetup("Expected '%s' field in interface dictionary, got: %s" % (mandatory_interface_field, interface))

        Device_str = self._verify_devices_same_type(self.interfaces)
        if '40Gb' in Device_str:
            self.speed = 40
        else:
            self.speed = 10

        minimum_required_lcores = len(self.interfaces) // 2 + 2
        if total_lcores < minimum_required_lcores:
            raise DpdkSetup('Your system should have at least %s cores for %s interfaces, and it has: %s.' %
                    (minimum_required_lcores, len(self.interfaces), total_lcores))
        interfaces_per_numa = defaultdict(int)

        for i in range(0, len(self.interfaces), 2):
            numa = self.interfaces[i]['NUMA']
            if numa != self.interfaces[i+1]['NUMA'] and not ignore_numa:
                raise DpdkSetup('NUMA of each pair of interfaces should be the same. Got NUMA %s for client interface %s, NUMA %s for server interface %s' %
                        (numa, self.interfaces[i]['Slot_str'], self.interfaces[i+1]['NUMA'], self.interfaces[i+1]['Slot_str']))
            interfaces_per_numa[numa] += 2

        self.interfaces_per_numa = interfaces_per_numa
        self.prefix              = prefix
        self.zmq_pub_port        = zmq_pub_port
        self.zmq_rpc_port        = zmq_rpc_port
        self.ignore_numa         = ignore_numa

    @staticmethod
    def verify_mac(mac_string):
        if not ConfigCreator.mac_re.match(mac_string):
            raise DpdkSetup('MAC address should be in format of 12:34:56:78:9a:bc, got: %s' % mac_string)
        return mac_string.lower()

    @staticmethod
    def _exit_if_bad_ip(ip):
        if not ConfigCreator._verify_ip(ip):
            raise DpdkSetup("Got bad IP %s" % ip)

    @staticmethod
    def _verify_ip(ip):
        a = ip.split('.')
        if len(a) != 4:
            return False
        for x in a:
            if not x.isdigit():
                return False
            i = int(x)
            if i < 0 or i > 255:
                return False
        return True

    @staticmethod
    def _verify_devices_same_type(interfaces_list):
        Device_str = interfaces_list[0]['Device_str']
        for interface in interfaces_list:
            if Device_str != interface['Device_str']:
                raise DpdkSetup('Interfaces should be of same type, got:\n\t* %s\n\t* %s' % (Device_str, interface['Device_str']))
        return Device_str

    def create_config(self, filename = None, print_config = False):
        config_str = '### Config file generated by dpdk_setup_ports.py ###\n\n'
        config_str += '- port_limit: %s\n' % len(self.interfaces)
        config_str += '  version: 2\n'
        config_str += "  interfaces: ['%s']\n" % "', '".join([interface['Slot_str'] for interface in self.interfaces])
        if self.speed > 10:
            config_str += '  port_bandwidth_gb: %s\n' % self.speed
        if self.prefix:
            config_str += '  prefix: %s\n' % self.prefix
        if self.zmq_pub_port:
            config_str += '  zmq_pub_port: %s\n' % self.zmq_pub_port
        if self.zmq_rpc_port:
            config_str += '  zmq_rpc_port: %s\n' % self.zmq_rpc_port
        config_str += '  port_info:\n'
        for index, interface in enumerate(self.interfaces):
            if 'ip' in interface:
                self._exit_if_bad_ip(interface['ip'])
                self._exit_if_bad_ip(interface['def_gw'])
                config_str += ' '*6 + '- ip: %s\n' % interface['ip']
                config_str += ' '*8 + 'default_gw: %s\n' % interface['def_gw']
            else:
                config_str += ' '*6 + '- dest_mac: %s' % self.verify_mac(interface['dest_mac'])
                if interface.get('loopback_dest'):
                    config_str += " # MAC OF LOOPBACK TO IT'S DUAL INTERFACE\n"
                else:
                    config_str += '\n'
                config_str += ' '*8 + 'src_mac:  %s\n' % self.verify_mac(interface['src_mac'])
            if index % 2:
                config_str += '\n' # dual if barrier

        if not self.ignore_numa:
            config_str += '  platform:\n'
            if len(self.interfaces_per_numa.keys()) == 1 and -1 in self.interfaces_per_numa: # VM, use any cores
                lcores_pool = sorted([lcore for lcores in self.lcores_per_numa.values() for lcore in lcores['all']])
                config_str += ' '*6 + 'master_thread_id: %s\n' % (0 if self.has_zero_lcore else lcores_pool.pop(0))
                config_str += ' '*6 + 'latency_thread_id: %s\n' % lcores_pool.pop(0)
                lcores_per_dual_if = int(len(lcores_pool) * 2 / len(self.interfaces))
                config_str += ' '*6 + 'dual_if:\n'
                for i in range(0, len(self.interfaces), 2):
                    lcores_for_this_dual_if = list(map(str, sorted(lcores_pool[:lcores_per_dual_if])))
                    lcores_pool = lcores_pool[lcores_per_dual_if:]
                    if not lcores_for_this_dual_if:
                        raise DpdkSetup('lcores_for_this_dual_if is empty (internal bug, please report with details of setup)')
                    config_str += ' '*8 + '- socket: 0\n'
                    config_str += ' '*10 + 'threads: [%s]\n\n' % ','.join(lcores_for_this_dual_if)
            else:
                # we will take common minimum among all NUMAs, to satisfy all
                lcores_per_dual_if = 99
                extra_lcores = 1 if self.has_zero_lcore else 2
                # worst case 3 iterations, to ensure master and "rx" have cores left
                while (lcores_per_dual_if * sum(self.interfaces_per_numa.values()) / 2) + extra_lcores > sum([len(lcores['all']) for lcores in self.lcores_per_numa.values()]):
                    lcores_per_dual_if -= 1
                    for numa, lcores_dict in self.lcores_per_numa.items():
                        if not self.interfaces_per_numa[numa]:
                            continue
                        lcores_per_dual_if = min(lcores_per_dual_if, int(2 * len(lcores_dict['all']) / self.interfaces_per_numa[numa]))
                lcores_pool = copy.deepcopy(self.lcores_per_numa)
                # first, allocate lcores for dual_if section
                dual_if_section = ' '*6 + 'dual_if:\n'
                for i in range(0, len(self.interfaces), 2):
                    numa = self.interfaces[i]['NUMA']
                    dual_if_section += ' '*8 + '- socket: %s\n' % numa
                    lcores_for_this_dual_if  = lcores_pool[numa]['all'][:lcores_per_dual_if]
                    lcores_pool[numa]['all'] = lcores_pool[numa]['all'][lcores_per_dual_if:]
                    for lcore in lcores_for_this_dual_if:
                        if lcore in lcores_pool[numa]['main']:
                            lcores_pool[numa]['main'].remove(lcore)
                        elif lcore in lcores_pool[numa]['siblings']:
                            lcores_pool[numa]['siblings'].remove(lcore)
                        else:
                            raise DpdkSetup('lcore not in main nor in siblings list (internal bug, please report with details of setup)')
                    if not lcores_for_this_dual_if:
                        raise DpdkSetup('Not enough cores at NUMA %s. This NUMA has %s processing units and %s interfaces.' % (numa, len(self.lcores_per_numa[numa]), self.interfaces_per_numa[numa]))
                    dual_if_section += ' '*10 + 'threads: [%s]\n\n' % ','.join(list(map(str, sorted(lcores_for_this_dual_if))))

                # take the cores left to master and rx
                mains_left = [lcore for lcores in lcores_pool.values() for lcore in lcores['main']]
                siblings_left = [lcore for lcores in lcores_pool.values() for lcore in lcores['siblings']]
                if mains_left:
                    rx_core = mains_left.pop(0)
                else:
                    rx_core = siblings_left.pop(0)
                if self.has_zero_lcore:
                    master_core = 0
                elif mains_left:
                    master_core = mains_left.pop(0)
                else:
                    master_core = siblings_left.pop(0)
                config_str += ' '*6 + 'master_thread_id: %s\n' % master_core
                config_str += ' '*6 + 'latency_thread_id: %s\n' % rx_core
                # add the dual_if section
                config_str += dual_if_section

        # verify config is correct YAML format
        try:
            yaml.safe_load(config_str)
        except Exception as e:
            raise DpdkSetup('Could not create correct yaml config.\nGenerated YAML:\n%s\nEncountered error:\n%s' % (config_str, e))

        if print_config:
            print(config_str)
        if filename:
            if os.path.exists(filename):
                if not dpdk_nic_bind.confirm('File %s already exist, overwrite? (y/N)' % filename):
                    print('Skipping.')
                    return config_str
            with open(filename, 'w') as f:
                f.write(config_str)
            print('Saved to %s.' % filename)
        return config_str

# only load igb_uio if it's available
def load_igb_uio():
    loaded_mods = dpdk_nic_bind.get_loaded_modules()
    if 'igb_uio' in loaded_mods:
        return True
    if 'uio' not in loaded_mods:
        ret = os.system('modprobe uio')
        if ret:
            return False
    km = './ko/%s/igb_uio.ko' % dpdk_nic_bind.kernel_ver
    if os.path.exists(km):
        return os.system('insmod %s' % km) == 0

# try to compile igb_uio if it's missing
def compile_and_load_igb_uio():
    loaded_mods = dpdk_nic_bind.get_loaded_modules()
    if 'igb_uio' in loaded_mods:
        return
    if 'uio' not in loaded_mods:
        ret = os.system('modprobe uio')
        if ret:
            print('Failed inserting uio module, please check if it is installed')
            sys.exit(-1)
    km = './ko/%s/igb_uio.ko' % dpdk_nic_bind.kernel_ver
    if not os.path.exists(km):
        print("ERROR: We don't have precompiled igb_uio.ko module for your kernel version")
        print('Will try compiling automatically - make sure you have file-system read/write permission')
        try:
            subprocess.check_output('make', cwd = './ko/src', stderr = subprocess.STDOUT)
            subprocess.check_output(['make', 'install'], cwd = './ko/src', stderr = subprocess.STDOUT)
            print('\nSuccess.')
        except Exception as e:
            print('\n ERROR:  Automatic compilation failed: (%s)' % e)
            print('Make sure you have file-system read/write permission')
            print('You can try compiling yourself, using the following commands:')
            print('  $cd ko/src')
            print('  $make')
            print('  $make install')
            print('  $cd -')
            print('Then, try to run TRex again.')
            print('Note: you might need additional Linux packages for that:')
            print('  * yum based (Fedora, CentOS, RedHat):')
            print('        sudo yum install kernel-devel-`uname -r`')
            print('        sudo yum group install "Development tools"')
            print('  * apt based (Ubuntu):')
            print('        sudo apt install linux-headers-`uname -r`')
            print('        sudo apt install build-essential')
            sys.exit(-1)
    ret = os.system('insmod %s' % km)
    if ret:
        print('Failed inserting igb_uio module')
        sys.exit(-1)

class map_driver(object):
    args=None;
    cfg_file='/etc/trex_cfg.yaml'
    parent_args = None

class DpdkSetup(Exception):
    pass

class CIfMap:

    def __init__(self, cfg_file):
        self.m_cfg_file =cfg_file;
        self.m_cfg_dict={};
        self.m_devices={};
        self.m_is_mellanox_mode=False;

    def dump_error (self,err):
        s="""%s  
From this TRex version a configuration file must exist in /etc/ folder "
The name of the configuration file should be /etc/trex_cfg.yaml "
The minimum configuration file should include something like this
- version       : 2 # version 2 of the configuration file 
  interfaces    : ["03:00.0","03:00.1","13:00.1","13:00.0"]  # list of the interfaces to bind run ./dpdk_nic_bind.py --status to see the list 
  port_limit      : 2 # number of ports to use valid is 2,4,6,8,10,12 

example of already bind devices 

$ ./dpdk_nic_bind.py --status

Network devices using DPDK-compatible driver
============================================
0000:03:00.0 '82599ES 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=
0000:03:00.1 '82599ES 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=
0000:13:00.0 '82599ES 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=
0000:13:00.1 '82599ES 10-Gigabit SFI/SFP+ Network Connection' drv=igb_uio unused=

Network devices using kernel driver
===================================
0000:02:00.0 '82545EM Gigabit Ethernet Controller (Copper)' if=eth2 drv=e1000 unused=igb_uio *Active*

Other network devices
=====================


          """ % (err);
        return s;


    def raise_error  (self,err):
        s= self.dump_error (err)
        raise DpdkSetup(s)

    def set_only_mellanox_nics(self):
        self.m_is_mellanox_mode=True;

    def get_only_mellanox_nics(self):
        return self.m_is_mellanox_mode


    def read_pci (self,pci_id,reg_id):
        out=subprocess.check_output(['setpci', '-s',pci_id, '%s.w' %(reg_id)])
        out=out.decode(errors='replace');
        return (out.strip());

    def write_pci (self,pci_id,reg_id,val):
        out=subprocess.check_output(['setpci','-s',pci_id, '%s.w=%s' %(reg_id,val)])
        out=out.decode(errors='replace');
        return (out.strip());

    def tune_mlx5_device (self,pci_id):
        # set PCIe Read to 1024 and not 512 ... need to add it to startup s
        val=self.read_pci (pci_id,68)
        if val[0]!='3':
            val='3'+val[1:]
            self.write_pci (pci_id,68,val)
            assert(self.read_pci (pci_id,68)==val);

    def get_mtu_mlx5 (self,dev_id):
        if len(dev_id)>0:
            out=subprocess.check_output(['ifconfig', dev_id])
            out=out.decode(errors='replace');
            obj=re.search(r'MTU:(\d+)',out,flags=re.MULTILINE|re.DOTALL);
            if obj:
                return int(obj.group(1));
            else:
                obj=re.search(r'mtu (\d+)',out,flags=re.MULTILINE|re.DOTALL);
                if obj:
                    return int(obj.group(1));
                else:
                    return -1

    def set_mtu_mlx5 (self,dev_id,new_mtu):
        if len(dev_id)>0:
            out=subprocess.check_output(['ifconfig', dev_id,'mtu',str(new_mtu)])
            out=out.decode(errors='replace');


    def set_max_mtu_mlx5_device(self,dev_id):
        mtu=9*1024+22
        dev_mtu=self.get_mtu_mlx5 (dev_id);
        if (dev_mtu>0) and (dev_mtu!=mtu):
            self.set_mtu_mlx5(dev_id,mtu);
            if self.get_mtu_mlx5(dev_id) != mtu: 
                print("Could not set MTU to %d" % mtu)
                sys.exit(-1);


    def disable_flow_control_mlx5_device (self,dev_id):

           if len(dev_id)>0:
               my_stderr = open("/dev/null","wb")
               cmd ='ethtool -A '+dev_id + ' rx off tx off '
               subprocess.call(cmd, stdout=my_stderr,stderr=my_stderr, shell=True)
               my_stderr.close();

    def check_ofed_version (self):
        ofed_info='/usr/bin/ofed_info'

        ofed_ver_re = re.compile('.*[-](\d)[.](\d)[-].*')

        ofed_ver= 40
        ofed_ver_show= '4.0'


        if not os.path.isfile(ofed_info):
            print("OFED %s is not installed on this setup" % ofed_info)
            sys.exit(-1);

        try:
          out = subprocess.check_output([ofed_info])
        except Exception as e:
            print("OFED %s can't run " % (ofed_info))
            sys.exit(-1);

        lines=out.splitlines();

        if len(lines)>1:
            m= ofed_ver_re.match(str(lines[0]))
            if m:
                ver=int(m.group(1))*10+int(m.group(2))
                if ver < ofed_ver:
                  print("installed OFED version is '%s' should be at least '%s' and up" % (lines[0],ofed_ver_show))
                  sys.exit(-1);
            else:
                print("not found valid  OFED version '%s' " % (lines[0]))
                sys.exit(-1);


    def verify_ofed_os(self):
        err_msg = 'Warning: Mellanox NICs where tested only with RedHat/CentOS 7.2/7.3\n'
        err_msg += 'Correct usage with other Linux distributions is not guaranteed.'
        try:
            dist = platform.dist()
            if dist[0] not in ('redhat', 'centos') or not dist[1].startswith('7.2'):
                print(err_msg)
        except Exception as e:
            print('Error while determining OS type: %s' % e)

    def load_config_file (self):

        fcfg=self.m_cfg_file

        if not os.path.isfile(fcfg) :
            self.raise_error ("There is no valid configuration file %s\n" % fcfg)

        try:
          stream = open(fcfg, 'r')
          self.m_cfg_dict= yaml.safe_load(stream)
        except Exception as e:
          print(e);
          raise e

        stream.close();
        cfg_dict = self.m_cfg_dict[0]
        if 'version' not in cfg_dict:
            raise DpdkSetup("Configuration file %s is old, it should include version field\n" % fcfg )

        if int(cfg_dict['version'])<2 :
            raise DpdkSetup("Configuration file %s is old, expected version 2, got: %s\n" % (fcfg, cfg_dict['version']))

        if 'interfaces' not in self.m_cfg_dict[0]:
            raise DpdkSetup("Configuration file %s is old, it should include interfaces field with even number of elements" % fcfg)

        if_list= if_list_remove_sub_if(self.m_cfg_dict[0]['interfaces']);
        l=len(if_list);
        if l > 16:
            raise DpdkSetup("Configuration file %s should include interfaces field with maximum 16 elements, got: %s." % (fcfg,l))
        if l % 2:
            raise DpdkSetup("Configuration file %s should include even number of interfaces " % (fcfg,l))
        if 'port_limit' in cfg_dict and cfg_dict['port_limit'] > len(if_list):
            raise DpdkSetup('Error: port_limit should not be higher than number of interfaces in config file: %s\n' % fcfg)


    def do_bind_all(self, drv, pci, force = False):
        assert type(pci) is list
        cmd = '{ptn} dpdk_nic_bind.py --bind={drv} {pci} {frc}'.format(
            ptn = sys.executable,
            drv = drv,
            pci = ' '.join(pci),
            frc = '--force' if force else '')
        print(cmd)
        return os.system(cmd)

    # pros: no need to compile .ko per Kernel version
    # cons: need special config/hw (not always works)
    def try_bind_to_vfio_pci(self, to_bind_list):
        krnl_params_file = '/proc/cmdline'
        if not os.path.exists(krnl_params_file):
            raise VFIOBindErr('Could not find file with Kernel boot parameters: %s' % krnl_params_file)
        with open(krnl_params_file) as f:
            krnl_params = f.read()
        if 'iommu=' not in krnl_params:
            raise VFIOBindErr('vfio-pci is not an option here')
        if 'vfio_pci' not in dpdk_nic_bind.get_loaded_modules():
            ret = os.system('modprobe vfio_pci')
            if ret:
                raise VFIOBindErr('Could not load vfio_pci')
        ret = self.do_bind_all('vfio-pci', to_bind_list)
        if ret:
            raise VFIOBindErr('Binding to vfio_pci failed')


    def pci_name_to_full_name (self,pci_name):
          c='[0-9A-Fa-f]';
          sp='[:]'
          s_short=c+c+sp+c+c+'[.]'+c;
          s_full=c+c+c+c+sp+s_short
          re_full = re.compile(s_full)
          re_short = re.compile(s_short)

          if re_short.match(pci_name):
              return '0000:'+pci_name

          if re_full.match(pci_name):
              return pci_name

          err=" %s is not a valid pci address \n" %pci_name;
          raise DpdkSetup(err)


    def run_dpdk_lspci (self):
        dpdk_nic_bind.get_nic_details()
        self.m_devices= dpdk_nic_bind.devices

    def do_run (self,only_check_all_mlx=False):
        """ return the number of mellanox drivers"""

        self.run_dpdk_lspci ()
        self.load_config_file()
        if (map_driver.parent_args is None or
                map_driver.parent_args.dump_interfaces is None or
                    (map_driver.parent_args.dump_interfaces == [] and
                            map_driver.parent_args.cfg)):
            if_list=if_list_remove_sub_if(self.m_cfg_dict[0]['interfaces'])
        else:
            if_list = map_driver.parent_args.dump_interfaces
            if not if_list:
                for dev in self.m_devices.values():
                    if dev.get('Driver_str') in dpdk_nic_bind.dpdk_drivers + dpdk_nic_bind.dpdk_and_kernel:
                        if_list.append(dev['Slot'])

        if_list = list(map(self.pci_name_to_full_name, if_list))


        # check how many mellanox cards we have
        Mellanox_cnt=0;
        for key in if_list:
            if key not in self.m_devices:
                err=" %s does not exist " %key;
                raise DpdkSetup(err)

            if 'Vendor_str' not in self.m_devices[key]:
                err=" %s does not have Vendor_str " %key;
                raise DpdkSetup(err)

            if 'Mellanox' in self.m_devices[key]['Vendor_str']:
                Mellanox_cnt += 1


        if not (map_driver.parent_args and map_driver.parent_args.dump_interfaces):
            if (Mellanox_cnt > 0) and (Mellanox_cnt != len(if_list)):
               err=" All driver should be from one vendor. you have at least one driver from Mellanox but not all "; 
               raise DpdkSetup(err)
            if Mellanox_cnt > 0:
                self.set_only_mellanox_nics()

        if self.get_only_mellanox_nics():
            if not map_driver.parent_args.no_ofed_check:
                self.verify_ofed_os()
                self.check_ofed_version()

            for key in if_list:
                if 'Virtual' not in self.m_devices[key]['Device_str']:
                    pci_id = self.m_devices[key]['Slot_str']
                    self.tune_mlx5_device(pci_id)
                if 'Interface' in self.m_devices[key]:
                    dev_id=self.m_devices[key]['Interface']
                    self.disable_flow_control_mlx5_device (dev_id)
                    self.set_max_mtu_mlx5_device(dev_id)


        if only_check_all_mlx:
            if Mellanox_cnt > 0:
                sys.exit(MLX_EXIT_CODE);
            else:
                sys.exit(0);

        if if_list and map_driver.args.parent and self.m_cfg_dict[0].get('enable_zmq_pub', True):
            publisher_port = self.m_cfg_dict[0].get('zmq_pub_port', 4500)
            pid = dpdk_nic_bind.get_tcp_port_usage(publisher_port)
            if pid:
                cmdline = dpdk_nic_bind.read_pid_cmdline(pid)
                print('ZMQ port is used by following process:\npid: %s, cmd: %s' % (pid, cmdline))
                if not dpdk_nic_bind.confirm('Ignore and proceed (y/N):'):
                    sys.exit(-1)

        if map_driver.parent_args and map_driver.parent_args.stl and not map_driver.parent_args.no_scapy_server:
            try:
                master_core = self.m_cfg_dict[0]['platform']['master_thread_id']
            except:
                master_core = 0
            ret = os.system('%s scapy_daemon_server restart -c %s' % (sys.executable, master_core))
            if ret:
                print("Could not start scapy_daemon_server, which is needed by GUI to create packets.\nIf you don't need it, use --no-scapy-server flag.")
                sys.exit(-1)


        to_bind_list = []
        for key in if_list:
            if key not in self.m_devices:
                err=" %s does not exist " %key;
                raise DpdkSetup(err)

            if self.m_devices[key].get('Driver_str') not in (dpdk_nic_bind.dpdk_drivers + dpdk_nic_bind.dpdk_and_kernel):
                to_bind_list.append(key)

        if to_bind_list:
            if Mellanox_cnt:
                ret = self.do_bind_all('mlx5_core', to_bind_list)
                if ret:
                    raise DpdkSetup('Unable to bind interfaces to driver mlx5_core.')
                return MLX_EXIT_CODE
            else:
                # if igb_uio is ready, use it as safer choice, afterwards try vfio-pci
                if load_igb_uio():
                    print('Trying to bind to igb_uio ...')
                    ret = self.do_bind_all('igb_uio', to_bind_list)
                    if ret:
                        raise DpdkSetup('Unable to bind interfaces to driver igb_uio.') # module present, loaded, but unable to bind
                    return

                try:
                    print('Trying to bind to vfio-pci ...')
                    self.try_bind_to_vfio_pci(to_bind_list)
                    return
                except VFIOBindErr as e:
                    pass
                    #print(e)

                print('Trying to compile and bind to igb_uio ...')
                compile_and_load_igb_uio()
                ret = self.do_bind_all('igb_uio', to_bind_list)
                if ret:
                    raise DpdkSetup('Unable to bind interfaces to driver igb_uio.')
        elif Mellanox_cnt:
            return MLX_EXIT_CODE

    def do_return_to_linux(self):
        if not self.m_devices:
            self.run_dpdk_lspci()
        dpdk_interfaces = []
        check_drivers = set()
        for device in self.m_devices.values():
            if device.get('Driver_str') in dpdk_nic_bind.dpdk_drivers:
                dpdk_interfaces.append(device['Slot'])
                check_drivers.add(device['Driver_str'])
        if not dpdk_interfaces:
            print('No DPDK bound interfaces.')
            return
        any_driver_used = False
        for driver in check_drivers:
            if dpdk_nic_bind.is_module_used(driver):
                any_driver_used = True
        if any_driver_used:
            pid = dpdk_nic_bind.get_pid_using_pci(dpdk_interfaces)
            if pid:
                cmdline = dpdk_nic_bind.read_pid_cmdline(pid)
                print('DPDK interfaces are in use. Unbinding them might cause following process to hang:\npid: %s, cmd: %s' % (pid, cmdline))
                if not dpdk_nic_bind.confirm('Confirm (y/N):'):
                    sys.exit(-1)

        # DPDK => Linux
        drivers_table = {
            'net_ixgbe': 'ixgbe',
            'net_ixgbe_vf': 'ixgbevf',
            'net_e1000_igb': 'igb',
            'net_i40e': 'i40e',
            'net_i40e_vf': 'i40evf',
            'net_e1000_em': 'e1000',
            'net_vmxnet3': 'vmxnet3',
            'net_virtio': 'virtio-pci',
            'net_enic': 'enic',
        }
        nics_info = dpdk_nic_bind.get_info_from_trex(dpdk_interfaces)
        if not nics_info:
            raise DpdkSetup('Could not determine interfaces information. Try to run manually: sudo ./t-rex-64 --dump-interfaces')
        for pci, info in nics_info.items():
            if pci not in self.m_devices:
                raise DpdkSetup('Internal error: PCI %s is not found among devices' % pci)
            dev = self.m_devices[pci]
            if info['TRex_Driver'] not in drivers_table:
                raise DpdkSetup("Got unknown driver '%s', description: %s" % (info['TRex_Driver'], dev['Device_str']))
            linux_driver = drivers_table[info['TRex_Driver']]
            if linux_driver not in dpdk_nic_bind.get_loaded_modules():
                print("No Linux driver installed, or wrong module name: %s" % linux_driver)
            else:
                print('Returning to Linux %s' % pci)
                dpdk_nic_bind.bind_one(pci, linux_driver, False)

    def _get_cpu_topology(self):
        cpu_topology_file = '/proc/cpuinfo'
        # physical processor -> physical core -> logical processing units (threads)
        cpu_topology = OrderedDict()
        if not os.path.exists(cpu_topology_file):
            raise DpdkSetup('File with CPU topology (%s) does not exist.' % cpu_topology_file)
        with open(cpu_topology_file) as f:
            for lcore in f.read().split('\n\n'):
                if not lcore:
                    continue
                lcore_dict = OrderedDict()
                for line in lcore.split('\n'):
                    key, val = line.split(':', 1)
                    lcore_dict[key.strip()] = val.strip()
                if 'processor' not in lcore_dict:
                    continue
                numa = int(lcore_dict.get('physical id', -1))
                if numa not in cpu_topology:
                    cpu_topology[numa] = OrderedDict()
                core = int(lcore_dict.get('core id', lcore_dict['processor']))
                if core not in cpu_topology[numa]:
                    cpu_topology[numa][core] = []
                cpu_topology[numa][core].append(int(lcore_dict['processor']))
        if not cpu_topology:
            raise DpdkSetup('Could not determine CPU topology from %s' % cpu_topology_file)
        return cpu_topology

    # input: list of different descriptions of interfaces: index, pci, name etc.
    # Binds to dpdk wanted interfaces, not bound to any driver.
    # output: list of maps of devices in dpdk_* format (self.m_devices.values())
    def _get_wanted_interfaces(self, input_interfaces, get_macs = True):
        if type(input_interfaces) is not list:
            raise DpdkSetup('type of input interfaces should be list')
        if not len(input_interfaces):
            raise DpdkSetup('Please specify interfaces to use in the config')
        if len(input_interfaces) % 2:
            raise DpdkSetup('Please specify even number of interfaces')
        wanted_interfaces = []
        sorted_pci = sorted(self.m_devices.keys())
        for interface in input_interfaces:
            dev = None
            try:
                interface = int(interface)
                if interface < 0 or interface >= len(sorted_pci):
                    raise DpdkSetup('Index of an interfaces should be in range 0:%s' % (len(sorted_pci) - 1))
                dev = self.m_devices[sorted_pci[interface]]
            except ValueError:
                for d in self.m_devices.values():
                    if interface in (d['Interface'], d['Slot'], d['Slot_str']):
                        dev = d
                        break
            if not dev:
                raise DpdkSetup('Could not find information about this interface: %s' % interface)
            if dev in wanted_interfaces:
                raise DpdkSetup('Interface %s is specified twice' % interface)
            dev['Interface_argv'] = interface
            wanted_interfaces.append(dev)

        if get_macs:
            unbound = []
            dpdk_bound = []
            for interface in wanted_interfaces:
                if 'Driver_str' not in interface:
                    unbound.append(interface['Slot'])
                elif interface.get('Driver_str') in dpdk_nic_bind.dpdk_drivers:
                    dpdk_bound.append(interface['Slot'])
            if unbound or dpdk_bound:
                for pci, info in dpdk_nic_bind.get_info_from_trex(unbound + dpdk_bound).items():
                    if pci not in self.m_devices:
                        raise DpdkSetup('Internal error: PCI %s is not found among devices' % pci)
                    self.m_devices[pci].update(info)

        return wanted_interfaces

    def do_create(self):

        ips = map_driver.args.ips
        def_gws = map_driver.args.def_gws
        dest_macs = map_driver.args.dest_macs
        if map_driver.args.force_macs:
            ip_config = False
            if ips:
                raise DpdkSetup("If using --force-macs, should not specify ips")
            if def_gws:
                raise DpdkSetup("If using --force-macs, should not specify default gateways")
        elif ips:
            ip_config = True
            if not def_gws:
                raise DpdkSetup("If specifying ips, must specify also def-gws")
            if dest_macs:
                raise DpdkSetup("If specifying ips, should not specify dest--macs")
            if len(ips) != len(def_gws) or len(ips) != len(map_driver.args.create_interfaces):
                raise DpdkSetup("Number of given IPs should equal number of given def-gws and number of interfaces")
        else:
            if dest_macs:
                ip_config = False
            else:
                ip_config = True

        # gather info about NICS from dpdk_nic_bind.py
        if not self.m_devices:
            self.run_dpdk_lspci()
        wanted_interfaces = self._get_wanted_interfaces(map_driver.args.create_interfaces, get_macs = not ip_config)

        for i, interface in enumerate(wanted_interfaces):
            dual_index = i + 1 - (i % 2) * 2
            if ip_config:
                if isinstance(ips, list) and len(ips) > i:
                    interface['ip'] = ips[i]
                else:
                    interface['ip'] = '.'.join([str(i+1) for _ in range(4)])
                if isinstance(def_gws, list) and len(def_gws) > i:
                    interface['def_gw'] = def_gws[i]
                else:
                    interface['def_gw'] = '.'.join([str(dual_index+1) for _ in range(4)])
            else:
                dual_if = wanted_interfaces[dual_index]
                if 'MAC' not in interface:
                    raise DpdkSetup('Could not determine MAC of interface: %s. Please verify with -t flag.' % interface['Interface_argv'])
                if 'MAC' not in dual_if:
                    raise DpdkSetup('Could not determine MAC of interface: %s. Please verify with -t flag.' % dual_if['Interface_argv'])
                interface['src_mac'] = interface['MAC']
                if isinstance(dest_macs, list) and len(dest_macs) > i:
                    interface['dest_mac'] = dest_macs[i]
                else:
                    interface['dest_mac'] = dual_if['MAC']
                    interface['loopback_dest'] = True

        config = ConfigCreator(self._get_cpu_topology(), wanted_interfaces, include_lcores = map_driver.args.create_include, exclude_lcores = map_driver.args.create_exclude,
                               only_first_thread = map_driver.args.no_ht, ignore_numa = map_driver.args.ignore_numa,
                               prefix = map_driver.args.prefix, zmq_rpc_port = map_driver.args.zmq_rpc_port, zmq_pub_port = map_driver.args.zmq_pub_port)
        if map_driver.args.output_config:
            config.create_config(filename = map_driver.args.output_config)
        else:
            print('### Dumping config to screen, use -o flag to save to file')
            config.create_config(print_config = True)

    def do_interactive_create(self):
        ignore_numa = False
        cpu_topology = self._get_cpu_topology()
        total_lcores = sum([len(lcores) for cores in cpu_topology.values() for lcores in cores.values()])
        if total_lcores < 1:
            raise DpdkSetup('Script could not determine number of cores of the system, exiting.')
        elif total_lcores < 2:
            if dpdk_nic_bind.confirm("You only have 1 core and can't run TRex at all. Ignore and continue? (y/N): "):
                ignore_numa = True
            else:
                sys.exit(1)
        elif total_lcores < 3:
            if dpdk_nic_bind.confirm("You only have 2 cores and will be able to run only stateful without latency checks.\nIgnore and continue? (y/N): "):
                ignore_numa = True
            else:
                sys.exit(1)

        if map_driver.args.force_macs:
            ip_based = False
        elif dpdk_nic_bind.confirm("By default, IP based configuration file will be created. Do you want to use MAC based config? (y/N)"):
            ip_based = False
        else:
            ip_based = True
            ip_addr_digit = 1

        if not self.m_devices:
            self.run_dpdk_lspci()
        dpdk_nic_bind.show_table(get_macs = not ip_based)
        print('Please choose even number of interfaces from the list above, either by ID , PCI or Linux IF')
        print('Stateful will use order of interfaces: Client1 Server1 Client2 Server2 etc. for flows.')
        print('Stateless can be in any order.')
        numa = None
        for dev in self.m_devices.values():
            if numa is None:
                numa = dev['NUMA']
            elif numa != dev['NUMA']:
                print('For performance, try to choose each pair of interfaces to be on the same NUMA.')
                break
        while True:
            try:
                input = dpdk_nic_bind.read_line('Enter list of interfaces separated by space (for example: 1 3) : ')
                create_interfaces = input.replace(',', ' ').replace(';', ' ').split()
                wanted_interfaces = self._get_wanted_interfaces(create_interfaces)
                ConfigCreator._verify_devices_same_type(wanted_interfaces)
            except Exception as e:
                print(e)
                continue
            break
        print('')

        for interface in wanted_interfaces:
            if interface['Active']:
                print('Interface %s is active. Using it by TRex might close ssh connections etc.' % interface['Interface_argv'])
                if not dpdk_nic_bind.confirm('Ignore and continue? (y/N): '):
                    sys.exit(-1)

        for i, interface in enumerate(wanted_interfaces):
            if not ip_based:
                if 'MAC' not in interface:
                    raise DpdkSetup('Could not determine MAC of interface: %s. Please verify with -t flag.' % interface['Interface_argv'])
                interface['src_mac'] = interface['MAC']
            dual_index = i + 1 - (i % 2) * 2
            dual_int = wanted_interfaces[dual_index]
            if not ignore_numa and interface['NUMA'] != dual_int['NUMA']:
                print('NUMA is different at pair of interfaces: %s and %s. It will reduce performance.' % (interface['Interface_argv'], dual_int['Interface_argv']))
                if dpdk_nic_bind.confirm('Ignore and continue? (y/N): '):
                    ignore_numa = True
                    print('')
                else:
                    return

            if ip_based:
                if ip_addr_digit % 2 == 0:
                    dual_ip_digit = ip_addr_digit - 1
                else:
                    dual_ip_digit = ip_addr_digit + 1
                ip = '.'.join([str(ip_addr_digit) for _ in range(4)])
                def_gw= '.'.join([str(dual_ip_digit) for _ in range(4)])
                ip_addr_digit += 1

                print("For interface %s, assuming loopback to it's dual interface %s." % (interface['Interface_argv'], dual_int['Interface_argv']))
                if dpdk_nic_bind.confirm("Putting IP %s, default gw %s Change it?(y/N)." % (ip, def_gw)):
                    while True:
                        ip = dpdk_nic_bind.read_line('Please enter IP address for interface %s: ' % interface['Interface_argv'])
                        if not ConfigCreator._verify_ip(ip):
                            print ("Bad IP address format")
                        else:
                            break
                    while True:
                        def_gw = dpdk_nic_bind.read_line('Please enter default gateway for interface %s: ' % interface['Interface_argv'])
                        if not ConfigCreator._verify_ip(def_gw):
                            print ("Bad IP address format")
                        else:
                            break
                wanted_interfaces[i]['ip'] = ip
                wanted_interfaces[i]['def_gw'] = def_gw
            else:
                dest_mac = dual_int['MAC']
                loopback_dest = True
                print("For interface %s, assuming loopback to it's dual interface %s." % (interface['Interface_argv'], dual_int['Interface_argv']))
                if dpdk_nic_bind.confirm("Destination MAC is %s. Change it to MAC of DUT? (y/N)." % dest_mac):
                    while True:
                        input_mac = dpdk_nic_bind.read_line('Please enter new destination MAC of interface %s: ' % interface['Interface_argv'])
                        try:
                            if input_mac:
                                ConfigCreator.verify_mac(input_mac) # verify format
                                dest_mac = input_mac
                                loopback_dest = False
                            else:
                                print('Leaving the loopback MAC.')
                        except Exception as e:
                            print(e)
                            continue
                        break
                wanted_interfaces[i]['dest_mac'] = dest_mac
                wanted_interfaces[i]['loopback_dest'] = loopback_dest

        config = ConfigCreator(cpu_topology, wanted_interfaces, include_lcores = map_driver.args.create_include, exclude_lcores = map_driver.args.create_exclude,
                               only_first_thread = map_driver.args.no_ht, ignore_numa = map_driver.args.ignore_numa or ignore_numa,
                               prefix = map_driver.args.prefix, zmq_rpc_port = map_driver.args.zmq_rpc_port, zmq_pub_port = map_driver.args.zmq_pub_port)
        if dpdk_nic_bind.confirm('Print preview of generated config? (Y/n)', default = True):
            config.create_config(print_config = True)
        if dpdk_nic_bind.confirm('Save the config to file? (Y/n)', default = True):
            print('Default filename is /etc/trex_cfg.yaml')
            filename = dpdk_nic_bind.read_line('Press ENTER to confirm or enter new file: ')
            if not filename:
                filename = '/etc/trex_cfg.yaml'
            config.create_config(filename = filename)


def parse_parent_cfg (parent_cfg):
    parent_parser = argparse.ArgumentParser(add_help = False)
    parent_parser.add_argument('-?', '-h', '--help', dest = 'help', action = 'store_true')
    parent_parser.add_argument('--cfg', default='')
    parent_parser.add_argument('--dump-interfaces', nargs='*', default=None)
    parent_parser.add_argument('--no-ofed-check', action = 'store_true')
    parent_parser.add_argument('--no-scapy-server', action = 'store_true')
    parent_parser.add_argument('--no-watchdog', action = 'store_true')
    parent_parser.add_argument('-i', action = 'store_true', dest = 'stl', default = False)
    map_driver.parent_args, _ = parent_parser.parse_known_args(shlex.split(parent_cfg))
    if map_driver.parent_args.help:
        sys.exit(0)


def process_options ():
    parser = argparse.ArgumentParser(usage=""" 

Examples:
---------

To return to Linux the DPDK bound interfaces (for ifconfig etc.)
  sudo ./dpdk_set_ports.py -l

To create TRex config file using interactive mode
  sudo ./dpdk_set_ports.py -i

To create a default config file (example)
  sudo ./dpdk_setup_ports.py -c 02:00.0 02:00.1 -o /etc/trex_cfg.yaml

To show interfaces status
  sudo ./dpdk_set_ports.py -s

To see more detailed info on interfaces (table):
  sudo ./dpdk_set_ports.py -t

    """,
    description=" unbind dpdk interfaces ",
    epilog=" written by hhaim");

    parser.add_argument("-l", "--linux", action='store_true',
                      help=""" Return all DPDK interfaces to Linux driver """,
     )

    parser.add_argument("--cfg",
                      help=""" configuration file name  """,
     )

    parser.add_argument("--parent",  
                      help=argparse.SUPPRESS
     )

    parser.add_argument('--dump-pci-description', help=argparse.SUPPRESS, dest='dump_pci_desc', action='store_true')

    parser.add_argument("-i", "--interactive", action='store_true',
                      help=""" Create TRex config in interactive mode """,
     )

    parser.add_argument("-c", "--create", nargs='*', default=None, dest='create_interfaces', metavar='<interface>',
                      help="""Try to create a configuration file by specifying needed interfaces by PCI address or Linux names: eth1 etc.""",
     )

    parser.add_argument("--ci", "--cores-include", nargs='*', default=[], dest='create_include', metavar='<cores>',
                      help="""White list of cores to use. Make sure there is enough for each NUMA.""",
     )

    parser.add_argument("--ce", "--cores-exclude", nargs='*', default=[], dest='create_exclude', metavar='<cores>',
                      help="""Black list of cores to exclude. Make sure there will be enough for each NUMA.""",
     )

    parser.add_argument("--no-ht", default=False, dest='no_ht', action='store_true',
                      help="""Use only one thread of each Core in created config yaml (No Hyper-Threading).""",
     )

    parser.add_argument("--dest-macs", nargs='*', default=[], action='store',
                      help="""Destination MACs to be used in created yaml file. Without them, will assume loopback (0<->1, 2<->3 etc.)""",
     )

    parser.add_argument("--force-macs", default=False, action='store_true',
                      help="""Use MACs in created config file.""",
     )

    parser.add_argument("--ips", nargs='*', default=[], action='store',
                      help="""IP addresses to be used in created yaml file. Without them, will assume loopback (0<->1, 2<->3 etc.)""",
     )

    parser.add_argument("--def-gws", nargs='*', default=[], action='store',
                      help="""Default gateways to be used in created yaml file. Without them, will assume loopback (0<->1, 2<->3 etc.)""",
     )

    parser.add_argument("-o", default=None, action='store', metavar='PATH', dest = 'output_config',
                      help="""Output the config to this file.""",
     )

    parser.add_argument("--prefix", default=None, action='store',
                      help="""Advanced option: prefix to be used in TRex config in case of parallel instances.""",
     )

    parser.add_argument("--zmq-pub-port", default=None, action='store',
                      help="""Advanced option: ZMQ Publisher port to be used in TRex config in case of parallel instances.""",
     )

    parser.add_argument("--zmq-rpc-port", default=None, action='store',
                      help="""Advanced option: ZMQ RPC port to be used in TRex config in case of parallel instances.""",
     )

    parser.add_argument("--ignore-numa", default=False, action='store_true',
                      help="""Advanced option: Ignore NUMAs for config creation. Use this option only if you have to, as it will reduce performance.""",
     )

    parser.add_argument("-s", "--show", action='store_true',
                      help=""" show the status """,
     )

    parser.add_argument("-t", "--table", action='store_true',
                      help=""" show table with NICs info """,
     )

    parser.add_argument('--version', action='version',
                        version="0.2" )

    map_driver.args = parser.parse_args();
    if map_driver.args.parent :
        parse_parent_cfg (map_driver.args.parent)
        if map_driver.parent_args.cfg:
            map_driver.cfg_file = map_driver.parent_args.cfg;
    if  map_driver.args.cfg :
        map_driver.cfg_file = map_driver.args.cfg;

def main ():
    try:
        if os.getuid() != 0:
            raise DpdkSetup('Please run this program as root/with sudo')

        process_options ()

        if map_driver.args.show:
            dpdk_nic_bind.show_status()
            return

        if map_driver.args.table:
            dpdk_nic_bind.show_table()
            return

        if map_driver.args.dump_pci_desc:
            dpdk_nic_bind.dump_pci_description()
            return

        obj =CIfMap(map_driver.cfg_file);

        if map_driver.args.create_interfaces is not None:
            obj.do_create();
        elif map_driver.args.interactive:
            obj.do_interactive_create();
        elif map_driver.args.linux:
            obj.do_return_to_linux();
        else:
            ret = obj.do_run()
            print('The ports are bound/configured.')
            sys.exit(ret)
        print('')
    except DpdkSetup as e:
        print(e)
        sys.exit(-1)
    except Exception:
        traceback.print_exc()
        sys.exit(-1)
    except KeyboardInterrupt:
        print('Ctrl+C')
        sys.exit(-1)





if __name__ == '__main__':
    main()

