#!/usr/bin/python

from console.plugins import *
from trex_stl_lib.services.trex_stl_service_IPv6ND import STLServiceIPv6ND
import struct
import socket

'''
IPv6 ND plugin
'''

class IPv6ND_plugin(ConsolePlugin):
    '''
        Service for implementing IPv6ND
    '''

    def __init__(self):
        super(IPv6ND_plugin, self).__init__()
        self.requests = list()
        self.vlan = list()
        self.fmt = ""
        self.count = 1

    def plugin_description(self):
        return 'IPv6 neighbor discovery plugin'

    def plugin_load(self):
        self.add_argument("-s", "--src-ip", type = str,
                dest = 'src_ip', 
                required=True,
                help = 'src ip to use')
        self.add_argument("-T", "--verify-timeout", type = int,
                dest = 'verify_timeout', 
                default=0,
                help = 'timeout to wait for neighbor verification NS')
        self.add_argument("-p", "--port", type = int,
                dest = 'port', 
                required=True,
                help = 'trex port to use')
        self.add_argument("-t", "--timeout", type = int,
                dest = 'timeout', 
                default=2,
                help = 'timeout to wait for NA')
        self.add_argument("-r", "--rate", type = int,
                dest = 'rate', 
                default = 1000,
                help = 'rate limiter value to pass to services framework')
        self.add_argument("-R", "--retries", type = int,
                dest = 'retries', 
                default = 2,
                help = 'number of retries in case no answer was received')
        self.add_argument("-c", "--count", type = int,
                dest = 'count', 
                default = 1,
                help = 'nr of nd to perform (auto-scale src-addr to test parallel NDs)')
        self.add_argument("-d", "--dst-ip", type = str,
                dest = 'dst_ip', 
                required=True,
                help = 'IPv6 dst ip to discover')
        self.add_argument("-m", "--src-mac", type = str,
                dest = 'src_mac', 
                help = 'src mac to use')
        self.add_argument("-v", "--vlan", type = str,
                dest = 'vlan', 
                default=None,
                help = 'vlan(s) to use (comma separated)')
        self.add_argument("-f", "--format", type = str,
                dest = 'fmt', 
                default=None,
                help = 'vlan encapsulation to use (QQ: qinq, DA: 802.1AD -> 802.1q)')
        self.add_argument("-V", "--verbose", action = 'store_true',
                dest = 'verbose', 
                default = None,
                help = 'verbose mode')
        self.add_argument("-b", "--brief", action = 'store_true',
                dest = 'brief', 
                default = None,
                help = 'show summary only')


    def do_resolve(self, port, src_ip, src_mac, dst_ip, vlan, fmt, timeout, verify_timeout, count, rate, retries, verbose):
        ''' perform IPv6 neighbor discovery '''
        print("")
        print("performing ND for {0} addresses.".format(count))
        print("NA response timeout............: {0}s".format(timeout))
        print("Neighbor verification timeout..: {0}s".format(verify_timeout))
        print("")

        ctx  = self.trex_client.create_service_ctx(port = port)
        mac_lists = None

        if vlan != None:
            self.vlan = [ int(x) for x in vlan.split(',') ]

        self.fmt = fmt
        self.count = count

        if verbose != None:
            verbose_level = STLServiceIPv6ND.INFO
        else:
            verbose_level = STLServiceIPv6ND.ERROR

        src_ip_list = struct.unpack("!HHHHHHHH", socket.inet_pton(socket.AF_INET6, src_ip))

        if src_mac != None:
            mac_list = src_mac.split(':')

        # empty requests (in case of multiple consecutive runs)
        self.requests = list()

        for c in range(0,count):
            tmp_src_ip_list = list(src_ip_list)
            tmp_src_ip_list[-1] += c
            tmp_src_ip = socket.inet_ntop(socket.AF_INET6, struct.pack("!HHHHHHHH", *tmp_src_ip_list))

            if src_mac != None:
                offset = c + int(mac_list[-1], 16)
                tmp_mac_list = [ int(x,16) for x in mac_list ]
                tmp_mac_list[-1] = (offset & 255)
                tmp_mac_list[-2] = (offset >> 8)
                tmp_mac = ":".join("{:02x}".format(x) for x in tmp_mac_list)

            else:
                tmp_mac = None

            self.requests.append(STLServiceIPv6ND(   ctx,
                                                src_mac=tmp_mac,
                                                vlan=self.vlan,
                                                src_ip = tmp_src_ip,
                                                dst_ip = dst_ip,
                                                timeout = timeout,
                                                retries = retries,
                                                verify_timeout = verify_timeout,
                                                verbose_level = verbose_level,
                                                fmt = self.fmt
                                            ))
        ctx.run(self.requests, rate)

    def do_status(self, brief):
        ''' show status of generated ND requests'''
        print("")
        print("")
        print("ND Status")
        print("---------")
        print("")
        print("used vlan(s)...................: {0}".format(self.vlan))
        print("used encapsulation.............: {0}".format(self.fmt))
        print("number of IPv6 source addresses: {0}".format(self.count))
        print("")
        print("")

        if brief == None:
            fmt_string = "{:17s}  {:30s} | {:30s} {:17s} {:12s} {:5}"
            header = fmt_string.format("    SRC MAC", "      SRC IPv6", "      DST IPv6", "    DST MAC", "STATE", "VERIFIED")
            print(header)
            print("-" * len(header))
            for r in self.requests:
                entry = r.get_record()
                print(fmt_string.format(entry.src_mac, entry.src_ip, entry.dst_ip, entry.dst_mac, entry.state, entry.neighbor_verifications))

        resolved   = 0
        unresolved = 0
        verified   = 0
        for r in self.requests:
            if r.get_record().is_resolved() == True:
                resolved += 1

                if r.get_record().neighbor_verifications > 0:
                    verified += 1
            else:
                unresolved += 1

        print()
        print("resolved..: {0} ".format(resolved))
        print("unresolved: {0}".format(unresolved))
        print("verified..: {0}".format(verified))
        print()

    def do_clear(self):
        ''' clear IPv6 ND requests/entries'''
        self.requests = list()
