from trex.emu.api import *
from trex.emu.trex_emu_client import dump_json_yaml, wait_for_pressed_key
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

    DOT1X_METHODS = {
        4: 'EAP_TYPE_MD5',
        26: 'EAP_TYPE_MSCHAPV2'
    }


    def __init__(self, emu_client):
        super(DOT1XPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='dot1x_client_cnt')

    # API methods
    @client_api('getter', True)
    @update_docstring(EMUPluginBase._get_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def get_counters(self, c_key, cnt_filter=None, zero=True, verbose=True):
        return self._get_client_counters(c_key, cnt_filter, zero, verbose)

    @client_api('command', True)
    @update_docstring(EMUPluginBase._clear_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def clear_counters(self, c_key):
        return self._clear_client_counters(c_key)

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
        res = listify(res)

        for r in res:
            if 'state' in r:
                r['state'] = DOT1XPlugin.DOT1X_STATES.get(r['state'], r['state'])
            if 'method' in r:
                r['method'] = DOT1XPlugin.DOT1X_METHODS.get(r['method'], r['method'])
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
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True

    @plugin_api('dot1x_show_client_info', 'emu')
    def dot1x_show_client_info_line(self, line):
        '''Show dot1x client info (per 1 client).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "dot1x_show_client_info",
                                        self.dot1x_show_client_info_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.EMU_DUMPS_OPT,
                                        )

        opts = parser.parse_args(line.split())

        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        res = self.get_clients_info(c_key)

        if opts.json or opts.yaml:
            dump_json_yaml(data = res, to_json = opts.json, to_yaml = opts.yaml)
            return True

        keys_to_headers = [
            {'key': 'state',        'header': 'State'},
            {'key': 'method',       'header': 'Method'},
            {'key': 'eap_version',  'header': 'EAP Version'},
        ]
        args = {'title': 'Dot1x Client Information', 'empty_msg': 'No Dot1x information for client', 'keys_to_headers': keys_to_headers}
        self.print_table_by_keys(data = res, **args)
        return True

    @plugin_api('dot1x_show_all', 'emu')
    def dot1x_show_all_line(self, line):
        '''Show dot1x client info (per 1 ns).\n'''
        parser = parsing_opts.gen_parser(self,
                                        "dot1x_show_all",
                                        self.dot1x_show_all_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.SHOW_MAX_CLIENTS,
                                        parsing_opts.EMU_DUMPS_OPT,
                                        )

        opts = parser.parse_args(line.split())
        
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)

        if opts.json or opts.yaml:
            # gets all data, no pauses
            c_keys = self.emu_c.get_all_clients_for_ns(ns_key)
            all_data = self.get_clients_info(c_keys)
            return dump_json_yaml(all_data, opts.json, opts.yaml)

        c_keys_gen = self.emu_c._yield_n_clients_for_ns(ns_key = ns_key, amount = opts.max_clients)
        for i, c_keys_chunk in enumerate(c_keys_gen):
            if i != 0:
                wait_for_pressed_key('Press Enter to see more clients')
            
            c_keys_chunk = listify(c_keys_chunk)            
            res = self.get_clients_info(c_keys_chunk)
            for i, r in enumerate(res):
                r['mac'] = c_keys_chunk[i].mac.V()

            keys_to_headers = [
                {'key': 'mac',          'header': 'MAC'},
                {'key': 'state',        'header': 'State'},
                {'key': 'method',       'header': 'Method'},
                {'key': 'eap_version',  'header': 'EAP Version'},
            ]
            args = {'title': 'Dot1x Clients Information', 'empty_msg': 'No Dot1x information for those clients', 'keys_to_headers': keys_to_headers}
            self.print_table_by_keys(data = res, **args)

        return True
