#!/router/bin/python
import sys
if sys.version_info >= (3, 0):
    import configparser
else:
    import ConfigParser

import outer_packages
import yaml
from collections import namedtuple
import subprocess, shlex
import os

TRexConfig = namedtuple('TRexConfig', 'trex, router, tftp')

# debug/development purpose, lists object's attributes and their values
def print_r(obj):
    for attr in dir(obj):
        print('obj.%s %s' % (attr, getattr(obj, attr)))

def mix_string (str):
    """Convert all string to lowercase letters, and replaces spaces with '_' char"""
    return str.replace(' ', '_').lower()

# executes given command, returns tuple (return_code, stdout, stderr)
def run_command(cmd, background = False):
    if background:
        print('Running command in background: %s' % cmd)
        with open(os.devnull, 'w') as tempf:
            subprocess.Popen(shlex.split(cmd), stdin=tempf, stdout=tempf, stderr=tempf)
        return (None,)*3
    else:
        print('Running command: %s' % cmd)
        proc = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdout, stderr) = proc.communicate()
        stdout = stdout.decode()
        stderr = stderr.decode()
        if stdout:
            print('Stdout:\n%s' % stdout)
        if proc.returncode:
            if stderr:
                print('Stderr:\n%s' % stderr)
            print('Return code: %s' % proc.returncode)
        return (proc.returncode, stdout, stderr)


def run_remote_command(host, command_string, background = False, timeout = 20):
    cmd = 'ssh -tt %s \'sudo%s sh -ec "%s"\'' % (host, (' timeout %s' % timeout) if (timeout and not background) else '', command_string)
    return run_command(cmd, background)


def generate_intf_lists (interfacesList):
    retDict = {
        'relevant_intf'       : [],
        'relevant_ip_addr'    : [],
        'relevant_mac_addr'   : [],
        'total_pairs'         : None
        }

    for intf in interfacesList:
        retDict['relevant_intf'].append(intf['client'])
        retDict['relevant_ip_addr'].append(intf['client_config']['ip_addr'])
        retDict['relevant_mac_addr'].append(intf['client_config']['mac_addr'])
        retDict['relevant_intf'].append(intf['server'])
        retDict['relevant_ip_addr'].append(intf['server_config']['ip_addr'])
        retDict['relevant_mac_addr'].append(intf['server_config']['mac_addr'])

    retDict['total_pairs'] = len(interfacesList)

    return retDict

def get_single_net_client_addr (ip_addr, octetListDict = {'3' : 1}, ip_type = 'ipv4'):
    """ get_single_net_client_addr(ip_addr, octetListDict, ip_type) -> str

        Parameters
        ----------
        ip_addr : str
            a string an IP address (by default, of type A.B.C.D)
        octetListDict : dict
            a ditionary representing the octets on which to act such that ip[octet_key] = ip[octet_key] + octet_value
        ip_type : str
            a string that defines the ip type to parse. possible inputs are 'ipv4', 'ipv6'

        By default- Returns a new ip address - A.B.C.(D+1)
    """
    if ip_type == 'ipv4':
        ip_lst = ip_addr.split('.')

        for octet,increment in octetListDict.items():
            int_octet = int(octet)
            if ((int_octet < 0) or (int_octet > 3)):
                raise ValueError('the provided octet is not legal in {0} format'.format(ip_type) )
            else:
                if (int(ip_lst[int_octet]) + increment) < 255:
                    ip_lst[int_octet] = str(int(ip_lst[int_octet]) + increment)
                else:
                    raise ValueError('the requested increment exceeds 255 client address limit')

        return '.'.join(ip_lst)

    else: # this is a ipv6 address, handle accordingly
        ip_lst = ip_addr.split(':')

        for octet,increment in octetListDict.items():
            int_octet = int(octet)
            if ((int_octet < 0) or (int_octet > 7)):
                raise ValueError('the provided octet is not legal in {0} format'.format(ip_type) )
            else:
                if (int(ip_lst[int_octet]) + increment) < 65535:
                    ip_lst[int_octet] = format( int(ip_lst[int_octet], 16) + increment, 'X')
                else:
                    raise ValueError('the requested increment exceeds 65535 client address limit')

        return ':'.join(ip_lst)


def load_complete_config_file (filepath):
    """load_complete_config_file(filepath) -> list

    Loads a configuration file (.yaml) for both trex config and router config
    Returns a list with a dictionary to each of the configurations
    """

    # create response dictionaries
    trex_config = {}     
    rtr_config  = {}
    tftp_config = {}

    try:
        with open(filepath, 'r') as f:
            config = yaml.safe_load(f)

            # Handle TRex configuration
            trex_config['trex_name']         = config["trex"]["hostname"]
            trex_config['trex_cores']        = int(config["trex"]["cores"])
            trex_config['modes']          = config['trex'].get('modes', [])
            for key, val in config['trex'].items():
                trex_config[key] = val

            if 'loopback' not in trex_config['modes']:
                trex_config['router_interface']  = config["router"]["ip_address"]

                # Handle Router configuration
                rtr_config['model']              = config["router"]["model"]
                rtr_config['hostname']           = config["router"]["hostname"]
                rtr_config['ip_address']         = config["router"]["ip_address"]
                rtr_config['image']              = config["router"]["image"]
                rtr_config['line_pswd']          = config["router"]["line_password"]
                rtr_config['en_pswd']            = config["router"]["en_password"]
                rtr_config['interfaces']         = config["router"]["interfaces"]
                rtr_config['clean_config']       = config["router"]["clean_config"]
                rtr_config['intf_masking']       = config["router"]["intf_masking"]
                rtr_config['ipv6_mask']          = config["router"]["ipv6_mask"]
                rtr_config['mgmt_interface']     = config["router"]["mgmt_interface"]

                # Handle TFTP configuration
                tftp_config['hostname']          = config["tftp"]["hostname"]
                tftp_config['ip_address']        = config["tftp"]["ip_address"]
                tftp_config['images_path']       = config["tftp"]["images_path"]

                if rtr_config['clean_config'] is None:
                    raise ValueError('A clean router configuration wasn`t provided.')

    except ValueError:
        print("")
        raise

    except Exception as inst:
        print("\nBad configuration file provided: '{0}'\n".format(filepath))
        raise inst

    return TRexConfig(trex_config, rtr_config, tftp_config)

def load_object_config_file (filepath):
    try:
        with open(filepath, 'r') as f:
            config = yaml.safe_load(f)
            return config
    except Exception as inst:
        print("\nBad configuration file provided: '{0}'\n".format(filepath))
        print(inst)
        exit(-1)

            
def query_yes_no(question, default="yes"):
    """Ask a yes/no question via raw_input() and return their answer.

    "question" is a string that is presented to the user.
    "default" is the presumed answer if the user just hits <Enter>.
        It must be "yes" (the default), "no" or None (meaning
        an answer is required of the user).

    The "answer" return value is True for "yes" or False for "no".
    """
    valid = { "yes": True, "y": True, "ye": True,
              "no": False, "n": False }
    if default is None:
        prompt = " [y/n] "
    elif default == "yes":
        prompt = " [Y/n] "
    elif default == "no":
        prompt = " [y/N] "
    else:
        raise ValueError("invalid default answer: '%s'" % default)

    while True:
        sys.stdout.write(question + prompt)
        choice = input().lower()
        if default is not None and choice == '':
            return valid[default]
        elif choice in valid:
            return valid[choice]
        else:
            sys.stdout.write("Please respond with 'yes' or 'no' "
                             "(or 'y' or 'n').\n")


def load_benchmark_config_file (filepath):
    """load_benchmark_config_file(filepath) -> list

    Loads a configuration file (.yaml) for both trex config and router config
    Returns a list with a dictionary to each of the configurations
    """

    # create response dictionary
    benchmark_config = {}

    try:
        with open(filepath) as f:
            benchmark_config = yaml.safe_load(f)

    except Exception as inst:
        print("\nBad configuration file provided: '{0}'\n".format(filepath))
        print(inst)
        exit(-1)

    return benchmark_config


def get_benchmark_param (benchmark_path, test_name, param, sub_param = None):

    config = load_benchmark_config_file(benchmark_path)
    if sub_param is None:
        return config[test_name][param]
    else:
        return config[test_name][param][sub_param]

def gen_increment_dict (dual_port_mask):
    addr_lst    = dual_port_mask.split('.')
    result      = {}
    for idx, octet_increment in enumerate(addr_lst):
        octet_int = int(octet_increment)
        if octet_int>0:
            result[str(idx)] = octet_int

    return result


def get_network_addr (ip_type = 'ipv4'):
    ipv4_addr = [1, 1, 1, 0]  # base ipv4 address to start generating from- 1.1.1.0
    ipv6_addr = ['2001', 'DB8', 0, '2222', 0, 0, 0, 0]  # base ipv6 address to start generating from- 2001:DB8:1111:2222:0:0
    while True:
        if ip_type == 'ipv4':
            if (ipv4_addr[2] < 255):
                yield [".".join( map(str, ipv4_addr) ), '255.255.255.0']
                ipv4_addr[2] += 1
            else:   # reached defined maximum limit of address allocation
                return
        else:   # handling ipv6 addressing
            if (ipv6_addr[2] < 4369):
                tmp_ipv6_addr = list(ipv6_addr)
                tmp_ipv6_addr[2] = hex(tmp_ipv6_addr[2])[2:]
                yield ":".join( map(str, tmp_ipv6_addr) )
                ipv6_addr[2] += 1
            else:   # reached defined maximum limit of address allocation
                return




if __name__ == "__main__":
    pass
