import re
import socket, struct

# cfg must include a dummy router id in case bird running with no interfaces
# also must include device protocol for scanning new connections
DEFAULT_CFG = """
router id 100.100.100.100;
protocol device {
    scan time 1;
}
"""  

class BirdCFGCreator:
    '''
        This class is used to create bird.conf file. Able to read a given config or using a default one, add/remove routes/routes protocols.
        And finally create the wanted config file. 
    '''
    # The name of the static protocol where the routes will beadded
    stat_name = "bird_cfg_routes"  

    def __init__(self, cfg_string = DEFAULT_CFG):
        '''
            Construct BirdCFGCreator object with a given cfg_string.
            
            :Parameters:
                cfg_string: string
                    The given bird.conf with all the protocols as string. In case cfg_string was not supply default cfg will be filled instead
        '''
        self.cfg = cfg_string
        self.routes = []              # i.e: route 0.0.0.0/0 via 198.51.100.130; 
        self.extended_routes = []     # i.e:  route 192.168.10.0/24 via 198.51.100.100 {
                                      #            ospf_metric1 = 20;      # Set extended attribute
                                      #          }

        self.protocols = {}           # map every protocol to it's names and data
                                      # i.e: {protocol: {name1: {from_conf: True, data: "some data"}}}
        self._init_protocols_dict()

    def add_protocol(self, protocol, name, data, from_conf = False):
        '''
            Add protocol to our future cfg. 
            
            :Parameters:
                protocol: string
                    The protocol we are about to add 
                        i.e: bgp, rip..
                name: string
                    The name of out protocol in bird 
                        i.e: bgp1, my_rip
                        Must be unique in the cfg.
                data: string
                    The data inside the protocol as a string 
                        i.e:
                        "ipv4 {
                        import all;
                        export all;}"
                from_conf: bool
                    Internal usage, True/False if that protocol was given by the self.cfg. False by deafult
            :raises:
                + :exc:`Exception` - in case trying to add an existing protocol or static one with BirdCFGCreator name.
        '''
        if protocol == 'static' and name == BirdCFGCreator.stat_name:
            raise Exception('Protocol %s named: "%s" is saved for BirdCFGCreator!' % (protocol, name))
        if protocol in self.protocols.keys():
            if name in self.protocols[protocol]:
                raise Exception('Protocol %s named: "%s" is already in the config file' % (protocol, name))
            else:
                self.protocols[protocol][name] = {'from_conf': from_conf, 'data': data}
        else:
            self.protocols[protocol] = {name: {'from_conf': from_conf, 'data': data}}

    def remove_protocol(self, protocol, name):
        '''
            Remove the protocol from our future cfg.
            
            :Parameters:
                protocol: string
                    The protocol to be removed 
                        i.e: bgp, rip. Not to be confused with the name as it in bird i.e bgp1, my_rip..
                name: string 
                    Protocol name (as it in bird) to be removed 
                        i.e: bgp1, my_rip. Not to be confused with the protocol itself like bgp, rip..
            
            :raises:
                + :exc:`Exception` - in case protocol was not added before or it is part of the original bird.cfg.
        '''
        if protocol in self.protocols.keys() and name in self.protocols[protocol]:
            if self.protocols[protocol][name][from_conf]:
                raise Exception('cannot delete %s protocol named "%s", it is from conf file' % (protocol, name))
            del self.protocols[protocol][name]
            if self.protocols[protocol] == {}:
                del self.protocols[protocol]
        else:
            raise Exception('There is no %s protocol named "%s"' % (protocol, name))

    def add_route(self, dst_cidr, next_hop):
        """
            Adding simple route to our future cfg. Simple route is any route where after "via" there are no brackets and there is only 1 term
                i.e: route 1.1.1.0/24 via "eth0"
            
            :Parameters:
                dst_cidr: string
                    Destination ip and subnetmask in cidr notation
                        i.e: 1.1.1.0/24
                next_hop: string
                    Next hop to get the dst_cidr. 
        """
        self.routes.append(Route(dst_cidr, next_hop))
    
    def add_extended_route(self, dst_cidr, next_hop):
        """
            Adding more complex route to our future cfg. Extended route is any route where after "via" there are more than 1 term 
                i.e: route 10.1.1.0/24 via 198.51.100.3 { rip_metric = 3; };
            
            :Parameters:
                dst_cidr: string
                    Destination ip and subnetmask in cidr notation i.e: 1.1.1.0/24
                next_hop: string
                    Next hop to get the dst_cidr. In extended route next_hop is more informative
                        i.e: 198.51.100.3 { rip_metric = 3; };
        """
        self.extended_routes.append(ExtRoute(dst_cidr, next_hop))

    def add_many_routes(self, start_ip, total_routes, next_hop, jump = 1):
        """
            Adding many simple routes to our future cfg. The function iterates from "start_ip" incrementing by "jump" with "total_routes" 
            
            :Parameters:
                start_ip: string
                    First ip to start to start counting from i.e: 1.1.1.2
                
                total_routes: string
                    Total number of routes to add
                
                next_hop: string
                    The next hop that will be in each route

                jump: string
                    The amount of ip addresses to jump from each route
        """
        for dst, from_str in self._generate_ips(start_ip, total_routes, next_hop, jump):
            self.add_route(dst, from_str)

    def remove_route(self, dst_cidr, next_hop = None):
        '''
            Remove route from our future cfg by his dst_cidr and next_hop. If next_hop was not provided it will remove every route with dst_cidr.
            
            :Parameters:
                dst_cidr: string
                    Destination ip and subnetmask in cidr notation
                        i.e: 1.1.1.0/24
                next_hop: string
                    Next hop to get the dst_cidr. This is an optional argument, in case it was not provided it will remove every route with dst_cidr.
        '''
        wanted_route = Route(dst_cidr, next_hop)
        if next_hop is None:
            # remove only by dst ip
            results = [r for r in self.routes if r.dst_cidr == wanted_route.dst_cidr]
            results.extend([r for r in self.extended_routes if r.dst_cidr == wanted_route.dst_cidr])
        else:
            results = [r for r in self.routes if r == wanted_route]
            results.extend([r for r in self.extended_routes if r == wanted_route])
        if len(results) == 0:
            print("Did not find route: %s" % dst_cidr)
        for r in results:
            self.routes.remove(r)

    def build_config(self):
        '''
            Create our final bird.conf content. Merge the given bird.cfg from constructor with the routes & protocols the user added.
        '''
        strings_to_merge = []

        # handle all non-static protocols #
        for protocol, pro_data in self.protocols.items():
            for pro_name, pro_name_data in pro_data.items():
                if pro_name_data['from_conf']:
                    continue
                pro_name_data['data'] = self._fix_protocol_data(pro_name_data['data'])
                strings_to_merge.append('\nprotocol ' + protocol + ' ' + pro_name + ' ' + pro_name_data['data'] + '\n')        
        
        # handle static routes #
        all_routes = self.routes + self.extended_routes
        all_routes_string = '\n'.join([str(r) for r in all_routes]) 

        if 'static' in self.protocols.keys() and BirdCFGCreator.stat_name in self.protocols['static']:
            # conf file already has our static protcol, just add the routes
            cfg_lines = self.cfg.split('\n')
            static_pro_lines = [(index, line) for index, line in enumerate(cfg_lines) if "protocol static %s" % BirdCFGCreator.stat_name in line]
            static_line = static_pro_lines[0][0]
            curr_line = cfg_lines[static_line].strip()
            while ('}' not in curr_line) or ('};' in curr_line):
                static_line += 1
                curr_line = cfg_lines[static_line].strip()
            cfg_lines.insert(static_line , all_routes_string)
            self.cfg = '\n'.join(cfg_lines) 
        else:
            ipv4_addition = "ipv4 {\nexport all;\nimport all;\n};"
            # ipv6_addition = "ipv6 {\nexport all;\nimport all;\n};"
            static_protocol = "protocol static %s {\n%s\n%s\n}\n" % (BirdCFGCreator.stat_name, ipv4_addition, all_routes_string)
            strings_to_merge.append(static_protocol)

        return self.cfg + '\n'.join(strings_to_merge)

    def _generate_ips(self, start, total_routes, next_hop, jump):
        start = struct.unpack('>I', socket.inet_aton(start))[0]
        end = int(start + total_routes * jump)
        for i in range(start, end, jump):
            s = '%s/32via%s;' % (socket.inet_ntoa(struct.pack('>I', i)), next_hop)
            yield s.split('via')        

    def _fix_protocol_data(self, data):
        """ fix opening & closing brackets for a data protocol string """
        data = data.strip()
        if not data.startswith('{'):
            data = '{\n' + data
        if not data.endswith('}'):
            data = data + '\n}\n'
        return data

    def _init_protocols_dict(self):
        protocol_name_pat = "protocol .* \{"
        protocol_data_pat = "\{.*?\}\n"

        names = re.findall(protocol_name_pat, self.cfg)
        datas = re.findall(protocol_data_pat, self.cfg, flags=re.DOTALL)

        for name, data in zip(names, datas):
            name = name.split(' ')
            pro, pro_name = name [1], name[2]
            if pro_name.startswith('{'):
                pro_name = pro  # if there is no name, take the protocol as name
            if self._is_api_static_protocol(pro_name):
                self.protocols[pro] = {pro_name: {'from_conf': True, 'data': data}}
            else:
                self.add_protocol(pro, pro_name, data.strip('{/}'), from_conf = True)

    def _is_api_static_protocol(self, pro_name):
        return pro_name == BirdCFGCreator.stat_name

    def __repr__(self):
        return "Routes: {}\nExtended: {}\nProtocols: {}".format(self.routes, self.extended_routes, self.protocols)

    # example methods
    def add_simple_rip(self):
        """ Add simple rip config """
        rip_data = """
                ipv4 {
                    import all;
                    export all;
                };
                interface "*";
        """
        self.add_protocol("rip", "rip1", rip_data)

    def add_simple_bgp(self):
        """ Add simple bgp config """
        bgp_data1 = """
            local 1.1.1.3 as 65000;
            neighbor 1.1.1.1 as 65000;
            ipv4 {
                    import all;
                    export all;
            };
        """
        bgp_data2 = """
            local 1.1.2.3 as 65000;
            neighbor 1.1.2.1 as 65000;
            ipv4 {
                    import all;
                    export all;
            };
        """
        self.add_protocol("bgp", "my_bgp1", bgp_data1)
        self.add_protocol("bgp", "my_bgp2", bgp_data2)

    def add_simple_ospf(self):
        """ Add simple ospf config """
        ospf_data = """
                    ipv4 {
                            import all;
                            export all;
                    };
                    area 0 {
                        interface "*" {
                            type pointopoint;
                        };
                    }; 
        """
        self.add_protocol("ospf", "ospf1", ospf_data)


class Route:

    ipv4_cidr_re = "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(\/(3[0-2]|[1-2][0-9]|[0-9]))$"
    ipv6_cidr_re = "^s*((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]d|1dd|[1-9]?d)(.(25[0-5]|2[0-4]d|1dd|[1-9]?d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]d|1dd|[1-9]?d)(.(25[0-5]|2[0-4]d|1dd|[1-9]?d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]d|1dd|[1-9]?d)(.(25[0-5]|2[0-4]d|1dd|[1-9]?d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]d|1dd|[1-9]?d)(.(25[0-5]|2[0-4]d|1dd|[1-9]?d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]d|1dd|[1-9]?d)(.(25[0-5]|2[0-4]d|1dd|[1-9]?d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]d|1dd|[1-9]?d)(.(25[0-5]|2[0-4]d|1dd|[1-9]?d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]d|1dd|[1-9]?d)(.(25[0-5]|2[0-4]d|1dd|[1-9]?d)){3}))|:)))(%.+)?s*(\/([0-9]|[1-9][0-9]|1[0-1][0-9]|12[0-8]))?$"

    def __init__(self, dst_cidr, next_hop):
        ''' Construct Route object specifing how to get dst_cidr '''
        if not Route.is_ipv4_cidr(dst_cidr) and not Route.is_ipv6_cidr(dst_cidr):
            raise ValueError("Destention IP: '{}' isn't valid, should be: A.B.C.D/MASK".format(dst_cidr))
        
        self.dst_cidr = dst_cidr
        self.next_hop = next_hop
    
    @staticmethod
    def is_ipv4_cidr(ip):
        return re.search(Route.ipv4_cidr_re, ip)

    @staticmethod
    def is_ipv6_cidr(ip):
        return re.search(Route.ipv6_cidr_re, ip)

    def __cmp__(self, other):
        return self.dst_cidr == other.dst_cidr and self.next_hop == other.next_hop

    def __repr__(self):
        semi_colon = '' if self.next_hop.endswith(';') else ';'
        return "route {} via {}{}".format(self.dst_cidr, self.next_hop, semi_colon)

class ExtRoute(Route):

    def __init__(self, dst_cidr, next_hop):
        ''' Construct Extended Route object specifing how to get dst_cidr, next_hop is a string'''
        next_hop = next_hop.lstrip()
        if next_hop.startswith('via'):
            next_hop = next_hop[4:]  # remove the via, it's added in repr() method
        Route.__init__(self, dst_cidr, next_hop)

    def __repr__(self):
        return "route {} via {}".format(self.dst_cidr, self.next_hop)
