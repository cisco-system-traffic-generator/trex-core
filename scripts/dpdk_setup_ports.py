#! /usr/bin/python
# hhaim 
import sys
import os
python_ver = 'python%s' % sys.version_info[0]
sys.path.append(os.path.join('external_libs', 'pyyaml-3.11', python_ver))
import yaml
import dpdk_nic_bind
import re
import argparse
import copy
import shlex
import traceback
from collections import defaultdict, OrderedDict
from distutils.util import strtobool

class ConfigCreator(object):
    mandatory_interface_fields = ['Slot_str', 'src_mac', 'dest_mac', 'Device_str', 'NUMA']
    _2hex_re = '[\da-fA-F]{2}'
    mac_re = re.compile('^({0}:){{5}}{0}$'.format(_2hex_re))

    # cpu_topology - dict: physical processor -> physical core -> logical processing unit (thread)
    # interfaces - array of dicts per interface, should include "mandatory_interface_fields" values
    def __init__(self, cpu_topology, interfaces, include_lcores = [], exclude_lcores = [], only_first_thread = False, zmq_rpc_port = None, zmq_pub_port = None, prefix = None):
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
        for cores in self.cpu_topology.values():
            for core, lcores in cores.items():
                for lcore in copy.copy(lcores):
                    if include_lcores and lcore not in include_lcores:
                        cores[core].remove(lcore)
                    if exclude_lcores and lcore in exclude_lcores:
                        cores[core].remove(lcore)
                if 0 in lcores:
                    self.has_zero_lcore = True
                    cores[core].remove(0)
        Device_str = None
        for interface in self.interfaces:
            for mandatory_interface_field in ConfigCreator.mandatory_interface_fields:
                if mandatory_interface_field not in interface:
                    raise DpdkSetup("Expected '%s' field in interface dictionary, got: %s" % (mandatory_interface_field, interface))
            if Device_str is None:
                Device_str = interface['Device_str']
            elif Device_str != interface['Device_str']:
                raise DpdkSetup('Interfaces should be of same type, got:\n\t* %s\n\t* %s' % (Device_str, interface['Device_str']))
        if '40Gb' in Device_str:
            self.speed = 40
        else:
            self.speed = 10
        lcores_per_numa = OrderedDict()
        system_lcores = int(self.has_zero_lcore)
        for numa, core in self.cpu_topology.items():
            for lcores in core.values():
                if numa not in lcores_per_numa:
                    lcores_per_numa[numa] = []
                lcores_per_numa[numa].extend(lcores)
                system_lcores += len(lcores)
        minimum_required_lcores = len(self.interfaces) / 2 + 2
        if system_lcores < minimum_required_lcores:
            raise DpdkSetup('Your system should have at least %s cores for %s interfaces, and it has: %s.' %
                    (minimum_required_lcores, len(self.interfaces), system_lcores + (0 if self.has_zero_lcore else 1)))
        #if not stateless_only:
        interfaces_per_numa = defaultdict(int)
        for i in range(0, len(self.interfaces), 2):
            if self.interfaces[i]['NUMA'] != self.interfaces[i+1]['NUMA']:
                raise DpdkSetup('NUMA of each pair of interfaces should be same, got NUMA %s for client interface %s, NUMA %s for server interface %s' %
                        (self.interfaces[i]['NUMA'], self.interfaces[i]['Slot_str'], self.interfaces[i+1]['NUMA'], self.interfaces[i+1]['Slot_str']))
            interfaces_per_numa[self.interfaces[i]['NUMA']] += 2
        self.lcores_per_numa     = lcores_per_numa
        self.interfaces_per_numa = interfaces_per_numa
        self.prefix              = prefix
        self.zmq_pub_port        = zmq_pub_port
        self.zmq_rpc_port        = zmq_rpc_port

    def _convert_mac(self, mac_string):
        if not ConfigCreator.mac_re.match(mac_string):
            raise DpdkSetup('MAC address should be in format of 12:34:56:78:9a:bc, got: %s' % mac_string)
        return ', '.join([('0x%s' % elem).lower() for elem in mac_string.split(':')])

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
        for interface in self.interfaces:
            config_str += ' '*6 + '- dest_mac: [%s]\n' % self._convert_mac(interface['dest_mac'])
            config_str += ' '*8 + 'src_mac:  [%s]\n' % self._convert_mac(interface['src_mac'])
        config_str += '\n  platform:\n'
        if len(self.interfaces_per_numa.keys()) == 1 and -1 in self.interfaces_per_numa: # VM, use any cores, 1 core per dual_if
            lcores_pool = sorted([lcore for lcores in self.lcores_per_numa.values() for lcore in lcores])
            config_str += ' '*6 + 'master_thread_id: %s\n' % (0 if self.has_zero_lcore else lcores_pool.pop())
            config_str += ' '*6 + 'latency_thread_id: %s\n' % lcores_pool.pop()
            config_str += ' '*6 + 'dual_if:\n'
            for i in range(0, len(self.interfaces), 2):
                config_str += ' '*8 + '- socket: 0\n'
                config_str += ' '*10 + 'threads: [%s]\n\n' % lcores_pool.pop()
        else:
            # we will take common minimum among all NUMAs, to satisfy all
            lcores_per_dual_if = 99
            extra_lcores = 1 if self.has_zero_lcore else 2
            # worst case 3 iterations, to ensure master and "rx" have cores left
            while (lcores_per_dual_if * sum(self.interfaces_per_numa.values()) / 2) + extra_lcores > sum([len(lcores) for lcores in self.lcores_per_numa.values()]):
                lcores_per_dual_if -= 1
                for numa, cores in self.lcores_per_numa.items():
                    if not self.interfaces_per_numa[numa]:
                        continue
                    lcores_per_dual_if = min(lcores_per_dual_if, int(2 * len(cores) / self.interfaces_per_numa[numa]))
            lcores_pool = copy.deepcopy(self.lcores_per_numa)
            # first, allocate lcores for dual_if section
            dual_if_section = ' '*6 + 'dual_if:\n'
            for i in range(0, len(self.interfaces), 2):
                numa = self.interfaces[i]['NUMA']
                dual_if_section += ' '*8 + '- socket: %s\n' % numa
                lcores_for_this_dual_if = [str(lcores_pool[numa].pop()) for _ in range(lcores_per_dual_if)]
                if not lcores_for_this_dual_if:
                    raise DpdkSetup('Not enough cores at NUMA %s. This NUMA has %s processing units and %s interfaces.' % (numa, len(self.lcores_per_numa[numa]), self.interfaces_per_numa[numa]))
                dual_if_section += ' '*10 + 'threads: [%s]\n\n' % ','.join(lcores_for_this_dual_if)
            # take the cores left to master and rx
            lcores_pool_left = [lcore for lcores in lcores_pool.values() for lcore in lcores]
            config_str += ' '*6 + 'master_thread_id: %s\n' % (0 if self.has_zero_lcore else lcores_pool_left.pop())
            config_str += ' '*6 + 'latency_thread_id: %s\n' % lcores_pool_left.pop()
            # add the dual_if section
            config_str += dual_if_section

        # verify config is correct YAML format
        try:
            yaml.safe_load(config_str)
        except Exception as e:
            raise DpdkSetup('Could not create correct yaml config.\nGenerated YAML:\n%s\nEncountered error:\n%s' % (config_str, e))

        if filename:
            if os.path.exists(filename):
                result = None
                try:
                    result = strtobool(raw_input('File %s already exist, overwrite? (y/N)' % filename))
                except:
                    pass
                finally:
                    if not result:
                        print('Skipping.')
                        return config_str
            with open(filename, 'w') as f:
                f.write(config_str)
            print('Saved.')
        if print_config:
            print(config_str)
        return config_str


class map_driver(object):
    args=None;
    cfg_file='/etc/trex_cfg.yaml'
    parent_cfg = None
    dump_interfaces = None

class DpdkSetup(Exception):
    pass

class CIfMap:

    def __init__(self, cfg_file):
        self.m_cfg_file =cfg_file;
        self.m_cfg_dict={};
        self.m_devices={};

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

    def load_config_file (self):
        #return

        fcfg=self.m_cfg_file

        if not os.path.isfile(fcfg) :
            self.raise_error ("There is no valid configuration file %s " % fcfg)

        try:
          stream = open(fcfg, 'r')
          self.m_cfg_dict= yaml.load(stream)
        except Exception as e:
          print(e);
          raise e

        stream.close();
        cfg_dict = self.m_cfg_dict[0]
        if 'version' not in cfg_dict:
            self.raise_error ("Configuration file %s is old, should include version field\n" % fcfg )

        if int(cfg_dict['version'])<2 :
            self.raise_error ("Configuration file %s is old, should include version field with value greater than 2\n" % fcfg)

        if 'interfaces' not in self.m_cfg_dict[0]:
            self.raise_error ("Configuration file %s is old, should include interfaces field even number of elemets" % fcfg)

        if_list=self.m_cfg_dict[0]['interfaces']
        l=len(if_list);
        if (l>20):
            self.raise_error ("Configuration file %s should include interfaces field with maximum of number of elemets" % (fcfg,l))
        if ((l % 2)==1):
            self.raise_error ("Configuration file %s should include even number of interfaces " % (fcfg,l))
        if 'port_limit' in cfg_dict and cfg_dict['port_limit'] > len(if_list):
            self.raise_error ('Error: port_limit should not be higher than number of interfaces in config file: %s\n' % fcfg)


    def do_bind_one (self,key):
        cmd='%s dpdk_nic_bind.py --bind=igb_uio %s ' % (sys.executable, key)
        print(cmd)
        res=os.system(cmd);
        if res!=0:
            raise DpdkSetup('')



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

    def do_run (self):
        self.run_dpdk_lspci ()
        if map_driver.dump_interfaces is None or (map_driver.dump_interfaces is [] and map_driver.parent_cfg):
            self.load_config_file()
            if_list=self.m_cfg_dict[0]['interfaces']
        else:
            if_list = map_driver.dump_interfaces

        for obj in if_list:
            key= self.pci_name_to_full_name (obj)
            if key not in self.m_devices:
                err=" %s does not exist " %key;
                raise DpdkSetup(err)


            if 'Driver_str' in self.m_devices[key]:
                if self.m_devices[key]['Driver_str'] !='igb_uio' :
                    self.do_bind_one (key)
            else:
                self.do_bind_one (key)


    def do_create(self):
        create_interfaces = map_driver.args.create_interfaces
        if type(create_interfaces) is not list:
            raise DpdkSetup('type of map_driver.args.create_interfaces should be list')
        if not len(create_interfaces):
            raise DpdkSetup('Please specify interfaces to use in the config')
        if len(create_interfaces) % 2:
            raise DpdkSetup('Please specify even number of interfaces')
        # gather info about NICS from dpdk_nic_bind.py
        if not self.m_devices:
            self.run_dpdk_lspci()
        wanted_interfaces = []
        for interface in copy.copy(create_interfaces):
            for m_device in self.m_devices.values():
                if interface in (m_device['Interface'], m_device['Slot'], m_device['Slot_str']):
                    if m_device in wanted_interfaces:
                        raise DpdkSetup('Interface %s is specified twice' % interface)
                    m_device['Interface_argv'] = interface
                    wanted_interfaces.append(m_device)
                    create_interfaces.remove(interface)
                    break
        # verify all interfaces identified
        if len(create_interfaces):
            raise DpdkSetup('Could not find information about those interfaces: %s' % create_interfaces)

        dpdk_bound = []
        for interface in wanted_interfaces:
            if 'Driver_str' not in interface:
                self.do_bind_one(interface['Slot'])
                dpdk_bound.append(interface['Slot'])
            elif interface['Driver_str'] == 'igb_uio':
                dpdk_bound.append(interface['Slot'])
        if dpdk_bound:
            for pci, mac in dpdk_nic_bind.get_macs_from_trex(dpdk_bound).items():
                self.m_devices[pci]['MAC'] = mac

        dest_macs = map_driver.args.dest_macs
        for i, interface in enumerate(wanted_interfaces):
            interface['src_mac'] = interface['MAC']
            if isinstance(dest_macs, list) and len(dest_macs) > i:
                interface['dest_mac'] = dest_macs[i]
            dual_index = i + 1 - (i % 2) * 2
            if 'dest_mac' not in wanted_interfaces[dual_index]:
                wanted_interfaces[dual_index]['dest_mac'] = interface['MAC'] # loopback

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
                numa = int(lcore_dict['physical id'])
                if numa not in cpu_topology:
                    cpu_topology[numa] = OrderedDict()
                core = int(lcore_dict['core id'])
                if core not in cpu_topology[numa]:
                    cpu_topology[numa][core] = []
                cpu_topology[numa][core].append(int(lcore_dict['processor']))

        config = ConfigCreator(cpu_topology, wanted_interfaces, include_lcores = map_driver.args.create_include, exclude_lcores = map_driver.args.create_exclude,
                               only_first_thread = map_driver.args.no_ht,
                               prefix = map_driver.args.prefix, zmq_rpc_port = map_driver.args.zmq_rpc_port, zmq_pub_port = map_driver.args.zmq_pub_port)
        config.create_config(filename = map_driver.args.o, print_config = map_driver.args.dump)

def parse_parent_cfg (parent_cfg):
    parent_parser = argparse.ArgumentParser()
    parent_parser.add_argument('--cfg', default='')
    parent_parser.add_argument('--dump-interfaces', nargs='*', default=None)
    args, unkown = parent_parser.parse_known_args(shlex.split(parent_cfg))
    return (args.cfg, args.dump_interfaces)

def process_options ():
    parser = argparse.ArgumentParser(usage=""" 

Examples:
---------

To unbind the interfaces using the trex configuration file 
  sudo ./dpdk_set_ports.py -l

To create a default config file 
  sudo ./dpdk_setup_ports.py -c 02:00.0 02:00.1 -o /etc/trex_cfg.yaml

To show interfaces status
  sudo ./dpdk_set_ports.py -s

    """,
    description=" unbind dpdk interfaces ",
    epilog=" written by hhaim");

    parser.add_argument("-l", "--load", action='store_true',
                      help=""" unbind the interfaces using the configuration file given  """,
     )

    parser.add_argument("--cfg",
                      help=""" configuration file name  """,
     )

    parser.add_argument("--parent",  
                      help=argparse.SUPPRESS
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

    parser.add_argument("--dump", default=False, action='store_true',
                      help="""Dump created config to screen.""",
     )

    parser.add_argument("--no-ht", default=False, dest='no_ht', action='store_true',
                      help="""Use only one thread of each Core in created config yaml (No Hyper-Threading).""",
     )

    parser.add_argument("--dest-macs", nargs='*', default=[], action='store',
                      help="""Destination MACs to be used in created yaml file. Without them, will be assumed loopback (0<->1, 2<->3 etc.)""",
     )

    parser.add_argument("-o", default=None, action='store', metavar='PATH',
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

    parser.add_argument("-s", "--show", action='store_true',
                      help=""" show the status """,
     )

    parser.add_argument("-t", "--table", action='store_true',
                      help=""" show table with NICs info """,
     )

    parser.add_argument('--version', action='version',
                        version="0.1" )

    map_driver.args = parser.parse_args();
    if map_driver.args.parent :
        map_driver.parent_cfg, map_driver.dump_interfaces = parse_parent_cfg (map_driver.args.parent)
        if map_driver.parent_cfg != '':
            map_driver.cfg_file = map_driver.parent_cfg;
    if  map_driver.args.cfg :
        map_driver.cfg_file = map_driver.args.cfg;

def main ():
    try:
        process_options ()

        if map_driver.args.show:
            res=os.system('%s dpdk_nic_bind.py --status' % sys.executable);
            return(res);

        if map_driver.args.table:
            res=os.system('%s dpdk_nic_bind.py -t' % sys.executable);
            return(res);

        obj =CIfMap(map_driver.cfg_file);

        if map_driver.args.create_interfaces is not None:
            obj.do_create();
        else:
            obj.do_run();
    except Exception as e:
        #debug
        #traceback.print_exc()
        print(e)
        exit(-1)

if __name__ == '__main__':
    main()

