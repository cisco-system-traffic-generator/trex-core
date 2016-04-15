#!/router/bin/python

from interfaces_e import IFType
from platform_cmd_link import *
import CustomLogger
import misc_methods
import re
import time
import CProgressDisp
from CShowParser import CShowParser

class CPlatform(object):
    def __init__(self, silent_mode):
        self.if_mngr            = CIfManager()
        self.cmd_link           = CCommandLink(silent_mode)
        self.nat_config         = None
        self.stat_route_config  = None
        self.running_image      = None
        self.needed_image_path  = None
        self.tftp_cfg           = None
        self.config_history     = { 'basic_if_config' : False, 'tftp_server_config' : False }

    def configure_basic_interfaces(self, mtu = 4000):

        cache = CCommandCache()
        for dual_if in self.if_mngr.get_dual_if_list():
            client_if_command_set   = []
            server_if_command_set   = []

            client_if_command_set.append ('mac-address {mac}'.format( mac = dual_if.client_if.get_src_mac_addr()) )
            client_if_command_set.append ('mtu %s' % mtu)
            client_if_command_set.append ('ip address {ip} 255.255.255.0'.format( ip = dual_if.client_if.get_ipv4_addr() ))
            client_if_command_set.append ('ipv6 address {ip}/64'.format( ip = dual_if.client_if.get_ipv6_addr() ))

            cache.add('IF', client_if_command_set, dual_if.client_if.get_name())

            server_if_command_set.append ('mac-address {mac}'.format( mac = dual_if.server_if.get_src_mac_addr()) )
            server_if_command_set.append ('mtu %s' % mtu)
            server_if_command_set.append ('ip address {ip} 255.255.255.0'.format( ip = dual_if.server_if.get_ipv4_addr() ))
            server_if_command_set.append ('ipv6 address {ip}/64'.format( ip = dual_if.server_if.get_ipv6_addr() ))

            cache.add('IF', server_if_command_set, dual_if.server_if.get_name())

        self.cmd_link.run_single_command(cache)
        self.config_history['basic_if_config'] = True



    def configure_basic_filtered_interfaces(self, intf_list, mtu = 4000):

        cache = CCommandCache()
        for intf in intf_list:
            if_command_set   = []

            if_command_set.append ('mac-address {mac}'.format( mac = intf.get_src_mac_addr()) )
            if_command_set.append ('mtu %s' % mtu)
            if_command_set.append ('ip address {ip} 255.255.255.0'.format( ip = intf.get_ipv4_addr() ))
            if_command_set.append ('ipv6 address {ip}/64'.format( ip = intf.get_ipv6_addr() ))

            cache.add('IF', if_command_set, intf.get_name())

        self.cmd_link.run_single_command(cache)


    def load_clean_config (self, config_filename = "clean_config.cfg", cfg_drive = "bootflash"):
        self.clear_nat_translations()

        cache = CCommandCache()
        cache.add('EXEC', "configure replace {drive}:{file} force".format(drive = cfg_drive, file = config_filename))
        res = self.cmd_link.run_single_command(cache)
        if 'Rollback Done' not in res:
            raise UserWarning('Could not load clean config, please verify file exists. Response:\n%s' % res)

    def config_pbr (self, mode = 'config'):
        idx = 1
        unconfig_str = '' if mode=='config' else 'no '

        cache = CCommandCache()
        pre_commit_cache = CCommandCache()
        pre_commit_set = set([])

        for dual_if in self.if_mngr.get_dual_if_list():
            client_if_command_set   = []
            server_if_command_set   = []
            conf_t_command_set      = []
            client_net_next_hop = misc_methods.get_single_net_client_addr(dual_if.server_if.get_ipv4_addr() )
            server_net_next_hop = misc_methods.get_single_net_client_addr(dual_if.client_if.get_ipv4_addr() )

            if dual_if.is_duplicated():
                # define the relevant VRF name
                pre_commit_set.add('{mode}ip vrf {dup}'.format( mode = unconfig_str, dup = dual_if.get_vrf_name()) )
                
                # assign VRF to interfaces, config interfaces with relevant route-map
                client_if_command_set.append ('{mode}ip vrf forwarding {dup}'.format( mode = unconfig_str, dup = dual_if.get_vrf_name()) )
                client_if_command_set.append ('{mode}ip policy route-map {dup}_{p1}_to_{p2}'.format( 
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
                server_if_command_set.append ('{mode}ip vrf forwarding {dup}'.format( mode = unconfig_str, dup = dual_if.get_vrf_name()) )
                server_if_command_set.append ('{mode}ip policy route-map {dup}_{p2}_to_{p1}'.format( 
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )

                # config route-map routing
                conf_t_command_set.append('{mode}route-map {dup}_{p1}_to_{p2} permit 10'.format(
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
                if mode == 'config':
                    conf_t_command_set.append('set ip next-hop {next_hop}'.format(
                         next_hop = client_net_next_hop) )
                conf_t_command_set.append('{mode}route-map {dup}_{p2}_to_{p1} permit 10'.format(
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
                if mode == 'config':
                    conf_t_command_set.append('set ip next-hop {next_hop}'.format(
                         next_hop = server_net_next_hop) )

                # config global arp to interfaces net address and vrf
                conf_t_command_set.append('{mode}arp vrf {dup} {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    next_hop = server_net_next_hop, 
                    dest_mac = dual_if.client_if.get_dest_mac()))
                conf_t_command_set.append('{mode}arp vrf {dup} {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str, 
                    dup = dual_if.get_vrf_name(), 
                    next_hop = client_net_next_hop, 
                    dest_mac = dual_if.server_if.get_dest_mac()))
            else:
                # config interfaces with relevant route-map
                client_if_command_set.append ('{mode}ip policy route-map {p1}_to_{p2}'.format( 
                    mode = unconfig_str,
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
                server_if_command_set.append ('{mode}ip policy route-map {p2}_to_{p1}'.format( 
                    mode = unconfig_str,
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )

                # config route-map routing
                conf_t_command_set.append('{mode}route-map {p1}_to_{p2} permit 10'.format(
                    mode = unconfig_str,
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
                if mode == 'config':
                    conf_t_command_set.append('set ip next-hop {next_hop}'.format(
                         next_hop = client_net_next_hop) )
                conf_t_command_set.append('{mode}route-map {p2}_to_{p1} permit 10'.format(
                    mode = unconfig_str,
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
                if mode == 'config':
                    conf_t_command_set.append('set ip next-hop {next_hop}'.format(
                         next_hop = server_net_next_hop) )

                # config global arp to interfaces net address
                conf_t_command_set.append('{mode}arp {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str,
                    next_hop = server_net_next_hop, 
                    dest_mac = dual_if.client_if.get_dest_mac()))
                conf_t_command_set.append('{mode}arp {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str,
                    next_hop = client_net_next_hop, 
                    dest_mac = dual_if.server_if.get_dest_mac()))

            # assign generated config list to cache
            cache.add('IF', server_if_command_set, dual_if.server_if.get_name())
            cache.add('IF', client_if_command_set, dual_if.client_if.get_name())
            cache.add('CONF', conf_t_command_set)
            idx += 2

        # finish handling pre-config cache
        pre_commit_set = list(pre_commit_set)
#       pre_commit_set.append('exit')
        pre_commit_cache.add('CONF', pre_commit_set )
        # deploy the configs (order is important!)
        self.cmd_link.run_command( [pre_commit_cache, cache] )
        if self.config_history['basic_if_config']:
            # in this case, duplicated interfaces will lose its ip address. 
            # re-config IPv4 addresses
            self.configure_basic_filtered_interfaces(self.if_mngr.get_duplicated_if() )

    def config_no_pbr (self):
        self.config_pbr(mode = 'unconfig')

    def config_static_routing (self, stat_route_obj, mode = 'config'):

        if mode == 'config':
            self.stat_route_config = stat_route_obj   # save the latest static route config for future removal purposes

        unconfig_str = '' if mode=='config' else 'no '
        cache              = CCommandCache()
        pre_commit_cache   = CCommandCache()
        pre_commit_set     = set([])
        current_dup_intf   = None
        # client_net       = None
        # server_net       = None
        client_net         = stat_route_obj.client_net_start
        server_net         = stat_route_obj.server_net_start
        conf_t_command_set = []

        for dual_if in self.if_mngr.get_dual_if_list():

            # handle duplicated addressing generation
            if dual_if.is_duplicated():
                if dual_if.get_vrf_name() != current_dup_intf:
                    # if this is a dual interfaces, and it is different from the one we proccessed so far, reset static route addressing
                    current_dup_intf = dual_if.get_vrf_name()
                    client_net       = stat_route_obj.client_net_start
                    server_net       = stat_route_obj.server_net_start
            else:
                if current_dup_intf is not None:
                    current_dup_intf = None
                    client_net       = stat_route_obj.client_net_start
                    server_net       = stat_route_obj.server_net_start

            client_net_next_hop = misc_methods.get_single_net_client_addr(dual_if.server_if.get_ipv4_addr() )
            server_net_next_hop = misc_methods.get_single_net_client_addr(dual_if.client_if.get_ipv4_addr() )

            # handle static route configuration for the interfaces
            if dual_if.is_duplicated():
                client_if_command_set   = []
                server_if_command_set   = []

                # define the relevant VRF name
                pre_commit_set.add('{mode}ip vrf {dup}'.format( mode = unconfig_str, dup = dual_if.get_vrf_name()) )
                
                # assign VRF to interfaces, config interfaces with relevant route-map
                client_if_command_set.append ('{mode}ip vrf forwarding {dup}'.format( mode = unconfig_str, dup = dual_if.get_vrf_name()) )
                server_if_command_set.append ('{mode}ip vrf forwarding {dup}'.format( mode = unconfig_str, dup = dual_if.get_vrf_name()) )

                conf_t_command_set.append( "{mode}ip route vrf {dup} {next_net} {dest_mask} {next_hop}".format(
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    next_net = client_net,
                    dest_mask = stat_route_obj.client_mask,
                    next_hop = client_net_next_hop))
                conf_t_command_set.append( "{mode}ip route vrf {dup} {next_net} {dest_mask} {next_hop}".format(
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    next_net = server_net,
                    dest_mask = stat_route_obj.server_mask,
                    next_hop = server_net_next_hop))

                # config global arp to interfaces net address and vrf
                conf_t_command_set.append('{mode}arp vrf {dup} {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str,
                    dup = dual_if.get_vrf_name(), 
                    next_hop = server_net_next_hop, 
                    dest_mac = dual_if.client_if.get_dest_mac()))
                conf_t_command_set.append('{mode}arp vrf {dup} {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str, 
                    dup = dual_if.get_vrf_name(), 
                    next_hop = client_net_next_hop, 
                    dest_mac = dual_if.server_if.get_dest_mac()))

                # assign generated interfaces config list to cache
                cache.add('IF', server_if_command_set, dual_if.server_if.get_name())
                cache.add('IF', client_if_command_set, dual_if.client_if.get_name())

            else:
                conf_t_command_set.append( "{mode}ip route {next_net} {dest_mask} {next_hop}".format(
                    mode = unconfig_str,
                    next_net = client_net,
                    dest_mask = stat_route_obj.client_mask,
                    next_hop = server_net_next_hop))
                conf_t_command_set.append( "{mode}ip route {next_net} {dest_mask} {next_hop}".format(
                    mode = unconfig_str,
                    next_net = server_net,
                    dest_mask = stat_route_obj.server_mask,
                    next_hop = client_net_next_hop))

                # config global arp to interfaces net address
                conf_t_command_set.append('{mode}arp {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str,
                    next_hop = server_net_next_hop, 
                    dest_mac = dual_if.client_if.get_dest_mac()))
                conf_t_command_set.append('{mode}arp {next_hop} {dest_mac} arpa'.format(
                    mode = unconfig_str,
                    next_hop = client_net_next_hop, 
                    dest_mac = dual_if.server_if.get_dest_mac()))

            # bump up to the next client network address
            client_net = misc_methods.get_single_net_client_addr(client_net, stat_route_obj.net_increment)
            server_net = misc_methods.get_single_net_client_addr(server_net, stat_route_obj.net_increment)


        # finish handling pre-config cache
        pre_commit_set = list(pre_commit_set)
#       pre_commit_set.append('exit')
        pre_commit_cache.add('CONF', pre_commit_set )
        # assign generated config list to cache
        cache.add('CONF', conf_t_command_set)
        # deploy the configs (order is important!)
        self.cmd_link.run_command( [pre_commit_cache, cache] )
        if self.config_history['basic_if_config']:
            # in this case, duplicated interfaces will lose its ip address. 
            # re-config IPv4 addresses
            self.configure_basic_filtered_interfaces(self.if_mngr.get_duplicated_if() )


    def config_no_static_routing (self, stat_route_obj = None):

        if stat_route_obj is None and self.stat_route_config is not None:
            self.config_static_routing(self.stat_route_config, mode = 'unconfig')
            self.stat_route_config = None  # reverse current static route config back to None (no nat config is known to run).
        elif stat_route_obj is not None:
            self.config_static_routing(stat_route_obj, mode = 'unconfig')
        else:
            raise UserWarning('No static route configuration is available for removal.')

    def config_nbar_pd (self, mode = 'config'):
        unconfig_str = '' if mode=='config' else 'no '
        cache = CCommandCache()

        for intf in self.if_mngr.get_if_list(if_type = IFType.Client):
            cache.add('IF', "{mode}ip nbar protocol-discovery".format( mode = unconfig_str ), intf.get_name())

        self.cmd_link.run_single_command( cache )

    def config_no_nbar_pd (self):
        self.config_nbar_pd (mode = 'unconfig')


    def config_nat_verify (self, mode = 'config'):

        # toggle all duplicate interfaces
        # dup_ifs = self.if_mngr.get_duplicated_if()
        if mode=='config':
            self.toggle_duplicated_intf(action = 'down')
            # self.__toggle_interfaces(dup_ifs, action = 'down' )
        else:
            # if we're in 'unconfig', toggle duplicated interfaces back up
            self.toggle_duplicated_intf(action = 'up')
            # self.__toggle_interfaces(dup_ifs)

    def config_no_nat_verify (self):
        self.config_nat_verify(mode = 'unconfig')

    def config_nat (self, nat_obj, mode = 'config'):

        if mode == 'config':
            self.nat_config = nat_obj   # save the latest nat config for future removal purposes

        cache               = CCommandCache()
        conf_t_command_set  = []
        client_net          = nat_obj.clients_net_start
        pool_net            = nat_obj.nat_pool_start
        unconfig_str        = '' if mode=='config' else 'no '

        # toggle all duplicate interfaces
        # dup_ifs = self.if_mngr.get_duplicated_if()
        if mode=='config':
            self.toggle_duplicated_intf(action = 'down')
            # self.__toggle_interfaces(dup_ifs, action = 'down' )
        else:
            # if we're in 'unconfig', toggle duplicated interfaces back up
            self.toggle_duplicated_intf(action = 'up')
            # self.__toggle_interfaces(dup_ifs)

        for dual_if in self.if_mngr.get_dual_if_list(is_duplicated = False):
            cache.add('IF', "{mode}ip nat inside".format( mode = unconfig_str ), dual_if.client_if.get_name())
            cache.add('IF', "{mode}ip nat outside".format( mode = unconfig_str ), dual_if.server_if.get_name())
            pool_id = dual_if.get_id() + 1

            conf_t_command_set.append("{mode}ip nat pool pool{pool_num} {start_addr} {end_addr} netmask {mask}".format(
                mode = unconfig_str,
                pool_num = pool_id,
                start_addr = pool_net,
                end_addr = CNatConfig.calc_pool_end(pool_net, nat_obj.nat_netmask),
                mask = nat_obj.nat_netmask))

            conf_t_command_set.append("{mode}ip nat inside source list {num} pool pool{pool_num} overload".format(
                mode = unconfig_str,
                num = pool_id,
                pool_num = pool_id ))
            conf_t_command_set.append("{mode}access-list {num} permit {net_addr} {net_wildcard}".format(
                mode = unconfig_str,
                num = pool_id,
                net_addr = client_net,
                net_wildcard = nat_obj.client_acl_wildcard))

            # bump up to the next client address
            client_net = misc_methods.get_single_net_client_addr(client_net, nat_obj.net_increment)
            pool_net   = misc_methods.get_single_net_client_addr(pool_net, nat_obj.net_increment)


        # assign generated config list to cache
        cache.add('CONF', conf_t_command_set)

        # deploy the configs (order is important!)
        return self.cmd_link.run_single_command( cache )


    def config_no_nat (self, nat_obj = None):
        # first, clear all nat translations
        self.clear_nat_translations()

        # then, decompose the known config
        if nat_obj is None and self.nat_config is not None:
            self.config_nat(self.nat_config, mode = 'unconfig')
            self.nat_config = None  # reverse current NAT config back to None (no nat config is known to run).
        elif nat_obj is not None:
            self.config_nat(nat_obj, mode = 'unconfig')
        else:
            raise UserWarning('No NAT configuration is available for removal.')


    def config_zbf (self, mode = 'config'):
        cache               = CCommandCache()
        pre_commit_cache    = CCommandCache()
        conf_t_command_set  = []        

        # toggle all duplicate interfaces down
        self.toggle_duplicated_intf(action = 'down')
        # dup_ifs = self.if_mngr.get_duplicated_if()
        # self.__toggle_interfaces(dup_ifs, action = 'down' )

        # define security zones and security service policy to be applied on the interfaces
        conf_t_command_set.append('class-map type inspect match-any c1')
        conf_t_command_set.append('match protocol tcp')
        conf_t_command_set.append('match protocol udp')
        conf_t_command_set.append('policy-map type inspect p1')
        conf_t_command_set.append('class type inspect c1')
        conf_t_command_set.append('inspect')
        conf_t_command_set.append('class class-default')
        conf_t_command_set.append('pass')

        conf_t_command_set.append('zone security z_in')
        conf_t_command_set.append('zone security z_out')

        conf_t_command_set.append('zone-pair security in2out source z_in destination z_out')
        conf_t_command_set.append('service-policy type inspect p1')
        conf_t_command_set.append('zone-pair security out2in source z_out destination z_in')
        conf_t_command_set.append('service-policy type inspect p1')
        conf_t_command_set.append('exit')

        pre_commit_cache.add('CONF', conf_t_command_set)

        for dual_if in self.if_mngr.get_dual_if_list(is_duplicated = False):
            cache.add('IF', "zone-member security z_in", dual_if.client_if.get_name() )
            cache.add('IF', "zone-member security z_out", dual_if.server_if.get_name() )

        self.cmd_link.run_command( [pre_commit_cache, cache] )

    def config_no_zbf (self):
        cache               = CCommandCache()
        conf_t_command_set  = []        

        # define security zones and security service policy to be applied on the interfaces
        conf_t_command_set.append('no zone-pair security in2out source z_in destination z_out')
        conf_t_command_set.append('no zone-pair security out2in source z_out destination z_in')

        conf_t_command_set.append('no policy-map type inspect p1')
        conf_t_command_set.append('no class-map type inspect match-any c1')

        conf_t_command_set.append('no zone security z_in')
        conf_t_command_set.append('no zone security z_out')

        cache.add('CONF', conf_t_command_set)

        for dual_if in self.if_mngr.get_dual_if_list(is_duplicated = False):
            cache.add('IF', "no zone-member security z_in", dual_if.client_if.get_name() )
            cache.add('IF', "no zone-member security z_out", dual_if.server_if.get_name() )

        self.cmd_link.run_command( [cache] )
        # toggle all duplicate interfaces back up
        self.toggle_duplicated_intf(action = 'up')
        # dup_ifs = self.if_mngr.get_duplicated_if()
        # self.__toggle_interfaces(dup_ifs)


    def config_ipv6_pbr (self, mode = 'config'):
        idx = 1
        unconfig_str = '' if mode=='config' else 'no '
        cache               = CCommandCache()
        conf_t_command_set  = []

        conf_t_command_set.append('{mode}ipv6 unicast-routing'.format(mode = unconfig_str) )

        for dual_if in self.if_mngr.get_dual_if_list():
            client_if_command_set   = []
            server_if_command_set   = []
            
            client_net_next_hop = misc_methods.get_single_net_client_addr(dual_if.server_if.get_ipv6_addr(), {'7':1}, ip_type = 'ipv6' )
            server_net_next_hop = misc_methods.get_single_net_client_addr(dual_if.client_if.get_ipv6_addr(), {'7':1}, ip_type = 'ipv6' )


            client_if_command_set.append ('{mode}ipv6 enable'.format(mode = unconfig_str))
            server_if_command_set.append ('{mode}ipv6 enable'.format(mode = unconfig_str))

            if dual_if.is_duplicated():
                prefix = 'ipv6_' + dual_if.get_vrf_name()
            else:
                prefix = 'ipv6'
                
            # config interfaces with relevant route-map
            client_if_command_set.append ('{mode}ipv6 policy route-map {pre}_{p1}_to_{p2}'.format( 
                mode = unconfig_str,
                pre = prefix, 
                p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
            server_if_command_set.append ('{mode}ipv6 policy route-map {pre}_{p2}_to_{p1}'.format( 
                mode = unconfig_str,
                pre = prefix, 
                p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )

            # config global arp to interfaces net address and vrf
            conf_t_command_set.append('{mode}ipv6 neighbor {next_hop} {intf} {dest_mac}'.format(
                    mode = unconfig_str,
                    next_hop = server_net_next_hop, 
                    intf = dual_if.client_if.get_name(),
                    dest_mac = dual_if.client_if.get_dest_mac()))
            conf_t_command_set.append('{mode}ipv6 neighbor {next_hop} {intf} {dest_mac}'.format(
                    mode = unconfig_str,
                    next_hop = client_net_next_hop, 
                    intf = dual_if.server_if.get_name(),
                    dest_mac = dual_if.server_if.get_dest_mac()))

            conf_t_command_set.append('{mode}route-map {pre}_{p1}_to_{p2} permit 10'.format(
                    mode = unconfig_str,
                    pre = prefix,
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
            if (mode == 'config'):
                conf_t_command_set.append('set ipv6 next-hop {next_hop}'.format(next_hop = client_net_next_hop ) )
            conf_t_command_set.append('{mode}route-map {pre}_{p2}_to_{p1} permit 10'.format(
                    mode = unconfig_str,
                    pre = prefix,
                    p1 = 'p'+str(idx), p2 = 'p'+str(idx+1) ) )
            if (mode == 'config'):
                conf_t_command_set.append('set ipv6 next-hop {next_hop}'.format(next_hop = server_net_next_hop ) )
                conf_t_command_set.append('exit')

            # assign generated config list to cache
            cache.add('IF', server_if_command_set, dual_if.server_if.get_name())
            cache.add('IF', client_if_command_set, dual_if.client_if.get_name())
            idx += 2

        cache.add('CONF', conf_t_command_set)
        
        # deploy the configs (order is important!)
        self.cmd_link.run_command( [cache] )

    def config_no_ipv6_pbr (self):
        self.config_ipv6_pbr(mode = 'unconfig')

    # show methods
    def get_cpu_util (self):
        response    = self.cmd_link.run_single_command('show platform hardware qfp active datapath utilization | inc Load')
        return CShowParser.parse_cpu_util_stats(response)

    def get_cft_stats (self):
        response    = self.cmd_link.run_single_command('test platform hardware qfp active infrastructure cft datapath function cft-cpp-show-all-instances')
        return CShowParser.parse_cft_stats(response)

    def get_nbar_stats (self):
        per_intf_stats = {}
        for intf in self.if_mngr.get_if_list(if_type = IFType.Client):
            response = self.cmd_link.run_single_command("show ip nbar protocol-discovery interface {interface} stats packet-count protocol".format( interface = intf.get_name() ), flush_first = True)
            per_intf_stats[intf.get_name()] = CShowParser.parse_nbar_stats(response)
        return per_intf_stats

    def get_nbar_profiling_stats (self):
        response = self.cmd_link.run_single_command("show platform hardware qfp active feature nbar profiling")
        return CShowParser.parse_nbar_profiling_stats(response)

    def get_drop_stats (self):

        response    = self.cmd_link.run_single_command('show platform hardware qfp active interface all statistics', flush_first = True)
        # print response
        # response    = self.cmd_link.run_single_command('show platform hardware qfp active interface all statistics')
        # print response
        if_list_by_name = [x.get_name() for x in self.if_mngr.get_if_list()]
        return CShowParser.parse_drop_stats(response, if_list_by_name )

    def get_nat_stats (self):
        response    = self.cmd_link.run_single_command('show ip nat statistics')
        return CShowParser.parse_nat_stats(response)

    def get_nat_trans (self):
        return self.cmd_link.run_single_command('show ip nat translation')

    def get_cvla_memory_usage(self):
        response    = self.cmd_link.run_single_command('show platform hardware qfp active infrastructure cvla client handles')
        # (res, res2) = CShowParser.parse_cvla_memory_usage(response)
        return CShowParser.parse_cvla_memory_usage(response)


    # clear methods
    def clear_nat_translations(self):
        pre_commit_cache = CCommandCache()
        # prevent new NAT entries
        # http://www.cisco.com/c/en/us/support/docs/ip/network-address-translation-nat/13779-clear-nat-comments.html
        for dual_if in self.if_mngr.get_dual_if_list(is_duplicated = False):
            pre_commit_cache.add('IF', "no ip nat inside", dual_if.client_if.get_name())
            pre_commit_cache.add('IF', "no ip nat outside", dual_if.server_if.get_name())
        # clear the translation
        pre_commit_cache.add('EXEC', 'clear ip nat translation *')
        self.cmd_link.run_single_command(pre_commit_cache)
        time.sleep(1)

    def clear_cft_counters (self):
        """ clear_cft_counters(self) -> None

        Clears the CFT counters on the platform
        """
        self.cmd_link.run_single_command('test platform hardware qfp active infrastructure cft datapath function cft-cpp-clear-instance-stats')

    def clear_counters(self):
        """ clear_counters(self) -> None

        Clears the platform counters
        """

        pre_commit_cache = CCommandCache()
        pre_commit_cache.add('EXEC', ['clear counters','\r'] )
        self.cmd_link.run_single_command( pre_commit_cache )

    def clear_nbar_stats(self):
        """ clear_nbar_stats(self) -> None

        Clears the NBAR-PD classification stats
        """
        pre_commit_cache = CCommandCache()
        pre_commit_cache.add('EXEC', ['clear ip nbar protocol-discovery','\r'] )
        self.cmd_link.run_single_command( pre_commit_cache )

    def clear_packet_drop_stats(self):
        """ clear_packet_drop_stats(self) -> None

        Clears packet-drop stats
        """
#       command = "show platform hardware qfp active statistics drop clear"
        self.cmd_link.run_single_command('show platform hardware qfp active interface all statistics clear_drop')

    ###########################################
    # file transfer and image loading methods #
    ###########################################
    def get_running_image_details (self):
        """ get_running_image_details() -> dict

        Check for the currently running image file on the platform.
        Returns a dictionary, where 'drive' key is the drive in which the image is installed,
        and 'image' key is the actual image file used.
        """
        response = self.cmd_link.run_single_command('show version | include System image')
        parsed_info = CShowParser.parse_show_image_version(response)
        self.running_image = parsed_info
        return parsed_info
        

    def check_image_existence (self, img_name):
        """ check_image_existence(self, img_name) -> boolean

        Parameters
        ----------
        img_name : str
            a string represents the image name.

        Check if the image file defined in the platform_config already loaded into the platform.
        """
        search_drives = ['bootflash', 'harddisk', self.running_image['drive']]
        for search_drive in search_drives:
            command = "dir {drive}: | include {image}".format(drive = search_drive, image = img_name)
            response = self.cmd_link.run_single_command(command, timeout = 10)
            if CShowParser.parse_image_existence(response, img_name):
                self.needed_image_path = '%s:/%s' % (search_drive, img_name)
                print('Found image in platform:', self.needed_image_path)
                return True
        return False

    def config_tftp_server(self, device_cfg_obj, external_tftp_config = None, applyToPlatform = False):
        """ configure_tftp_server(self, external_tftp_config, applyToPlatform) -> str

        Parameters
        ----------
        external_tftp_config : dict (Not is use)
            A path to external tftp config file other than using the one defined in the instance.
        applyToPlatform : boolean
            set to True in order to apply the config into the platform

        Configures the tftp server on an interface of the platform.
        """
#       tmp_tftp_config = external_tftp_config if external_tftp_config is not None else self.tftp_server_config
        self.tftp_cfg   = device_cfg_obj.get_tftp_info()
        cache           = CCommandCache()
        
        command = "ip tftp source-interface {intf}".format( intf = device_cfg_obj.get_mgmt_interface() )
        cache.add('CONF', command )
        self.cmd_link.run_single_command(cache)
        self.config_history['tftp_server_config'] = True

    def load_platform_image(self, img_filename, external_tftp_config = None):
        """ load_platform_image(self, img_filename, external_tftp_config) -> None

        Parameters
        ----------
        external_tftp_config : dict
            A path to external tftp config file other than using the one defined in the instance.
        img_filename : str
            image name to be saved into the platforms drive.

        This method loads the configured image into the platform's harddisk (unless it is already loaded),
        and sets that image to be the boot_image of the platform.
        """
        if not self.check_image_existence(img_filename): # check if this image isn't already saved in platform
            #tmp_tftp_config = external_tftp_config if external_tftp_config is not None else self.tftp_cfg
        
            if self.config_history['tftp_server_config']:  # make sure a TFTP configuration has been loaded
                cache = CCommandCache()
                if self.running_image is None:
                    self.get_running_image_details()
                
                command = "copy tftp://{tftp_ip}/{img_path}/{image} harddisk:".format(
                    tftp_ip  = self.tftp_cfg['ip_address'],
                    img_path = self.tftp_cfg['images_path'],
                    image    = img_filename)
                cache.add('EXEC', [command, '\r', '\r'])

                progress_thread = CProgressDisp.ProgressThread(notifyMessage = "Copying image via tftp, this may take a while...\n")
                progress_thread.start()

                response = self.cmd_link.run_single_command(cache, timeout = 900, read_until = ['\?', '\#'])
                print("RESPONSE:")
                print(response)
                progress_thread.join()
                copy_ok = CShowParser.parse_file_copy(response)

                if not copy_ok:
                    raise UserWarning('Image file loading failed. Please make sure the accessed image exists and has read privileges')
            else:
                raise UserWarning('TFTP configuration is not available. Please make sure a valid TFTP configuration has been provided')

    def set_boot_image(self, boot_image):
        """ set_boot_image(self, boot_image) -> None

        Parameters
        ----------
        boot_image : str
            An image file to be set as boot_image

        Configures boot_image as the boot image of the platform into the running-config + config-register
        """
        cache = CCommandCache()
        if self.needed_image_path is None:
            if not self.check_image_existence(boot_image):
                raise Exception("Trying to set boot image that's not found in router, please copy it first.")

        boot_img_cmd = "boot system flash %s" % self.needed_image_path
        config_register_cmd = "config-register 0x2021"
        cache.add('CONF', ["no boot system", boot_img_cmd, config_register_cmd, '\r'])
        response = self.cmd_link.run_single_command( cache )
        print("RESPONSE:")
        print(response)
        self.save_config_to_startup_config()

    def is_image_matches(self, needed_image):
        """ set_boot_image(self, needed_image) -> boolean

        Parameters
        ----------
        needed_image : str
            An image file to compare router running image

        Compares image name to router running image, returns match result.
        
        """
        if self.running_image is None:
            self.get_running_image_details()
        needed_image = needed_image.lower()
        current_image = self.running_image['image'].lower()
        if needed_image.find(current_image) != -1:
            return True
        if current_image.find(needed_image) != -1:
            return True
        return False

    # misc class related methods

    def load_platform_data_from_file (self, device_cfg_obj):
        self.if_mngr.load_config(device_cfg_obj)

    def launch_connection (self, device_cfg_obj):
        self.running_image = None # clear the image name "cache"
        self.cmd_link.launch_platform_connectivity(device_cfg_obj)

    def reload_connection (self, device_cfg_obj):
        self.cmd_link.close_platform_connection()
        self.launch_connection(device_cfg_obj)

    def save_config_to_startup_config (self):
        """ save_config_to_startup_config(self) -> None

        Copies running-config into startup-config.
        """
        cache = CCommandCache()
        cache.add('EXEC', ['wr', '\r'] )
        self.cmd_link.run_single_command(cache)

    def reload_platform(self, device_cfg_obj):
        """ reload_platform(self) -> None

        Reloads the platform.
        """
        from subprocess import call
        import os
        i = 0
        sleep_time = 30 # seconds

        try: 
            cache = CCommandCache()

            cache.add('EXEC', ['reload','n\r','\r'] )
            self.cmd_link.run_single_command( cache )

            progress_thread = CProgressDisp.ProgressThread(notifyMessage = "Reloading the platform, this may take a while...\n")
            progress_thread.start()
            time.sleep(60) # need delay for device to shut down before polling it
            # poll the platform until ping response is received.
            while True:
                time.sleep(sleep_time)
                try:
                    x = call(["ping", "-c 1", device_cfg_obj.get_ip_address()], stdout = open(os.devnull, 'wb'))
                except:
                    x = 1
                if x == 0:
                    break
                elif i > 20:
                    raise TimeoutError('Platform failed to reload after reboot for over {minutes} minutes!'.format(minutes = round(1 + i * sleep_time / 60)))
                else:
                    i += 1
                    
            time.sleep(30)
            self.reload_connection(device_cfg_obj)
            progress_thread.join()
        except Exception as e:
            print(e)

    def get_if_manager(self):
        return self.if_mngr

    def dump_obj_config (self, object_name):
        if object_name=='nat' and self.nat_config is not None:
            self.nat_config.dump_config()
        elif object_name=='static_route' and self.stat_route_config is not None:
            self.stat_route_config.dump_config()
        else:
            raise UserWarning('No known configuration exists.')

    def toggle_duplicated_intf(self, action = 'down'):

        dup_ifs = self.if_mngr.get_duplicated_if()
        self.__toggle_interfaces( dup_ifs, action = action )


    def __toggle_interfaces (self, intf_list, action = 'up'):
        cache = CCommandCache()
        mode_str = 'no ' if action == 'up' else ''

        for intf_obj in intf_list:
            cache.add('IF', '{mode}shutdown'.format(mode = mode_str), intf_obj.get_name())

        self.cmd_link.run_single_command( cache )


class CStaticRouteConfig(object):

    def __init__(self, static_route_dict):
        self.clients_start  = static_route_dict['clients_start']
        self.servers_start  = static_route_dict['servers_start']
        self.net_increment  = misc_methods.gen_increment_dict(static_route_dict['dual_port_mask'])
        self.client_mask    = static_route_dict['client_destination_mask']
        self.server_mask    = static_route_dict['server_destination_mask']
        self.client_net_start = self.extract_net_addr(self.clients_start, self.client_mask)
        self.server_net_start = self.extract_net_addr(self.servers_start, self.server_mask)
        self.static_route_dict = static_route_dict

    def extract_net_addr (self, ip_addr, ip_mask):
        addr_lst = ip_addr.split('.')
        mask_lst = ip_mask.split('.')
        mask_lst = [str(int(x) & int(y)) for x, y in zip(addr_lst, mask_lst)]
        return '.'.join(mask_lst)

    def dump_config (self):
        import yaml
        print(yaml.dump( self.static_route_dict , default_flow_style=False))


class CNatConfig(object):
    def __init__(self, nat_dict):
        self.clients_net_start  = nat_dict['clients_net_start']
        self.client_acl_wildcard= nat_dict['client_acl_wildcard_mask']
        self.net_increment      = misc_methods.gen_increment_dict(nat_dict['dual_port_mask'])
        self.nat_pool_start     = nat_dict['pool_start']
        self.nat_netmask        = nat_dict['pool_netmask']
        self.nat_dict           = nat_dict

    @staticmethod
    def calc_pool_end (nat_pool_start, netmask):
        pool_start_lst  = [int(x) for x in nat_pool_start.split('.')]
        pool_end_lst    = list( pool_start_lst )   # create new list object, don't point to the original one
        mask_lst        = [int(x) for x in netmask.split('.')]
        curr_octet      = 3 # start with the LSB octet
        inc_val         = 1

        while True:
            tmp_masked = inc_val & mask_lst[curr_octet]
            if tmp_masked == 0:
                if (inc_val << 1) > 255:
                    inc_val = 1
                    pool_end_lst[curr_octet] = 255
                    curr_octet -= 1
                else:
                    inc_val <<= 1
            else:
                pool_end_lst[curr_octet] += (inc_val - 1)
                break
        return '.'.join([str(x) for x in pool_end_lst])

    def dump_config (self):
        import yaml
        print(yaml.dump( self.nat_dict , default_flow_style=False))


if __name__ == "__main__":
    pass
