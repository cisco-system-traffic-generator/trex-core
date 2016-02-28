#! /usr/bin/python
# hhaim 
import sys,site
site.addsitedir('python-lib') 
import yaml
import os.path
import os
import dpdk_nic_bind
import re
import argparse;



class map_driver(object):
     args=None;
     cfg_file='/etc/trex_cfg.yaml'

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

        fcfg=self.m_cfg_file

        if not os.path.isfile(fcfg) :
            self.raise_error ("There is no valid configuration file %s " % fcfg)

        try:
          stream = open(fcfg, 'r')
          self.m_cfg_dict= yaml.load(stream)
        except Exception,e:
          print e;
          raise e

        stream.close();
        cfg_dict = self.m_cfg_dict[0]
        if not cfg_dict.has_key('version') :
            self.raise_error ("Configuration file %s is old, should include version field\n" % fcfg )

        if int(cfg_dict['version'])<2 :
            self.raise_error ("Configuration file %s is old, should include version field with value greater than 2\n" % fcfg)

        if not self.m_cfg_dict[0].has_key('interfaces') :
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
        cmd='./dpdk_nic_bind.py --force --bind=igb_uio %s ' % ( key)
        print cmd
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
        self.load_config_file ()
        self.run_dpdk_lspci ()

        if_list=self.m_cfg_dict[0]['interfaces']

        for obj in if_list:
            key= self.pci_name_to_full_name (obj)
            if not self.m_devices.has_key(key) :
                err=" %s does not exist " %key;
                raise DpdkSetup(err)


            if self.m_devices[key].has_key('Driver_str'):
                if self.m_devices[key]['Driver_str'] !='igb_uio' :
                    self.do_bind_one (key)
            else:
                self.do_bind_one (key)


    def do_create (self):
        print " not supported yet !"


def parse_parent_cfg (parent_cfg):
    l=parent_cfg.split(" ");
    cfg_file='';
    next=False;
    for obj in l:
        if next:
            cfg_file=obj
            next=False;
        if obj == '--cfg':
            next=True

    return (cfg_file)

def process_options ():
    parser = argparse.ArgumentParser(usage=""" 

Examples:
---------

To unbind the interfaces using the trex configuration file 
  dpdk_set_ports.py -l

To create a default file 
  dpdk_set_ports.py -c

To show status
  dpdk_set_ports.py -s

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
                      help=""" parent configuration CLI, extract --cfg from it if given  """,
     )

    parser.add_argument("-c", "--create", action='store_true',
                      help=""" try to create a configuration file. It is heuristic try to look into the file before """,
     )

    parser.add_argument("-s", "--show", action='store_true',
                      help=""" show the status """,
     )

    parser.add_argument('--version', action='version',
                        version="0.1" )

    map_driver.args = parser.parse_args();

    if map_driver.args.parent :
        cfg = parse_parent_cfg (map_driver.args.parent)
        if cfg != '':
            map_driver.cfg_file = cfg;
    if  map_driver.args.cfg :
        map_driver.cfg_file = map_driver.args.cfg;

def main ():
    try:
        process_options ()

        if map_driver.args.show:
            res=os.system('./dpdk_nic_bind.py --status');
            return(res);

        obj =CIfMap(map_driver.cfg_file);

        if map_driver.args.create:
            obj.do_create();
        else:
            obj.do_run();
    except Exception,e:
      print e
      exit(-1)

main();

