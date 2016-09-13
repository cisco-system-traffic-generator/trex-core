#! /usr/bin/python
#
#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import sys, os, getopt, subprocess, shlex
from os.path import exists, abspath, dirname, basename
from distutils.util import strtobool
text_tables_path = os.path.join('external_libs', 'texttable-0.8.4')
sys.path.append(text_tables_path)
import texttable
sys.path.remove(text_tables_path)
import re
import termios
import getpass

# The PCI device class for ETHERNET devices
ETHERNET_CLASS = "0200"

# global dict ethernet devices present. Dictionary indexed by PCI address.
# Each device within this is itself a dictionary of device properties
devices = {}
# list of supported DPDK drivers
dpdk_drivers = [ "igb_uio", "vfio-pci", "uio_pci_generic" ]

# command-line arg flags
b_flag = None
status_flag = False
table_flag = False
force_flag = False
args = []

try:
    raw_input
except: # Python3
    raw_input = input

def usage():
    '''Print usage information for the program'''
    argv0 = basename(sys.argv[0])
    print("""
Usage:
------

     %(argv0)s [options] DEVICE1 DEVICE2 ....

where DEVICE1, DEVICE2 etc, are specified via PCI "domain:bus:slot.func" syntax
or "bus:slot.func" syntax. For devices bound to Linux kernel drivers, they may
also be referred to by Linux interface name e.g. eth0, eth1, em0, em1, etc.

Options:
    --help, --usage:
        Display usage information and quit

    -s, --status:
        Print the current status of all known network interfaces.
        For each device, it displays the PCI domain, bus, slot and function,
        along with a text description of the device. Depending upon whether the
        device is being used by a kernel driver, the igb_uio driver, or no
        driver, other relevant information will be displayed:
        * the Linux interface name e.g. if=eth0
        * the driver being used e.g. drv=igb_uio
        * any suitable drivers not currently using that device
            e.g. unused=igb_uio
        NOTE: if this flag is passed along with a bind/unbind option, the status
        display will always occur after the other operations have taken place.

    -t, --table:
        Similar to --status, but gives more info: NUMA, MAC etc.

    -b driver, --bind=driver:
        Select the driver to use or \"none\" to unbind the device

    -u, --unbind:
        Unbind a device (Equivalent to \"-b none\")

    --force:
        By default, devices which are used by Linux - as indicated by having
        established connections - cannot be modified. Using the --force
        flag overrides this behavior, allowing active links to be forcibly
        unbound.
        WARNING: This can lead to loss of network connection and should be used
        with caution.

Examples:
---------

To display current device status:
        %(argv0)s --status

To bind eth1 from the current driver and move to use igb_uio
        %(argv0)s --bind=igb_uio eth1

To unbind 0000:01:00.0 from using any driver
        %(argv0)s -u 0000:01:00.0

To bind 0000:02:00.0 and 0000:02:00.1 to the ixgbe kernel driver
        %(argv0)s -b ixgbe 02:00.0 02:00.1

    """ % locals()) # replace items from local variables

# This is roughly compatible with check_output function in subprocess module
# which is only available in python 2.7.
def check_output(args, stderr=None, **kwargs):
    '''Run a command and capture its output'''
    return subprocess.Popen(args, stdout=subprocess.PIPE,
                            stderr=stderr, **kwargs).communicate()[0]

def find_module(mod):
    '''find the .ko file for kernel module named mod.
    Searches the $RTE_SDK/$RTE_TARGET directory, the kernel
    modules directory and finally under the parent directory of
    the script '''
    # check $RTE_SDK/$RTE_TARGET directory
    if 'RTE_SDK' in os.environ and 'RTE_TARGET' in os.environ:
        path = "%s/%s/kmod/%s.ko" % (os.environ['RTE_SDK'],\
                                     os.environ['RTE_TARGET'], mod)
        if exists(path):
            return path

    # check using depmod
    try:
        depmod_out = check_output(["modinfo", "-n", mod], \
                                  stderr=subprocess.STDOUT, universal_newlines = True).lower()
        if "error" not in depmod_out:
            path = depmod_out.strip()
            if exists(path):
                return path
    except: # if modinfo can't find module, it fails, so continue
        pass

    # check for a copy based off current path
    tools_dir = dirname(abspath(sys.argv[0]))
    if (tools_dir.endswith("tools")):
        base_dir = dirname(tools_dir)
        find_out = check_output(["find", base_dir, "-name", mod + ".ko"], universal_newlines = True)
        if len(find_out) > 0: #something matched
            path = find_out.splitlines()[0]
            if exists(path):
                return path

def check_modules():
    '''Checks that igb_uio is loaded'''
    global dpdk_drivers

    with open("/proc/modules") as fd:
        loaded_mods = fd.readlines()

    # list of supported modules
    mods =  [{"Name" : driver, "Found" : False} for driver in dpdk_drivers]

    # first check if module is loaded
    for line in loaded_mods:
        for mod in mods:
            if line.startswith(mod["Name"]):
                mod["Found"] = True
            # special case for vfio_pci (module is named vfio-pci,
            # but its .ko is named vfio_pci)
            elif line.replace("_", "-").startswith(mod["Name"]):
                mod["Found"] = True

    # check if we have at least one loaded module
    if True not in [mod["Found"] for mod in mods] and b_flag is not None:
        if b_flag in dpdk_drivers:
            print("Error - no supported modules(DPDK driver) are loaded")
            sys.exit(1)
        else:
            print("Warning - no supported modules(DPDK driver) are loaded")

    # change DPDK driver list to only contain drivers that are loaded
    dpdk_drivers = [mod["Name"] for mod in mods if mod["Found"]]

def has_driver(dev_id):
    '''return true if a device is assigned to a driver. False otherwise'''
    return "Driver_str" in devices[dev_id]

def get_pci_device_details(dev_id):
    '''This function gets additional details for a PCI device'''
    device = {}

    extra_info = check_output(["lspci", "-vmmks", dev_id], universal_newlines = True).splitlines()

    # parse lspci details
    for line in extra_info:
        if len(line) == 0:
            continue
        name, value = line.split("\t", 1)
        name = name.strip(":") + "_str"
        device[name] = value
    # check for a unix interface name
    sys_path = "/sys/bus/pci/devices/%s/net/" % dev_id
    if exists(sys_path):
        device["Interface"] = ",".join(os.listdir(sys_path))
        device['net_path'] = sys_path
    else:
        device["Interface"] = ""
        device['net_path'] = None
        for path, dirs, _ in os.walk('/sys/bus/pci/devices/%s' % dev_id):
            if 'net' in dirs:
                device['net_path'] = os.path.join(path, 'net')
                device["Interface"] = ",".join(os.listdir(device['net_path']))
                break
    # check if a port is used for connections (ssh etc)
    device["Active"] = ""

    return device

def get_nic_details():
    '''This function populates the "devices" dictionary. The keys used are
    the pci addresses (domain:bus:slot.func). The values are themselves
    dictionaries - one for each NIC.'''
    global devices
    global dpdk_drivers

    # clear any old data
    devices = {}
    # first loop through and read details for all devices
    # request machine readable format, with numeric IDs
    dev = {};
    dev_lines = check_output(["lspci", "-Dvmmn"], universal_newlines = True).splitlines()
    for dev_line in dev_lines:
        if (len(dev_line) == 0):
            if dev["Class"] == ETHERNET_CLASS:
                #convert device and vendor ids to numbers, then add to global
                dev["Vendor"] = int(dev["Vendor"],16)
                dev["Device"] = int(dev["Device"],16)
                devices[dev["Slot"]] = dict(dev) # use dict to make copy of dev
        else:
            name, value = dev_line.split("\t", 1)
            dev[name.rstrip(":")] = value

    # check active interfaces with established connections
    established_ip = set()
    for line in check_output(['netstat', '-tn'], universal_newlines = True).splitlines(): # tcp        0      0 10.56.216.133:22        10.56.46.20:45079       ESTABLISHED
        line_arr = line.split()
        if len(line_arr) == 6 and line_arr[0] == 'tcp' and line_arr[5] == 'ESTABLISHED':
            established_ip.add(line_arr[3].rsplit(':', 1)[0])

    active_if = []
    for line in check_output(["ip", "-o", "addr"], universal_newlines = True).splitlines(): # 6: eth4    inet 10.56.216.133/24 brd 10.56.216.255 scope global eth4\       valid_lft forever preferred_lft forever
        line_arr = line.split()
        if len(line_arr) > 6 and line_arr[2] == 'inet':
            if line_arr[3].rsplit('/', 1)[0] in established_ip:
                active_if.append(line_arr[1])

    # based on the basic info, get extended text details
    for d in devices.keys():
        # get additional info and add it to existing data
        devices[d] = dict(list(devices[d].items()) +
                          list(get_pci_device_details(d).items()))

        for _if in active_if:
            if _if in devices[d]["Interface"].split(","):
                devices[d]["Active"] = "*Active*"
                break;

        # add igb_uio to list of supporting modules if needed
        if "Module_str" in devices[d]:
            for driver in dpdk_drivers:
                if driver not in devices[d]["Module_str"]:
                    devices[d]["Module_str"] = devices[d]["Module_str"] + ",%s" % driver
        else:
            devices[d]["Module_str"] = ",".join(dpdk_drivers)

        # make sure the driver and module strings do not have any duplicates
        if has_driver(d):
            modules = devices[d]["Module_str"].split(",")
            if devices[d]["Driver_str"] in modules:
                modules.remove(devices[d]["Driver_str"])
                devices[d]["Module_str"] = ",".join(modules)

        # get MAC from Linux if available
        if devices[d]['net_path']:
            mac_file = '%s/%s/address' % (devices[d]['net_path'], devices[d]['Interface'])
            if os.path.exists(mac_file):
                with open(mac_file) as f:
                    devices[d]['MAC'] = f.read().strip()

        # get NUMA from Linux if available
        numa_node_file = '/sys/bus/pci/devices/%s/numa_node' % devices[d]['Slot']
        if os.path.exists(numa_node_file):
            with open(numa_node_file) as f:
                devices[d]['NUMA'] = int(f.read().strip())

def dev_id_from_dev_name(dev_name):
    '''Take a device "name" - a string passed in by user to identify a NIC
    device, and determine the device id - i.e. the domain:bus:slot.func - for
    it, which can then be used to index into the devices array'''
    dev = None
    # check if it's already a suitable index
    if dev_name in devices:
        return dev_name
    # check if it's an index just missing the domain part
    elif "0000:" + dev_name in devices:
        return "0000:" + dev_name
    else:
        # check if it's an interface name, e.g. eth1
        for d in devices.keys():
            if dev_name in devices[d]["Interface"].split(","):
                return devices[d]["Slot"]
    # if nothing else matches - error
    print("Unknown device: %s. " \
        "Please specify device in \"bus:slot.func\" format" % dev_name)
    sys.exit(1)

def get_igb_uio_usage():
    for driver in dpdk_drivers:
        refcnt_file = '/sys/module/%s/refcnt' % driver
        if not os.path.exists(refcnt_file):
            continue
        with open(refcnt_file) as f:
            ref_cnt = int(f.read().strip())
            if ref_cnt:
                return True
    return False

def get_pid_using_pci(pci_list):
    if not isinstance(pci_list, list):
        pci_list = [pci_list]
    pci_list = map(dev_id_from_dev_name, pci_list)
    for pid in os.listdir('/proc'):
        try:
            int(pid)
        except ValueError:
            continue
        with open('/proc/%s/maps' % pid) as f:
            f_cont = f.read()
        for pci in pci_list:
            if '/%s/' % pci in f_cont:
                return int(pid)

def read_pid_cmdline(pid):
    cmdline_file = '/proc/%s/cmdline' % pid
    if not os.path.exists(cmdline_file):
        return None
    with open(cmdline_file, 'rb') as f:
        return f.read().replace(b'\0', b' ').decode(errors = 'replace')

def confirm(msg, default = False):
    if not os.isatty(1):
        return default
    termios.tcflush(sys.stdin, termios.TCIFLUSH)
    try:
        return strtobool(raw_input(msg))
    except KeyboardInterrupt:
        print('')
        sys.exit(1)
    except Exception:
        return default

def read_line(msg = '', default = ''):
    if not os.isatty(1):
        return default
    termios.tcflush(sys.stdin, termios.TCIFLUSH)
    try:
        return raw_input(msg).strip()
    except KeyboardInterrupt:
        print('')
        sys.exit(1)

def unbind_one(dev_id, force):
    '''Unbind the device identified by "dev_id" from its current driver'''
    dev = devices[dev_id]
    if not has_driver(dev_id):
        print("%s %s %s is not currently managed by any driver\n" % \
            (dev[b"Slot"], dev[b"Device_str"], dev[b"Interface"]))
        return

    if not force and dev.get('Driver_str') in dpdk_drivers and get_igb_uio_usage():
        pid = get_pid_using_pci(dev_id)
        if pid:
            cmdline = read_pid_cmdline(pid)
            print('Interface %s is in use by process:\n%s' % (dev_id, cmdline))
            if not confirm('Unbinding might hang the process. Confirm unbind (y/N)'):
                print('Not unbinding.')
                return

    # prevent us disconnecting ourselves
    if dev["Active"] and not force:
        print('netstat indicates that interface %s is active.' % dev_id)
        if not confirm('Confirm unbind (y/N)'):
            print('Not unbinding.')
            return

    # write to /sys to unbind
    filename = "/sys/bus/pci/drivers/%s/unbind" % dev["Driver_str"]
    try:
        f = open(filename, "a")
    except:
        print("Error: unbind failed for %s - Cannot open %s" % (dev_id, filename))
        sys.exit(1)
    f.write(dev_id)
    f.close()

def bind_one(dev_id, driver, force):
    '''Bind the device given by "dev_id" to the driver "driver". If the device
    is already bound to a different driver, it will be unbound first'''
    dev = devices[dev_id]
    saved_driver = None # used to rollback any unbind in case of failure

    # prevent disconnection of our ssh session
    if dev["Active"] and not force:
        print("netstat indicates that interface %s is active" % dev_id)
        if not confirm("Confirm bind (y/N)"):
            print('Not binding.')
            return

    # unbind any existing drivers we don't want
    if has_driver(dev_id):
        if dev["Driver_str"] == driver:
            print("%s already bound to driver %s, skipping\n" % (dev_id, driver))
            return
        else:
            saved_driver = dev["Driver_str"]
            if not force and get_igb_uio_usage():
                pid = get_pid_using_pci(dev_id)
                if pid:
                    cmdline = read_pid_cmdline(pid)
                    print('Interface %s is in use by process:\n%s' % (dev_id, cmdline))
                    if not confirm('Binding to other driver might hang the process. Confirm unbind (y/N)'):
                        print('Not binding.')
                        return

            unbind_one(dev_id, force)
            dev["Driver_str"] = "" # clear driver string

    # if we are binding to one of DPDK drivers, add PCI id's to that driver
    if driver in dpdk_drivers:
        filename = "/sys/bus/pci/drivers/%s/new_id" % driver
        try:
            f = open(filename, "w")
        except:
            print("Error: bind failed for %s - Cannot open %s" % (dev_id, filename))
            return
        try:
            f.write("%04x %04x" % (dev["Vendor"], dev["Device"]))
            f.close()
        except:
            print("Error: bind failed for %s - Cannot write new PCI ID to " \
                "driver %s" % (dev_id, driver))
            return

    # do the bind by writing to /sys
    filename = "/sys/bus/pci/drivers/%s/bind" % driver
    try:
        f = open(filename, "a")
    except:
        print("Error: bind failed for %s - Cannot open %s" % (dev_id, filename))
        if saved_driver is not None: # restore any previous driver
            bind_one(dev_id, saved_driver, force)
        return
    try:
        f.write(dev_id)
        f.close()
    except:
        # for some reason, closing dev_id after adding a new PCI ID to new_id
        # results in IOError. however, if the device was successfully bound,
        # we don't care for any errors and can safely ignore IOError
        tmp = get_pci_device_details(dev_id)
        if "Driver_str" in tmp and tmp["Driver_str"] == driver:
            return
        print("Error: bind failed for %s - Cannot bind to driver %s" % (dev_id, driver))
        if saved_driver is not None: # restore any previous driver
            bind_one(dev_id, saved_driver, force)
        return


def unbind_all(dev_list, force=False):
    """Unbind method, takes a list of device locations"""
    dev_list = map(dev_id_from_dev_name, dev_list)
    for d in dev_list:
        unbind_one(d, force)

def bind_all(dev_list, driver, force=False):
    """Bind method, takes a list of device locations"""
    global devices

    dev_list = list(map(dev_id_from_dev_name, dev_list))

    for d in dev_list:
        bind_one(d, driver, force)

    # when binding devices to a generic driver (i.e. one that doesn't have a
    # PCI ID table), some devices that are not bound to any other driver could
    # be bound even if no one has asked them to. hence, we check the list of
    # drivers again, and see if some of the previously-unbound devices were
    # erroneously bound.
    for d in devices.keys():
        # skip devices that were already bound or that we know should be bound
        if "Driver_str" in devices[d] or d in dev_list:
            continue

        # update information about this device
        devices[d] = dict(list(devices[d].items()) +
                          list(get_pci_device_details(d).items()))

        # check if updated information indicates that the device was bound
        if "Driver_str" in devices[d]:
            unbind_one(d, force)

def display_devices(title, dev_list, extra_params = None):
    '''Displays to the user the details of a list of devices given in "dev_list"
    The "extra_params" parameter, if given, should contain a string with
    %()s fields in it for replacement by the named fields in each device's
    dictionary.'''
    strings = [] # this holds the strings to print. We sort before printing
    print("\n%s" % title)
    print("="*len(title))
    if len(dev_list) == 0:
        strings.append("<none>")
    else:
        for dev in dev_list:
            if extra_params is not None:
                strings.append("%s '%s' %s" % (dev["Slot"], \
                                dev["Device_str"], extra_params % dev))
            else:
                strings.append("%s '%s'" % (dev["Slot"], dev["Device_str"]))
    # sort before printing, so that the entries appear in PCI order
    strings.sort()
    print("\n".join(strings)) # print one per line

def show_status():
    '''Function called when the script is passed the "--status" option. Displays
    to the user what devices are bound to the igb_uio driver, the kernel driver
    or to no driver'''
    global dpdk_drivers
    if not devices:
        get_nic_details()
    kernel_drv = []
    dpdk_drv = []
    no_drv = []

    # split our list of devices into the three categories above
    for d in devices.keys():
        if not has_driver(d):
            no_drv.append(devices[d])
            continue
        if devices[d]["Driver_str"] in dpdk_drivers:
            dpdk_drv.append(devices[d])
        else:
            kernel_drv.append(devices[d])

    # print each category separately, so we can clearly see what's used by DPDK
    display_devices("Network devices using DPDK-compatible driver", dpdk_drv, \
                    "drv=%(Driver_str)s unused=%(Module_str)s")
    display_devices("Network devices using kernel driver", kernel_drv,
                    "if=%(Interface)s drv=%(Driver_str)s unused=%(Module_str)s %(Active)s")
    display_devices("Other network devices", no_drv,\
                    "unused=%(Module_str)s")

def get_info_from_trex(pci_addr_list):
    if not pci_addr_list:
        return {}
    pci_info_dict = {}
    run_command = 'sudo ./t-rex-64 --dump-interfaces %s' % ' '.join(pci_addr_list)
    proc = subprocess.Popen(shlex.split(run_command), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, universal_newlines = True)
    stdout, _ = proc.communicate()
    if proc.returncode:
        if 'PANIC in rte_eal_init' in stdout:
            print("Could not run TRex to get MAC info about interfaces, check if it's already running.")
        else:
            print('Error upon running TRex to get MAC info:\n%s' % stdout)
        sys.exit(1)
    pci_mac_str = 'PCI: (\S+).+?MAC: (\S+).+?Driver: (\S+)'
    pci_mac_re = re.compile(pci_mac_str)
    for line in stdout.splitlines():
        match = pci_mac_re.match(line)
        if match:
            pci = match.group(1)
            if pci not in pci_addr_list: # sanity check, should not happen
                print('Internal error while getting info of DPDK bound interfaces, unknown PCI: %s' % pci)
                sys.exit(1)
            pci_info_dict[pci] = {}
            pci_info_dict[pci]['MAC'] = match.group(2)
            pci_info_dict[pci]['TRex_Driver'] = match.group(3)
    return pci_info_dict

def show_table():
    '''Function called when the script is passed the "--table" option.
    Similar to show_status() function, but shows more info: NUMA etc.'''
    global dpdk_drivers
    if not devices:
        get_nic_details()
    dpdk_drv = []
    for d in devices.keys():
        if devices[d].get("Driver_str") in dpdk_drivers:
            dpdk_drv.append(d)

    for pci, info in get_info_from_trex(dpdk_drv).items():
        if pci not in dpdk_drv: # sanity check, should not happen
            print('Internal error while getting MACs of DPDK bound interfaces, unknown PCI: %s' % pci)
            return
        devices[pci].update(info)
        
    table = texttable.Texttable(max_width=-1)
    table.header(['ID', 'NUMA', 'PCI', 'MAC', 'Name', 'Driver', 'Linux IF', 'Active'])
    for id, pci in enumerate(sorted(devices.keys())):
        d = devices[pci]
        table.add_row([id, d['NUMA'], d['Slot_str'], d.get('MAC', ''), d['Device_str'], d.get('Driver_str', ''), d['Interface'], d['Active']])
    print(table.draw())

def parse_args():
    '''Parses the command-line arguments given by the user and takes the
    appropriate action for each'''
    global b_flag
    global status_flag
    global table_flag
    global force_flag
    global args
    if len(sys.argv) <= 1:
        usage()
        sys.exit(0)

    try:
        opts, args = getopt.getopt(sys.argv[1:], "b:ust",
                               ["help", "usage", "status", "table", "force",
                                "bind=", "unbind"])
    except getopt.GetoptError as error:
        print(str(error))
        print("Run '%s --usage' for further information" % sys.argv[0])
        sys.exit(1)

    for opt, arg in opts:
        if opt == "--help" or opt == "--usage":
            usage()
            sys.exit(0)
        if opt == "--status" or opt == "-s":
            status_flag = True
        if opt == "--table" or opt == "-t":
            table_flag = True
        if opt == "--force":
            force_flag = True
        if opt == "-b" or opt == "-u" or opt == "--bind" or opt == "--unbind":
            if b_flag is not None:
                print("Error - Only one bind or unbind may be specified\n")
                sys.exit(1)
            if opt == "-u" or opt == "--unbind":
                b_flag = "none"
            else:
                b_flag = arg

def do_arg_actions():
    '''do the actual action requested by the user'''
    global b_flag
    global status_flag
    global table_flag
    global force_flag
    global args

    if b_flag is None and not status_flag and not table_flag:
        print("Error: No action specified for devices. Please give a -b or -u option")
        print("Run '%s --usage' for further information" % sys.argv[0])
        sys.exit(1)

    if b_flag is not None and len(args) == 0:
        print("Error: No devices specified.")
        print("Run '%s --usage' for further information" % sys.argv[0])
        sys.exit(1)

    if b_flag == "none" or b_flag == "None":
        unbind_all(args, force_flag)
    elif b_flag is not None:
        bind_all(args, b_flag, force_flag)
    if status_flag:
        if b_flag is not None:
            get_nic_details() # refresh if we have changed anything
        show_status()
    if table_flag:
        if b_flag is not None:
            get_nic_details() # refresh if we have changed anything
        show_table()

def main():
    '''program main function'''
    parse_args()
    check_modules()
    get_nic_details()
    do_arg_actions()

if __name__ == "__main__":
    if getpass.getuser() != 'root':
        print('Please run this program as root/with sudo')
        exit(1)
    main()
