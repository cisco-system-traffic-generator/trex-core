#!/router/bin/python-2.7.4

from CPlatform import *
import cmd
import outer_packages
import termstyle
import os
from misc_methods import load_object_config_file
from optparse import OptionParser
from CShowParser import PlatformResponseMissmatch, PlatformResponseAmbiguity

class InteractivePlatform(cmd.Cmd):

    intro = termstyle.green("\nInteractive shell to control a remote Cisco IOS platform.\nType help to view available pre-defined configurations\n(c) All rights reserved.\n")
    prompt = '> '

    def __init__(self, cfg_yaml_path = None, silent_mode = False, virtual_mode = False ):
#       super(InteractivePlatform, self).__init__()
        cmd.Cmd.__init__(self)
        self.virtual_mode   = virtual_mode
        self.platform       = CPlatform(silent_mode)
        if cfg_yaml_path is None:
            try:
                cfg_yaml_path = raw_input(termstyle.cyan("Please enter a readable .yaml configuration file path: "))
                cfg_yaml_path = os.path.abspath(cfg_yaml_path)
            except KeyboardInterrupt:
                exit(-1)
        try:
            self.device_cfg = CDeviceCfg(cfg_yaml_path)
            self.platform.load_platform_data_from_file(self.device_cfg)
            if not virtual_mode:
                # if not virtual mode, try to establish a phyisical connection to platform
                self.platform.launch_connection(self.device_cfg)

        except Exception as inst:
            print(termstyle.magenta(inst))
            exit(-1)
            
    def do_show_cfg (self, line):
        """Outputs the loaded interface configuration"""
        self.platform.get_if_manager().dump_if_config()
        print(termstyle.green("*** End of interface configuration ***"))

    def do_show_nat_cfg (self, line):
        """Outputs the loaded nat provided configuration"""
        try:
            self.platform.dump_obj_config('nat')
            print(termstyle.green("*** End of nat configuration ***"))
        except UserWarning as inst:
            print(termstyle.magenta(inst))


    def do_show_static_route_cfg (self, line):
        """Outputs the loaded static route configuration"""
        try:
            self.platform.dump_obj_config('static_route')
            print(termstyle.green("*** End of static route configuration ***"))
        except UserWarning as inst:
            print(termstyle.magenta(inst))

    def do_switch_cfg (self, cfg_file_path):
        """Switch the current platform interface configuration with another one"""
        if cfg_file_path:
            cfg_yaml_path = os.path.abspath(cfg_file_path)
            self.device_cfg = CDeviceCfg(cfg_yaml_path)
            self.platform.load_platform_data_from_file(self.device_cfg)
            if not self.virtual_mode:
                self.platform.reload_connection(self.device_cfg)
            print(termstyle.green("Configuration switching completed successfully."))
        else:
            print(termstyle.magenta("Configuration file is missing. Please try again."))

    def do_load_clean (self, arg):
        """Loads a clean configuration file onto the platform
        Specify no arguments will load 'clean_config.cfg' file from bootflash disk
        First argument is clean config filename
        Second argument is platform file's disk"""
        if arg:
            in_val = arg.split(' ')
            if len(in_val)==2:
                self.platform.load_clean_config(in_val[0], in_val[1])
            else:
                print(termstyle.magenta("One of the config inputs is missing."))
        else:
            self.platform.load_clean_config()
#           print termstyle.magenta("Configuration file definition is missing. use 'help load_clean' for further info.")

    def do_basic_if_config(self, line):
        """Apply basic interfaces configuartion to all platform interfaces"""
        self.platform.configure_basic_interfaces()
        print(termstyle.green("Basic interfaces configuration applied successfully."))

    def do_basic_if_config_vlan(self, line):
        """Apply basic interfaces configuartion with vlan to all platform interfaces"""
        self.platform.configure_basic_interfaces(vlan = True)
        print(termstyle.green("Basic VLAN interfaces configuration applied successfully."))

    def do_pbr(self, line):
        """Apply IPv4 PBR configuration on all interfaces"""
        self.platform.config_pbr()
        print(termstyle.green("IPv4 PBR configuration applied successfully."))

    def do_pbr_vlan(self, line):
        """Apply IPv4 PBR configuration on all VLAN interfaces"""
        self.platform.config_pbr(vlan = True)
        print(termstyle.green("IPv4 VLAN PBR configuration applied successfully."))

    def do_no_pbr(self, line):
        """Removes IPv4 PBR configuration from all interfaces"""
        self.platform.config_no_pbr()
        print(termstyle.green("IPv4 PBR configuration removed successfully."))

    def do_no_pbr_vlan(self, line):
        """Removes IPv4 PBR configuration from all VLAN interfaces"""
        self.platform.config_no_pbr(vlan = True)
        print(termstyle.green("IPv4 PBR VLAN configuration removed successfully."))

    def do_nbar(self, line):
        """Apply NBAR PD configuration on all interfaces"""
        self.platform.config_nbar_pd()
        print(termstyle.green("NBAR configuration applied successfully."))

    def do_no_nbar(self, line):
        """Removes NBAR PD configuration from all interfaces"""
        self.platform.config_no_nbar_pd()
        print(termstyle.green("NBAR configuration removed successfully."))

    
    def do_dhcp_server (self, line):
        """Apply DHCP server configuration"""
        self.platform.config_dhcp_server('1.1.1.0', '255.255.255.0')
        print(termstyle.green("DHCP configuration applied successfully."))
        
        
    def do_no_dhcp_server (self, line):
        """Apply DHCP server configuration"""
        self.platform.config_no_dhcp_server()
        print(termstyle.green("DHCP configuration removed successfully."))
    
            
    def do_static_route(self, arg):
        """Apply IPv4 static routing configuration on all interfaces
        Specify no arguments will apply static routing with following config:
        1. clients_start            - 16.0.0.1
        2. servers_start            - 48.0.0.1
        3. dual_port_mask           - 1.0.0.0
        4. client_destination_mask  - 255.0.0.0
        5. server_destination_mask  - 255.0.0.0
        """
        if arg:
            stat_route_dict = load_object_config_file(arg)
#           else:
#               print termstyle.magenta("Unknown configutaion option requested. use 'help static_route' for further info.")
        else:
            stat_route_dict = { 'clients_start' : '16.0.0.1',
                                    'servers_start' : '48.0.0.1',
                                    'dual_port_mask': '1.0.0.0',
                                    'client_destination_mask' : '255.0.0.0',
                                    'server_destination_mask' : '255.0.0.0' }
        stat_route_obj = CStaticRouteConfig(stat_route_dict)
        self.platform.config_static_routing(stat_route_obj)
        print(termstyle.green("IPv4 static routing configuration applied successfully."))
#           print termstyle.magenta("Specific configutaion is missing. use 'help static_route' for further info.")

    def do_no_static_route(self, line):
        """Removes IPv4 static route configuration from all non-duplicated interfaces"""
        try:
            self.platform.config_no_static_routing()
            print(termstyle.green("IPv4 static routing configuration removed successfully."))
        except UserWarning as inst:
            print(termstyle.magenta(inst))

    def do_nat(self, arg):
        """Apply NAT configuration on all non-duplicated interfaces
        Specify no arguments will apply NAT with following config:
        1. clients_net_start        - 16.0.0.0
        2. client_acl_wildcard_mask - 0.0.0.255
        3. dual_port_mask           - 1.0.0.0
        4. pool_start               - 200.0.0.0
        5. pool_netmask             - 255.255.255.0
        """
        if arg:
            nat_dict = load_object_config_file(arg)
#           else:
#               print termstyle.magenta("Unknown nat configutaion option requested. use 'help nat' for further info.")
        else:
#           print termstyle.magenta("Specific nat configutaion is missing. use 'help nat' for further info.")
            nat_dict = { 'clients_net_start' : '16.0.0.0',
                             'client_acl_wildcard_mask' : '0.0.0.255',
                             'dual_port_mask' : '1.0.0.0',
                             'pool_start' : '200.0.0.0',
                             'pool_netmask' : '255.255.255.0' }
        nat_obj = CNatConfig(nat_dict)
        self.platform.config_nat(nat_obj)
        print(termstyle.green("NAT configuration applied successfully."))

    def do_no_nat(self, arg):
        """Removes NAT configuration from all non-duplicated interfaces"""
        try: 
            self.platform.config_no_nat()
            print(termstyle.green("NAT configuration removed successfully."))
        except UserWarning as inst:
            print(termstyle.magenta(inst))
            

    def do_ipv6_pbr(self, line):
        """Apply IPv6 PBR configuration on all interfaces"""
        self.platform.config_ipv6_pbr()
        print(termstyle.green("IPv6 PBR configuration applied successfully."))

    def do_ipv6_pbr_vlan(self, line):
        """Apply IPv6 PBR configuration on all vlan interfaces"""
        self.platform.config_ipv6_pbr(vlan = True)
        print(termstyle.green("IPv6 VLAN PBR configuration applied successfully."))

    def do_no_ipv6_pbr(self, line):
        """Removes IPv6 PBR configuration from all interfaces"""
        self.platform.config_no_ipv6_pbr()
        print(termstyle.green("IPv6 PBR configuration removed successfully."))

    def do_no_ipv6_pbr_vlan(self, line):
        """Removes IPv6 PBR configuration from all VLAN interfaces"""
        self.platform.config_no_ipv6_pbr(vlan = True)
        print(termstyle.green("IPv6 VLAN PBR configuration removed successfully."))

    def do_zbf(self, line):
        """Apply Zone-Based policy Firewall configuration on all interfaces"""
        self.platform.config_zbf()
        print(termstyle.green("Zone-Based policy Firewall configuration applied successfully."))

    def do_no_zbf(self, line):
        """Removes Zone-Based policy Firewall configuration from all interfaces"""
        self.platform.config_no_zbf()
        print(termstyle.green("Zone-Based policy Firewall configuration removed successfully."))

    def do_show_cpu_util(self, line):
        """Fetches CPU utilization stats from the platform"""
        try: 
            print(self.platform.get_cpu_util())
            print(termstyle.green("*** End of show_cpu_util output ***"))
        except PlatformResponseMissmatch as inst:
            print(termstyle.magenta(inst))

    def do_show_drop_stats(self, line):
        """Fetches packet drop stats from the platform.\nDrop are summed and presented for both input and output traffic of each interface"""
        print(self.platform.get_drop_stats())
        print(termstyle.green("*** End of show_drop_stats output ***"))

    def do_show_nbar_stats(self, line):
        """Fetches NBAR classification stats from the platform.\nStats are available both as raw data and as percentage data."""
        try:
            print(self.platform.get_nbar_stats())
            print(termstyle.green("*** End of show_nbar_stats output ***"))
        except PlatformResponseMissmatch as inst:
            print(termstyle.magenta(inst))

    def do_show_nat_stats(self, line):
        """Fetches NAT translations stats from the platform"""
        print(self.platform.get_nat_stats())
        print(termstyle.green("*** End of show_nat_stats output ***"))

    def do_show_cft_stats(self, line):
        """Fetches CFT stats from the platform"""
        print(self.platform.get_cft_stats())
        print(termstyle.green("*** End of show_sft_stats output ***"))

    def do_show_cvla_memory_usage(self, line):
        """Fetches CVLA memory usage stats from the platform"""
        (res, res2) = self.platform.get_cvla_memory_usage()
        print(res)
        print(res2)
        print(termstyle.green("*** End of show_cvla_memory_usage output ***"))

    def do_clear_counters(self, line):
        """Clears interfaces counters"""
        self.platform.clear_counters()
        print(termstyle.green("*** clear counters completed ***"))

    def do_clear_nbar_stats(self, line):
        """Clears interfaces counters"""
        self.platform.clear_nbar_stats()
        print(termstyle.green("*** clear nbar stats completed ***"))

    def do_clear_cft_counters(self, line):
        """Clears interfaces counters"""
        self.platform.clear_cft_counters()
        print(termstyle.green("*** clear cft counters completed ***"))

    def do_clear_drop_stats(self, line):
        """Clears interfaces counters"""
        self.platform.clear_packet_drop_stats()
        print(termstyle.green("*** clear packet drop stats completed ***"))

    def do_clear_nat_translations(self, line):
        """Clears nat translations"""
        self.platform.clear_nat_translations()
        print(termstyle.green("*** clear nat translations completed ***"))

    def do_set_tftp_server (self, line):
        """Configures TFTP access on platform"""
        self.platform.config_tftp_server(self.device_cfg)
        print(termstyle.green("*** TFTP config deployment completed ***"))

    def do_show_running_image (self, line):
        """Fetches currently loaded image of the platform"""
        res = self.platform.get_running_image_details()
        print(res)
        print(termstyle.green("*** Show running image completed ***"))

    def do_check_image_existence(self, arg):
        """Check if specific image file (usually *.bin) is already stored in platform drive"""
        if arg:
            try: 
                res = self.platform.check_image_existence(arg.split(' ')[0])
                print(res)
                print(termstyle.green("*** Check image existence completed ***"))
            except PlatformResponseAmbiguity as inst:
                print(termstyle.magenta(inst))
        else:
            print(termstyle.magenta("Please provide an image name in order to check for existance."))

    def do_load_image (self, arg):
        """Loads a given image filename from tftp server (if not available on disk) and sets it as the boot image on the platform"""
        if arg:
            try:
                self.platform.load_platform_image('asr1001-universalk9.BLD_V155_2_S_XE315_THROTTLE_LATEST_20150324_100047-std.bin')#arg.split(' ')[0])
            except UserWarning as inst:
                print(termstyle.magenta(inst))
        else:
            print(termstyle.magenta("Image filename is missing."))

    def do_reload (self, line):
        """Reloads the platform"""

        ans = misc_methods.query_yes_no('This will reload the platform. Are you sure?', default = None)
        if ans: 
            # user confirmed he wishes to reload the platform
            self.platform.reload_platform(self.device_cfg)
            print(termstyle.green("*** Platform reload completed ***"))
        else:
            print(termstyle.green("*** Platform reload aborted ***"))

    def do_quit(self, arg):
        """Quits the application"""
        return True

    def do_exit(self, arg):
        """Quits the application"""
        return True

    def do_all(self, arg):
        """Configures bundle of commands to set PBR routing"""
        self.do_load_clean('')
        self.do_set_tftp_server('')
        self.do_basic_if_config('')
        self.do_pbr('')
        self.do_ipv6_pbr('')

    def do_all_vlan(self, arg):
        """Configures bundle of commands to set PBR routing using on vlan interfaces"""
        self.do_load_clean('')
        self.do_set_tftp_server('')
        self.do_basic_if_config_vlan('')
        self.do_pbr_vlan('')
        self.do_ipv6_pbr_vlan('')



if __name__ == "__main__":
    parser = OptionParser(version="%prog 1.0 \t (C) Cisco Systems Inc.\n")
    parser.add_option("-c", "--config-file", dest="cfg_yaml_path",
                  action="store", help="Define the interface configuration to load the applicatino with.", metavar="FILE_PATH")
    parser.add_option("-s", "--silent", dest="silent_mode", default = False,
                  action="store_true", help="Silence the generated input when commands launched.")
    parser.add_option("-v", "--virtual", dest="virtual_mode", default = False,
                  action="store_true", help="Interact with a virtual router, no actual link will apply. Show commands are NOT available in this mode.")
    (options, args) = parser.parse_args()

    try:
        InteractivePlatform(**vars(options)).cmdloop()

    except KeyboardInterrupt:
            exit(-1)

