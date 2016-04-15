#!/router/bin/python

from interfaces_e import IFType
import CustomLogger
import misc_methods
import telnetlib
import socket
import time
from collections import OrderedDict

class CCommandCache(object):
    def __init__(self):
        self.__gen_clean_data_structure()

    def __gen_clean_data_structure (self):
        self.cache =  {"IF"   : OrderedDict(),
                       "CONF" : [],
                       "EXEC" : []}

    def __list_append (self, dest_list, cmd):
        if isinstance(cmd, list):
            dest_list.extend( cmd )
        else:
            dest_list.append( cmd )

    def add (self, cmd_type, cmd, interface = None):

        if interface is not None: # this is an interface ("IF") config command
            if interface in self.cache['IF']:
                # interface commands already exists
                self.__list_append(self.cache['IF'][interface], cmd)
            else:
                # no chached commands for this interface
                self.cache['IF'][interface] = []
                self.__list_append(self.cache['IF'][interface], cmd)
        else:                 # this is either a CONF or EXEC command
            self.__list_append(self.cache[cmd_type.upper()], cmd)

    def dump_config (self):
        # dump IF config:
        print("configure terminal")
        for intf, intf_cmd_list in self.cache['IF'].items():
            print("interface {if_name}".format( if_name = intf ))
            print('\n'.join(intf_cmd_list))

        if self.cache['IF']:
            # add 'exit' note only if if config actually took place
            print('exit')    # exit to global config mode

        # dump global config
        if self.cache['CONF']:
            print('\n'.join(self.cache['CONF']))

        # exit back to en mode
        print("exit")

        # dump exec config
        if self.cache['EXEC']:
            print('\n'.join(self.cache['EXEC']))

    def get_config_list (self):
        conf_list = []

        conf_list.append("configure terminal")
        for intf, intf_cmd_list in self.cache['IF'].items():
            conf_list.append( "interface {if_name}".format( if_name = intf ) )
            conf_list.extend( intf_cmd_list )
        if len(conf_list)>1:
            # add 'exit' note only if if config actually took place
            conf_list.append("exit") 

        conf_list.extend( self.cache['CONF'] )
        conf_list.append("exit")
        conf_list.extend( self.cache['EXEC'] )
        

        return conf_list

    def clear_cache (self):
        # clear all pointers to cache data (erase the data structure)
        self.cache.clear()  
        # Re-initialize the cache
        self.__gen_clean_data_structure()

    pass


class CCommandLink(object):
    def __init__(self, silent_mode = False):
        self.history        = []
        self.virtual_mode   = True
        self.silent_mode    = silent_mode
        self.telnet_con     = None


    def __transmit (self, cmd_list, **kwargs):
        self.history.extend(cmd_list)
        if not self.silent_mode:
            print('\n'.join(cmd_list))   # prompting the pushed platform commands
        if not self.virtual_mode:
            # transmit the command to platform. 
            return self.telnet_con.write_ios_cmd(cmd_list, **kwargs)

    def run_command (self, cmd_list, **kwargs):
        response = ''
        for cmd in cmd_list:
            
            # check which type of cmd we handle
            if isinstance(cmd, CCommandCache):
                tmp_response = self.__transmit( cmd.get_config_list(), **kwargs )   # join the commands with new-line delimiter
            else:
                tmp_response = self.__transmit([cmd], **kwargs)
            if not self.virtual_mode:
                response += tmp_response
        return response

    def run_single_command (self, cmd, **kwargs):
        return self.run_command([cmd], **kwargs)

    def get_history (self, as_string = False):
        if as_string:
            return '\n'.join(self.history)
        else:
            return self.history

    def clear_history (self):
        # clear all pointers to history data (erase the data structure)
        del self.history[:]
        # Re-initialize the histoyr with clear one
        self.history = []

    def launch_platform_connectivity (self, device_config_obj):
        connection_info = device_config_obj.get_platform_connection_data()
        self.telnet_con     = CIosTelnet( **connection_info )
        self.virtual_mode   = False # if physical connectivity was successful, toggle virtual mode off

    def close_platform_connection(self):
        if self.telnet_con is not None:
            self.telnet_con.close()



class CDeviceCfg(object):
    def __init__(self, cfg_yaml_path = None):
        if cfg_yaml_path is not None:
            (self.platform_cfg, self.tftp_cfg) = misc_methods.load_complete_config_file(cfg_yaml_path)[1:3]
            
            self.interfaces_cfg = self.platform_cfg['interfaces'] # extract only the router interface configuration

    def set_platform_config(self, config_dict):
        self.platform_cfg = config_dict
        self.interfaces_cfg = self.platform_cfg['interfaces']

    def set_tftp_config(self, tftp_cfg):
        self.tftp_cfg = tftp_cfg

    def get_interfaces_cfg (self):
        return self.interfaces_cfg

    def get_ip_address (self):
        return self.__get_attr('ip_address')

    def get_line_password (self):
        return self.__get_attr('line_pswd')

    def get_en_password (self):
        return self.__get_attr('en_pswd')

    def get_mgmt_interface (self):
        return self.__get_attr('mgmt_interface')

    def get_platform_connection_data (self):
        return { 'host' : self.get_ip_address(), 'line_pass' : self.get_line_password(), 'en_pass' : self.get_en_password() }

    def get_tftp_info (self):
        return self.tftp_cfg

    def get_image_name (self):
        return self.__get_attr('image')

    def __get_attr (self, attr):
        return self.platform_cfg[attr]

    def dump_config (self):
        import yaml
        print(yaml.dump(self.interfaces_cfg, default_flow_style=False))

class CIfObj(object):
    _obj_id = 0

    def __init__(self, if_name, ipv4_addr, ipv6_addr, src_mac_addr, dest_mac_addr, if_type):
        self.__get_and_increment_id()
        self.if_name        = if_name
        self.if_type        = if_type
        self.src_mac_addr   = src_mac_addr
        self.dest_mac_addr  = dest_mac_addr
        self.ipv4_addr      = ipv4_addr 
        self.ipv6_addr      = ipv6_addr 
        self.pair_parent    = None     # a pointer to CDualIfObj which holds this interface and its pair-complement

    def __get_and_increment_id (self):
        self._obj_id = CIfObj._obj_id
        CIfObj._obj_id += 1

    def get_name (self):
        return self.if_name

    def get_src_mac_addr (self):
        return self.src_mac_addr

    def get_dest_mac (self):
        return self.dest_mac_addr

    def get_id (self):
        return self._obj_id

    def get_if_type (self):
        return self.if_type

    def get_ipv4_addr (self):
        return self.ipv4_addr

    def get_ipv6_addr (self):
        return self.ipv6_addr

    def set_ipv4_addr (self, addr):
        self.ipv4_addr = addr

    def set_ipv6_addr (self, addr):
        self.ipv6_addr = addr

    def set_pair_parent (self, dual_if_obj):
        self.pair_parent = dual_if_obj

    def get_pair_parent (self):
        return self.pair_parent

    def is_client (self):
        return (self.if_type == IFType.Client)

    def is_server (self):
        return (self.if_type == IFType.Server)

    pass


class CDualIfObj(object):
    _obj_id = 0

    def __init__(self, vrf_name, client_if_obj, server_if_obj):
        self.__get_and_increment_id()
        self.vrf_name       = vrf_name
        self.client_if      = client_if_obj
        self.server_if      = server_if_obj

        # link if_objects to its parent dual_if
        self.client_if.set_pair_parent(self)    
        self.server_if.set_pair_parent(self)
        pass

    def __get_and_increment_id (self):
        self._obj_id = CDualIfObj._obj_id
        CDualIfObj._obj_id += 1

    def get_id (self):
        return self._obj_id

    def get_vrf_name (self):
        return self.vrf_name

    def is_duplicated (self):
        return self.vrf_name != None

class CIfManager(object):
    _ipv4_gen = misc_methods.get_network_addr()
    _ipv6_gen = misc_methods.get_network_addr(ip_type = 'ipv6')

    def __init__(self):
        self.interfarces     = OrderedDict()
        self.dual_intf       = []
        self.full_device_cfg = None

    def __add_if_to_manager (self, if_obj):
        self.interfarces[if_obj.get_name()] = if_obj

    def __add_dual_if_to_manager (self, dual_if_obj):
        self.dual_intf.append(dual_if_obj)

    def __get_ipv4_net_client_addr(self, ipv4_addr):
        return misc_methods.get_single_net_client_addr (ipv4_addr)

    def __get_ipv6_net_client_addr(self, ipv6_addr):
        return misc_methods.get_single_net_client_addr (ipv6_addr, {'7' : 1}, ip_type = 'ipv6')

    def load_config (self, device_config_obj):
        self.full_device_cfg = device_config_obj
        # first, erase all current config
        self.interfarces.clear()
        del self.dual_intf[:]

        # than, load the configuration
        intf_config = device_config_obj.get_interfaces_cfg()

        # finally, parse the information into data-structures
        for intf_pair in intf_config:
            # generate network addresses for client side, and initialize client if object
            tmp_ipv4_addr = self.__get_ipv4_net_client_addr (next(CIfManager._ipv4_gen)[0])
            tmp_ipv6_addr = self.__get_ipv6_net_client_addr (next(CIfManager._ipv6_gen))

            client_obj = CIfObj(if_name = intf_pair['client']['name'],
                ipv4_addr = tmp_ipv4_addr,
                ipv6_addr = tmp_ipv6_addr,
                src_mac_addr  = intf_pair['client']['src_mac_addr'],
                dest_mac_addr = intf_pair['client']['dest_mac_addr'],
                if_type   = IFType.Client)

            # generate network addresses for server side, and initialize server if object
            tmp_ipv4_addr = self.__get_ipv4_net_client_addr (next(CIfManager._ipv4_gen)[0])
            tmp_ipv6_addr = self.__get_ipv6_net_client_addr (next(CIfManager._ipv6_gen))
            
            server_obj = CIfObj(if_name = intf_pair['server']['name'],
                ipv4_addr = tmp_ipv4_addr,
                ipv6_addr = tmp_ipv6_addr,
                src_mac_addr  = intf_pair['server']['src_mac_addr'],
                dest_mac_addr = intf_pair['server']['dest_mac_addr'],
                if_type   = IFType.Server)

            dual_intf_obj = CDualIfObj(vrf_name = intf_pair['vrf_name'],
                client_if_obj = client_obj,
                server_if_obj = server_obj)

            # update single interfaces pointers
            client_obj.set_pair_parent(dual_intf_obj)
            server_obj.set_pair_parent(dual_intf_obj)

            # finally, update the data-structures with generated objects
            self.__add_if_to_manager(client_obj)
            self.__add_if_to_manager(server_obj)
            self.__add_dual_if_to_manager(dual_intf_obj)


    def get_if_list (self, if_type = IFType.All, is_duplicated = None):
        result = []
        for if_name,if_obj in self.interfarces.items():
            if (if_type == IFType.All) or ( if_obj.get_if_type() == if_type) :
                if (is_duplicated is None) or (if_obj.get_pair_parent().is_duplicated() == is_duplicated):
                    # append this if_obj only if matches both IFType and is_duplicated conditions
                    result.append(if_obj)
        return result

    def get_duplicated_if (self):
        result = []
        for dual_if_obj in self.dual_intf:
            if dual_if_obj.get_vrf_name() is not None :
                result.extend( (dual_if_obj.client_if, dual_if_obj.server_if) )
        return result

    def get_dual_if_list (self, is_duplicated = None):
        result = []
        for dual_if in self.dual_intf:
            if (is_duplicated is None) or (dual_if.is_duplicated() == is_duplicated):
                result.append(dual_if)
        return result

    def dump_if_config (self):
        if self.full_device_cfg is None:
            print("Device configuration isn't loaded.\nPlease load config and try again.")
        else:
            self.full_device_cfg.dump_config()


class AuthError(Exception): 
    pass

class CIosTelnet(telnetlib.Telnet):
    AuthError = AuthError

    # wrapper for compatibility with Python2/3, convert input to bytes
    def str_to_bytes_wrapper(self, func, text, *args, **kwargs):
        if type(text) in (list, tuple):
            text = [elem.encode('ascii') if type(elem) is str else elem for elem in text]
        res = func(self, text.encode('ascii') if type(text) is str else text, *args, **kwargs)
        return res.decode() if type(res) is bytes else res

    def read_until(self, text, *args, **kwargs):
        return self.str_to_bytes_wrapper(telnetlib.Telnet.read_until, text, *args, **kwargs)

    def write(self, text, *args, **kwargs):
        return self.str_to_bytes_wrapper(telnetlib.Telnet.write, text, *args, **kwargs)

    def expect(self, text, *args, **kwargs):
        return self.str_to_bytes_wrapper(telnetlib.Telnet.expect, text, *args, **kwargs)

    def __init__ (self, host, line_pass, en_pass, port = 23, str_wait = "#"):
        telnetlib.Telnet.__init__(self)
        self.host           = host
        self.port           = port
        self.line_passwd    = line_pass
        self.enable_passwd  = en_pass
        self.pr             = str_wait
#       self.set_debuglevel (1)
        try:
            self.open(self.host,self.port, timeout = 5)
            self.read_until("word:",1)
            self.write("{line_pass}\n".format(line_pass = self.line_passwd) )
            res = self.read_until(">",1)
            if 'Password' in res:
                raise AuthError('Invalid line password was provided')
            self.write("enable 15\n")
            self.read_until("d:",1)
            self.write("{en_pass}\n".format(en_pass = self.enable_passwd) )
            res = self.read_until(self.pr,1)
            if 'Password' in res:
                raise AuthError('Invalid en password was provided')
            self.write_ios_cmd(['terminal length 0'])

        except socket.timeout:
            raise socket.timeout('A timeout error has occured.\nCheck platform connectivity or the hostname defined in the config file')
        except Exception as inst:
            raise

    def write_ios_cmd (self, cmd_list, result_from = 0, timeout = 3, **kwargs):
        assert (isinstance (cmd_list, list) == True)
        self.read_until(self.pr, timeout = 1)

        res = ''
        if 'read_until' in kwargs:
            wf = kwargs['read_until']
        else:
            wf = self.pr

        for idx, cmd in enumerate(cmd_list):
            self.write(cmd+'\r\n')
            if idx < result_from:
                # don't care for return string
                if type(wf) is list:
                    self.expect(wf, timeout)[2]
                else:
                    self.read_until(wf, timeout)
            else:
                # care for return string
                if type(wf) is list:
                    res += self.expect(wf, timeout)[2]
                else:
                    res += self.read_until(wf, timeout)
#       return res.split('\r\n')
        return res  # return the received response as a string, each line is seperated by '\r\n'.


if __name__ == "__main__":
#   dev_cfg = CDeviceCfg('config/config.yaml')
#   print dev_cfg.get_platform_connection_data()
#   telnet = CIosTelnet( **(dev_cfg.get_platform_connection_data() ) )

#   if_mng  = CIfManager()
#   if_mng.load_config(dev_cfg)
#   if_mng.dump_config()
    pass
