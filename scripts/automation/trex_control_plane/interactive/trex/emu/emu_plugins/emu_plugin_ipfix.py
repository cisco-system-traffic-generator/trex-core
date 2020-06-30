from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_validator import EMUValidator
import trex.utils.parsing_opts as parsing_opts


class IPFIXPlugin(EMUPluginBase):
    '''Defines ipfix plugin '''

    plugin_name = 'IPFIX'

    # init jsons example for SDK
    INIT_JSON_NS = None
    """
    There is currently no ns init json for IPFix.
    """

    INIT_JSON_CLIENT = {'avcnet': {
                            "netflow_version": 10,
                            "dst_mac": [0, 0, 1, 0, 0, 0],
                            "dst_ipv4": [48, 0, 0, 0],
                            "dst_ipv6": [32, 1, 13, 184, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2],
                            "dst_port": 6007,
                            "src_port": 30334,
                            "generators": {
                                "gen1": {
                                    "type": "dns",
                                    "auto_start": True,
                                    "rate_pps": 5,
                                    "data_records": 7,
                                    "gen_type_data": {
                                        # DNS example
                                        "client_ip": [15, 0, 0, 1],
                                        "range_of_clients": 100,
                                        "dns_servers": 150,
                                        "nbar_hosts": 125
                                    }
                                }
                            }
                        }
                    }
    """
    :parameters:
        netflow_version: uint16
            Netflow version. Might be 9 or 10, notice version 9 doesn't support length variable and PEN.
        dst_mac: [6]bytes
            Optional, collector mac address. If not supplied wil try resolving mac from IP address.
        dst_ipv4: [4]byte
            Required, collector ipv4 address.
        dst_ipv6: [16]byte
            Optional, collector ipv6 address.
        dst_port: uint16
            Collector port, defaults to 4739.
        src_port: uint16
            Collector port, defaults to 30334.
        generators: dict of generators
            Keys as generator names and values as generator inforation.
    :Generators info:
        type: string
            Type of generator i.e dns.
        auto_start: bool
            True will start sending packets on creation. Defaults to false.
        rate_pps: uint16
            Rate of data packets (in pps), defaults to 3.
        data_records: uint16
            Number of data record in each data packet, defaults to max records according to MTU.
        gen_type_data: dict
            Dictionary with gen_type keys and values.
    :DNS gen_type_data:
        client_ip: [4]byte
            First client ipv4 address.
        range_of_clients: uint16
            Number of clients in total starting from `client_ip`. Each record will choose an ip address randomly.
        dns_servers: uin16
            Number of dns servers to randomize. Each record will choose a dns address randomly.
        nbar_hosts: uint16
            Number of nbar hosts to randomize i.e: random.co.il. Each record will choose an nbar host randomly.
    """

    def __init__(self, emu_client):
        """
        Init IpfixPlugin. 
        
            :parameters:
                emu_client: EMUClient
                    Valid emu client.
        """        
        super(IPFIXPlugin, self).__init__(emu_client, 'avcnet_c_cnt')

    # API methods
    @client_api('getter', True)
    def get_gens_infos(self, c_key, gen_names):
        """
        Gets ipfix generators information. 
        
            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
                gen_name: list of string
                    Name of the generators we want to get their infos.

            :returns:
                | dict:
                | {      
                | 'type': string
                | 'is_running': bool
                | 'template_rate_pps': float32
                | 'data_rate_pps': float32
                | 'data_records': uint32
                | 'flow_sequence': uint32
                | }
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey},
                    {'name': 'gen_names', 'arg': gen_names, 't': str, 'allow_list': True},
                    ]
        EMUValidator.verify(ver_args)
        res = self.emu_c._send_plugin_cmd_to_client('avcnet_c_get_gens_info', c_key, gen_names=gen_names)
        return res.get('gens_infos', {})

    @client_api('command', True)
    def set_gen_rate(self, c_key, gen_name, rate):
        """
        Set ipfix generator rate (in pps). 
        
            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
                gen_name: string
                    The name of the generator to alter.
                rate: uint32.
                    New data pkt rate to set, in pps.
            :returns:
               bool : Result of operation.
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey},
                    {'name': 'gen_name', 'arg': gen_name, 't': str},
                    {'name': 'rate', 'arg': rate, 't': int}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client('avcnet_c_set_gen_state', c_key=c_key, gen_name=gen_name, rate=rate)

    @client_api('command', True)
    def set_gen_running(self, c_key, gen_name, running):
        """
        Set ipfix generator value. 
        
            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
                gen_name: string
                    The name of the generator to alter.
                running: bool.
                    True for active this generator false for stopping.
            :returns:
               bool : Result of operation.
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey},
                    {'name': 'gen_name', 'arg': gen_name, 't': str},
                    {'name': 'running', 'arg': running, 't': bool}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client('avcnet_c_set_gen_state', c_key=c_key, gen_name=gen_name, running=running)


    # Plugins methods
    @plugin_api('ipfix_show_counters', 'emu')
    def ipfix_show_counters_line(self, line):
        '''Show ipfix data counters data.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "show_counters_ipfix",
                                        self.ipfix_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns = True)
        return True


    @plugin_api('ipfix_get_gens_infos', 'emu')
    def ipfix_get_gens_infos_line(self, line):
        '''Show ipfix generators information.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipfix_get_gens_info",
                                        self.ipfix_get_gens_infos_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.GEN_NAMES,
                                        parsing_opts.EMU_DUMPS_OPT,
                                        )

        opts = parser.parse_args(line.split())
        self._validate_port(opts)
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        res = self.get_gens_infos(c_key, opts.gen_names)

        if opts.json or opts.yaml:
            dump_json_yaml(data = res, to_json = opts.json, to_yaml = opts.yaml)
            return
    
        keys_to_headers = [{'key': 'type',               'header': 'Generator Type'},
                            {'key': 'is_running',        'header': 'Active'},
                            {'key': 'template_rate_pps', 'header': 'Template Rate (pps)'},
                            {'key': 'data_rate_pps',     'header': 'Data Rate (pps)'},
                            {'key': 'data_records',      'header': '#Data Records'},
                            {'key': 'flow_sequence',     'header': 'Current Flow Sequence'},
        ]

        for gen_name, gen_info in res.items():
            self.print_table_by_keys(gen_info, keys_to_headers, title=gen_name)

    @plugin_api('ipfix_start_gen', 'emu')
    def ipfix_start_gen_line(self, line):
        '''Start given ipfix generator.\n'''
        res = self._start_stop_gen_line(line, self.ipfix_start_gen_line, 'start')
        self.logger.post_cmd(res)

    @plugin_api('ipfix_stop_gen', 'emu')
    def ipfix_stop_gen_line(self, line):
        '''Stop given ipfix generator.\n'''
        res = self._start_stop_gen_line(line, self.ipfix_stop_gen_line, 'stop')
        self.logger.post_cmd(res)

    def _start_stop_gen_line(self, line, caller_func, start_stop):
        parser = parsing_opts.gen_parser(self,
                                        "ipfix_start_gen",
                                        caller_func.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.GEN_NAME,
                                        )

        opts = parser.parse_args(line.split())
        self._validate_port(opts)
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        return self.set_gen_running(c_key, opts.gen_name, start_stop == 'start')

    @plugin_api('ipfix_set_data_rate', 'emu')
    def ipfix_set_data_rate_line(self, line):
        '''Set ipfix generator data rate.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "ipfix_set_data_rate",
                                        self.ipfix_set_data_rate_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.GEN_NAME,
                                        parsing_opts.GEN_RATE,
                                        )

        opts = parser.parse_args(line.split())
        self._validate_port(opts)
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        return self.set_gen_rate(c_key, opts.gen_name, rate = opts.rate)
