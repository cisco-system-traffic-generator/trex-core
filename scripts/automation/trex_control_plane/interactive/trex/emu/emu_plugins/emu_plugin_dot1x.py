from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_validator import EMUValidator
from trex.emu.trex_emu_conversions import Mac
import trex.utils.parsing_opts as parsing_opts


class DOT1XPlugin(EMUPluginBase):
    '''Defines DOT1x plugin  RFC 8415  dot1x client with
        EAP-MD5
        EAP-MSCHAPv2
     '''

    plugin_name = 'DOT1X'

    # init jsons example for SDK
    INIT_JSON_NS = {}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'dot1x': {'user': 'username', 'password': 'pass', 'nthash': 'a12db4', 'timeo_idle': 2, 'max_start': 2}}
    """
    :parameters:
        user: string
            User name.
        password: string
            password
        nthash: string
            Hash string for MSCHAPv2
        timeo_idle: uint32
                timeout for success in sec
        max_start: uint32
            max number of retries
    """

    DOT1X_STATES = {
        1: 'EAP_WAIT_FOR_IDENTITY',
        2: 'EAP_WAIT_FOR_METHOD',
        3: 'EAP_WAIT_FOR_RESULTS',
        4: 'EAP_DONE_OK',
        5: 'EAP_DONE_FAIL'
    }

    DOT1X_METHOD_STATES = {
        1: 'METHOD_DONE',
        5: 'METHOD_INIT',
        6: 'METHOD_CONT',
        7: 'METHOD_MAY_CONT'
    }


    def __init__(self, emu_client):
        super(DOT1XPlugin, self).__init__(emu_client, 'dot1x_client_cnt')

    @client_api('command', True)
    def get_clients_info(self, c_keys):
        """
        Get dot1x clients information.
        
            :parameters:
                c_keys: list of EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`
        
            :return:
                | list: List of clients information
                | [{'state': string, 'method': string, 'eap_version': int}, {..}]
        """
        ver_args = [{'name': 'c_keys', 'arg': c_keys, 't': EMUClientKey, 'allow_list': True},]
        EMUValidator.verify(ver_args)
        c_keys = listify(c_keys)
        res = self.emu_c._send_plugin_cmd_to_clients('dot1x_client_info', c_keys)
        for r in res:
            if 'state' in r:
                r['state'] = DOT1XPlugin.DOT1X_STATES.get(r['state'], 'Unknown state')
            if 'method' in r:
                r['method'] = DOT1XPlugin.DOT1X_METHOD_STATES.get(r['method'], 'Unknown state')
        return res

    # Plugins methods
    @plugin_api('dot1x_show_counters', 'emu')
    def dot1x_show_counters_line(self, line):
        '''Show dot1x counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "dot1x_show_counters",
                                        self.dot1x_show_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.EMU_CLIENT_GROUP,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns = True)
        return True

    @plugin_api('dot1x_get_clients_info', 'emu')
    def dot1x_get_clients_info_line(self, line):
        '''Show dot1x counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "dot1x_get_clients_info",
                                        self.dot1x_get_clients_info_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESSES,
                                        )

        opts = parser.parse_args(line.split())

        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_keys = []
        for mac in opts.macs:
            mac = Mac(mac)
            c_keys.append(EMUClientKey(ns_key, mac.V()))
        res = self.get_clients_info(c_keys)
        keys_to_headers = [
            {'key': 'state',        'header': 'State'},
            {'key': 'method',       'header': 'Method'},
            {'key': 'eap_version',  'header': 'EAP Version'},
        ]
        args = {'title': 'Dot1x Clients Information', 'empty_msg': 'No Dot1x information for those clients', 'keys_to_headers': keys_to_headers}
        self.print_table_by_keys(data = res, **args)
        return True
