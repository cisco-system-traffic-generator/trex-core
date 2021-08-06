from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_validator import EMUValidator
import trex.utils.parsing_opts as parsing_opts
from trex.utils import text_tables
from trex.common.trex_types import listify


class MDNSPlugin(EMUPluginBase):
    """
        Defines a mDNS (Multicast DNS) plugin based on `mDNS <https://en.wikipedia.org/wiki/Multicast_DNS>`_ 

        Implemented based on `RFC 6762 <https://tools.ietf.org/html/rfc6762>`_ 
    """

    plugin_name = 'mDNS'

    # init json examples for SDK
    INIT_JSON_NS = { 'mdns': "Pointer to INIT_JSON_NS below"  }
    """
    :parameters:
        auto_play: bool
            Indicate if should automatically start generating traffic. Defaults to False.

        auto_play_params: dictionary.
            Dictionary specifying the paramaters of auto play, should be provided only in case autoplay is defined.

            :auto_play_params:

                rate: float
                    Rate in seconds between two consequent queries. Defaults to 1.0.

                query_amount: uint64
                    Amount of queries to send. Defaults to 0. The default value (zero) means infinite amount of queries.

                min_client: string
                    String representing the MAC address of the first client in the range of clients that will query.
                    For example "00:00:00:70:00:01".

                max_client: string
                    String representing the MAC address of the last client in the range of clients that will query.

                client_step: uint16
                    Incremental step between two consecutive clients. Defaults to 1.

                hostname_template: string
                    String that will be used as template format for hostname in queries. One number will be injected in it.
                    The number should be represented with %v, because the backend is in Golang.

                min_hostname: uint16
                    Unsigned integer representing the minimal value that can be applied to `hostname_template`.

                max_hostname: uint16
                    Unsigned integer representing the maximal value that can be applied to `hostname_template`.

                init_hostname: uint16
                    Unsigned integer representing the initial value that can be applied to `hostname_template`. Defaults to `min_hostname`.

                hostname_step: uint16
                    Incremental step between two consecutive hostnames. Defaults to 1.

                type: string
                    Dns Type. Allowed values are A, AAA, PTR and TXT. Defaults to A.

                class: string
                    Dns Class. Defaults to IN.

                ipv6: bool
                    Indicate if should send IPv6 traffic. Defaults to False.

                program: dictionary
                    Dictionary representing special/uncommon queries for clients. If a client entry is specified here, this has precedence over the standard query.

                    :key: string
                        MAC address of the client that sends the query.

                    :value: dictionary

                            hostnames: list
                                List of strings that specifies which hostnames to query.

                            type: string
                                Dns Type. Allowed values are A, AAA, PTR and TXT. Defaults to A.

                            class: string
                                Dns Class. Defaults to IN.

                            ipv6: bool
                                Indicate if should send IPv6 traffic. Defaults to False.

                    .. highlight:: python
                    .. code-block:: python

                        "program": {
                            "00:00:01:00:00:01": {
                                "hostnames": ["AppleTV"],
                                "type": "TXT",
                                "ipv6": true
                            }
                        }

        .. highlight:: python
        .. code-block:: python

            {
                "auto_play": true,
                "auto_play_params": {
                    "rate": 1.0,
                    "query_amount": 5,
                    "min_client": "00:00:01:00:00:00",
                    "max_client": "00:00:01:00:00:02",
                    "hostname_template": "client-%v"
                    "min_hostname": 0,
                    "max_hostname": 2,
                    "init_hostname": 1,
                    "hostname_step": 1,
                    "program": {
                        "00:00:01:00:00:02": {
                            "hostnames": ["UCS", "AppleTV"],
                            "type": "TXT",
                        }
                    }
                }
            }
    """

    INIT_JSON_CLIENT = { 'mdns': "Pointer to INIT_JSON_CLIENT below" }
    """
    :parameters:

        hosts: list
            List of hosts (strings) this client owns.

        ttl: uint32
            Time to live to set in mDNS replies. Defaults to 240.

        domain_name: string
            Domain Name to set in PTR replies.

        txt: list
            List of dictionaries that define the data in a TXT reply. Each dictionary looks like:

            :dictionary:

                field: string

                    Field like HW, OS etc.

                value: string

                    Value of the field.

            .. highlight:: python
            .. code-block:: python

                "txt": [
                    {
                        "field": "HW",
                        "value": "Cisco UCS 220M5"
                    },
                    {
                        "field": "OS",
                        "value": "Ubuntu 20.04",
                    }
                ]
    """


    def __init__(self, emu_client):
        """
        Initialize a MDnsPlugin.

            :parameters:
                emu_client: :class:`trex.emu.trex_emu_client.EMUClient`
                    Valid EMU client.
        """
        super(MDNSPlugin, self).__init__(emu_client, ns_cnt_rpc_cmd='mdns_ns_cnt', client_cnt_rpc_cmd='mdns_c_cnt')

    # API methods
    @client_api('getter', True)
    @update_docstring(EMUPluginBase._get_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def get_client_counters(self, c_key, cnt_filter=None, zero=True, verbose=True):
        return self._get_client_counters(c_key, cnt_filter, zero, verbose)

    @client_api('command', True)
    @update_docstring(EMUPluginBase._clear_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def clear_client_counters(self, c_key):
        return self._clear_client_counters(c_key)

    @client_api('getter', True)
    @update_docstring(EMUPluginBase._get_ns_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def get_ns_counters(self, ns_key, cnt_filter=None, zero=True, verbose=True):
        return self._get_ns_counters(ns_key, cnt_filter, zero, verbose)

    @client_api('command', True)
    @update_docstring(EMUPluginBase._clear_ns_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def clear_ns_counters(self, ns_key):
        return self._clear_ns_counters(ns_key)

    @client_api('command', True)
    def query(self, c_key, queries):
        """Send a mDNS query.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`

            queries: list
                List of query, where each query is represented by:

            :query:

                name: string
                    Name of the host we are querying.

                dns_type: string
                    Type of the mDNS query, supported types are A, AAAA, PTR, TXT. Default is "A".

                dns_class: string
                    Class of the mDNS query. Default is "IN".

                ipv6: bool
                    Indicate if the query should be sent using IPv6.

        :raises:
            TRexError
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'queries', 'arg': queries, 't': list, 'must': True}]
        EMUValidator.verify(ver_args)
        self.emu_c._send_plugin_cmd_to_client(cmd='mdns_c_query', c_key=c_key, queries=queries)

    @client_api('command', True)
    def add_remove_hosts(self, c_key, op, hosts):
        """ Add or Remove hosts (hostnames) from a mDNS client.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`

            op: bool
                Operation:
                    - False to add.
                    - True to remove.

            hosts: list
                List of strings, where each string represents a hostname to add or remove.

        :return:
                - In case of add, the already existing hosts (as a list) or a string if only one.
                - In case of remove, the non existing hosts (as a list) or a string if only one.
                - None otherwise.
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'op', 'arg': op, 't': bool, 'must': True},
                    {'name': 'hosts', 'arg': hosts, 't': list, 'must': True}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client(cmd='mdns_c_add_remove_hosts', c_key=c_key, op=op, hosts=hosts)

    @client_api('getter', True)
    def get_client_hosts(self, c_key):
        """ Get hostnames of a mDNS client.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`

        :return:
            Hostnames the client owns:
                - List of strings in case there are many
                - String in case there is only one hostname
                - None in case there are no hostnames.
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client(cmd='mdns_c_get_hosts', c_key=c_key)

    @client_api('getter', True)
    def get_cache(self, ns_key):
        """ Get resolved hosts (hostnames) cache from a mDNS namespace.

        :parameters:
            ns_key: EMUNamespaceKey
                see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`

        :returns:
            List of dictionaries

                 .. highlight:: python
                 .. code-block:: python

                    [
                        {
                            'dns_class': 'IN',
                            'dns_type': 'A',
                            'answer': '1.1.1.6',
                            'name': 'trex',
                            'ttl': 240,
                            'time_left': 238
                        },
                        {
                            'dns_class': 'IN',
                            'dns_type': 'A',
                            'answer': '1.1.1.5',
                            'name': 'UCS',
                            'ttl': 240,
                            'time_left': 211
                        }
                    ]
        """
        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey, 'must': True}]
        EMUValidator.verify(ver_args)
        params = ns_key.conv_to_dict(add_tunnel_key = True)
        return self.emu_c._get_n_items(cmd = 'mdns_ns_cache_iter', **params)

    @client_api('command', True)
    def flush_cache(self, ns_key):
        """ Flush mDNS namespace cache of resolved hostnames.

        :parameters:
            ns_key: EMUNamespaceKey
                see :class:`trex.emu.trex_emu_profile.EMUNamespaceKey`
        """

        ver_args = [{'name': 'ns_key', 'arg': ns_key, 't': EMUNamespaceKey, 'must': True}]
        EMUValidator.verify(ver_args)
        self.emu_c._send_plugin_cmd_to_ns(cmd="mdns_ns_cache_flush", ns_key=ns_key)

    # Plugins methods
    @plugin_api('mdns_show_c_counters', 'emu')
    def mdns_show_client_counters_line(self, line):
        """Show mDNS client counters.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "mdns_show_c_counters_line",
                                        self.mdns_show_client_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True

    @plugin_api('mdns_show_ns_counters', 'emu')
    def mdns_show_ns_counters_line(self, line):
        '''Show mDNS namespace counters.\n'''
        parser = parsing_opts.gen_parser(self,
                                        "mdns_show_ns_counters_line",
                                        self.mdns_show_ns_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_ALL_NS,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.ns_data_cnt, opts, req_ns = True)
        return True

    @plugin_api('mdns_query', 'emu')
    def mdns_query_line(self, line):
        """Query mDNS.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "mdns_query",
                                        self.mdns_query_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.DNS_QUERY_NAME,
                                        parsing_opts.DNS_QUERY_TYPE,
                                        parsing_opts.DNS_QUERY_CLASS,
                                        parsing_opts.MDNS_QUERY_IPV6
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        query = {
            "name": opts.name,
            "dns_type": opts.dns_type,
            "dns_class": opts.dns_class,
            "ipv6": opts.ipv6
        }
        self.query(c_key, queries=[query])
        self.logger.post_cmd(True) # If we got here, it was successful

    @plugin_api('mdns_add_hosts', 'emu')
    def mdns_add_hosts_line(self, line):
        """Add hosts to mDNS client.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "mdns_add_hosts",
                                        self.mdns_add_hosts_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.MDNS_HOSTS_LIST
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        existing_hosts = self.add_remove_hosts(c_key=c_key, op=False, hosts=opts.hosts)
        if existing_hosts:
            msg = "Hosts: {} were not added because they already exist.".format(listify(existing_hosts))
            text_tables.print_colored_line(msg, "yellow")
        self.logger.post_cmd(True) # If we got here, it was successful

    @plugin_api('mdns_remove_hosts', 'emu')
    def mdns_remove_hosts_line(self, line):
        """Remove hosts from mDNS client.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "mdns_remove_hosts",
                                        self.mdns_remove_hosts_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.MDNS_HOSTS_LIST
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        non_existing_hosts = self.add_remove_hosts(c_key=c_key, op=True, hosts=opts.hosts)
        if non_existing_hosts:
            msg = "Hosts: {} didn't exist.".format(listify(non_existing_hosts))
            text_tables.print_colored_line(msg, "yellow")
        self.logger.post_cmd(True) # If we got here, it was successful

    @plugin_api('mdns_show_hosts', 'emu')
    def mdns_show_hosts_line(self, line):
        """Show hosts of mDNS client.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "mdns_show_hosts",
                                        self.mdns_show_hosts_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        hosts = self.get_client_hosts(c_key=c_key)
        if hosts:
            hosts = listify(hosts)
            keys_to_headers = [ {'key': 'number',       'header': 'Number'},
                                {'key': 'hostname',     'header': 'Hostname'},
            ]
            res = [{'number': i+1, 'hostname': host} for i, host in enumerate(hosts)]
            self.print_table_by_keys(res, keys_to_headers, title="Hostnames")
        else:
            text_tables.print_colored_line("The client doesn't own any hostname.", "yellow")

    @plugin_api('mdns_show_cache', 'emu')
    def mdns_show_cache_line(self, line):
        """Show resolved hostnames in mDNS namespace\n"""
        parser = parsing_opts.gen_parser(self,
                                        "mdns_show_cache",
                                        self.mdns_show_cache_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )
        opts = parser.parse_args(line.split())
        keys_to_headers = [{'key': 'name',         'header': 'Hostname'},
                           {'key': 'answer',       'header': 'Answer'},
                           {'key': 'dns_type',     'header': 'Type'},
                           {'key': 'dns_class',    'header': 'Class'},
                           {'key': 'ttl',          'header': 'Time To Live'},
                           {'key': 'time_left',    'header': 'Time Left'},
        ]
        args = {'title': 'mDNS Cache', 'empty_msg': 'No entries in mDNS cache.', 'keys_to_headers': keys_to_headers}
        if opts.all_ns:
            self.run_on_all_ns(self.get_cache, print_ns_info = True, func_on_res = self.print_table_by_keys, func_on_res_args = args)
        else:
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            res = self.get_cache(ns_key)
            self.print_table_by_keys(data = res, **args)

    @plugin_api('mdns_flush_cache', 'emu')
    def mdns_flush_cache_line(self, line):
        """Flush mDNS namespace cache.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "mdns_flush_cache",
                                        self.mdns_flush_cache_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP_NOT_REQ,
                                        parsing_opts.EMU_ALL_NS
                                        )
        opts = parser.parse_args(line.split())
        if opts.all_ns:
            self.run_on_all_ns(self.flush_cache)
        else:
            ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
            self.flush_cache(ns_key)
        self.logger.post_cmd(True)
