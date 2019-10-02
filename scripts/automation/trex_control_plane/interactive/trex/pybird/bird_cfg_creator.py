import re

# cfg must include a dummy router id in case bird running with no interfaces
# also must include device protocol for scanning new connections
DEFAULT_CFG = """
router id 100.100.100.100;
protocol device {
    scan time 1;
}
"""  

class BirdCFGCreator:

    # The name of the static protocol where the routes will beadded
    stat_name = "bird_cfg_routes"  

    def __init__(self, cfg_string=DEFAULT_CFG):
        ''' Construct BirdCFGCreator object for integrating routes and bird cfg
        
        Parameters:
        cfg_string (string): The given BIRD cfg with all the protocols as string
        '''
        self.cfg = cfg_string
        self.routes = []              # i.e: route 0.0.0.0/0 via 198.51.100.130; 
        self.extended_routes = []     # i.e:  route 192.168.10.0/24 via 198.51.100.100 {
                                      #            ospf_metric1 = 20;      # Set extended attribute
                                      #          }

        self.protocols = {}           # map every protocol to it's names and data
                                      # i.e: {protocol: {name1: {from_conf: True, data: "some data"}}}
        self._init_protocols_dict()

    def add_protocol(self, protocol, name, data, from_conf=False):
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
        if protocol in self.protocols.keys() and name in self.protocols[protocol]:
            if self.protocols[protocol][name][from_conf]:
                raise Exception('cannot delete %s protocol named "%s", it is from conf file' % (protocol, name))
            del self.protocols[protocol][name]
            if self.protocols[protocol] == {}:
                del self.protocols[protocol]
        else:
            raise Exception('There is no %s protocol named "%s"' % (protocol, name))

    def add_route(self, dst_cidr, next_hop):
        self.routes.append(Route(dst_cidr, next_hop))
    
    def add_extended_route(self, dst_cidr, next_hop):
        self.extended_routes.append(ExtRoute(dst_cidr, next_hop))

    def remove_route(self, dst_cidr, next_hop=None):
        wanted_route = Route(dst_cidr, next_hop)
        if next_hop is None:
            # remove only by dst ip
            results = [r for r in self.routes if r.dst_cidr == wanted_route.dst_cidr]
            results.extend([r for r in self.extended_routes if r.dst_cidr == wanted_route.dst_cidr])
        else:
            results = [r for r in self.routes if r == wanted_route]
            results.extend([r for r in self.extended_routes if r == wanted_route])
        if len(results) == 0:
            print("Didn't remove anything!")
        for r in results:
            self.routes.remove(r)

    def merge_to_string(self):
        strings_to_merge = []

        # handle all non-static protocols #
        for protocol, pro_data in self.protocols.items():
            for pro_name, pro_name_data in pro_data.items():
                if pro_name_data['from_conf']:
                    continue
                pro_name_data['data'] = self._fix_protocol_data(pro_name_data['data'])
                strings_to_merge.append('protocol ' + protocol + ' ' + pro_name + ' ' + pro_name_data['data'])        
        
        # handle static routes #
        all_routes = self.routes + self.extended_routes
        all_routes_string = '\n'.join([str(r) for r in all_routes]) 
        if 'static' in self.protocols.keys() and BirdCFGCreator.stat_name in self.protocols['static']:
            # conf file alredy has our static protcol, just add the routes
            cfg_lines = self.cfg.split('\n')
            static_pro_lines = [(index, line) for index, line in enumerate(cfg_lines) if "protocol static %s" % BirdCFGCreator.stat_name in line]
            static_line = static_pro_lines[0][0]
            curr_line = cfg_lines[static_line].strip()
            while ('}' not in curr_line) or ('};' in curr_line):
                static_line += 1
                curr_line = cfg_lines[static_line].strip()
            cfg_lines.insert(curr_line , all_routes_string)
            self.cfg = '\n'.join(cfg_lines) 
        else:
            ipv4_addition = "ipv4 {\nexport all;\nimport all;\n};"
            # ipv6_addition = "ipv6 {\nexport all;\nimport all;\n};"
            static_protocol = "protocol static %s {\n%s\n%s\n}" % (BirdCFGCreator.stat_name, ipv4_addition, all_routes_string)
            strings_to_merge.append(static_protocol)

        return self.cfg + '\n'.join(strings_to_merge)
        
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
            if name[2].startswith('{'):
                name[2] = name[1]  # if there is no name, take the protocol as name 
            self.add_protocol(name[1], name[2], data.strip('{/}'), from_conf=True)

    def __repr__(self):
        return "Routes: {}\nExtended: {}\nProtocols: {}".format(self.routes, self.extended_routes, self.protocols)
             
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
