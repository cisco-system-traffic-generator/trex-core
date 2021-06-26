from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_validator import EMUValidator
import trex.utils.parsing_opts as parsing_opts
from trex.utils import text_tables
from trex.common.trex_types import listify


class DNSPlugin(EMUPluginBase):
    """
        Defines a DNS - Domain Name System plugin based on `DNS <https://en.wikipedia.org/wiki/Domain_Name_System>`_ 

        Implemented based on `RFCs 1034, 1035 <https://datatracker.ietf.org/doc/html/rfc1035>`_ 
    """

    plugin_name = 'DNS'

    # init json examples for SDK
    INIT_JSON_NS = None
    """
    There is currently no NS init json for DNS.
    """

    INIT_JSON_CLIENT = { 'dns': "Pointer to INIT_JSON_CLIENT below" }
    """
    :parameters:

        name_server: bool
            Indicate if client is represents a DNS Name Server. Name Servers answer queries based on a database.
            Default is False.

        dns_server_ip: string
            IPv4 or IPv6 of a DNS Name Server. Provide this only if the client is **not** a Name Server.

        database: Dict[str, list]
            Dictionary that represents the Name Server database. Provide this only if the client is a Name Server.

            The keys are domain that can be queried. The database can have multiple answers for each domain,
            and these answers are aggregated in a list.

            Each list is comprised of Dns entries, which are dictionaries.

            :Dns Entry:

                type: string

                    Dns Type. Supported values at the moment are A, AAAA, PTR and TXT. Defaults to *A*.

                class: string

                    Dns Class. Defaults to *IN*.

                answer: string

                    Answer for the domain if a query arrives with the correct type and class.

            A simple database looks like this:

            .. highlight:: python
            .. code-block:: python

                "database": {
                    "trex-tgn.cisco.com": [
                        {
                            "type": "A",
                            "class": "IN",
                            "answer": "173.36.109.208"
                        }
                    ],
                    "cisco.com": [
                        {
                            "type": "A",
                            "class": "IN",
                            "answer": "72.163.4.185"
                        },
                        {
                            "type": "A",
                            "class": "IN",
                            "answer": "72.163.4.161"
                        },
                        {
                            "type": "AAAA",
                            "class": "IN",
                            "answer": "2001:420:1101:1::185"
                        }
                    ]
                }
    """

    def __init__(self, emu_client):
        """
        Initialize a DNSPlugin.

            :parameters:
                emu_client: :class:`trex.emu.trex_emu_client.EMUClient`
                    Valid EMU client.
        """
        super(DNSPlugin, self).__init__(emu_client, client_cnt_rpc_cmd='dns_c_cnt')

    # API methods
    @client_api('getter', True)
    @update_docstring(EMUPluginBase._get_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def get_client_counters(self, c_key, cnt_filter=None, zero=True, verbose=True):
        return self._get_client_counters(c_key, cnt_filter, zero, verbose)

    @client_api('command', True)
    @update_docstring(EMUPluginBase._clear_client_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def clear_client_counters(self, c_key):
        return self._clear_client_counters(c_key)

    @client_api('command', True)
    def query(self, c_key, queries):
        """Send a Dns query from a Dns Resolver to the Dns Name Server specified in the resolver's init json.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`

            queries: list
                List of query, where each query is represented by:

                :query:

                    name: string
                        Name of the host we are querying.

                    dns_type: string
                        Type of the Dns query, supported types are A, AAAA, PTR, TXT. Default is *A*.

                    dns_class: string
                        Class of the Dns query. Default is *IN*.

        :raises:
            TRexError
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'queries', 'arg': queries, 't': list, 'must': True}]
        EMUValidator.verify(ver_args)
        self.emu_c._send_plugin_cmd_to_client(cmd='dns_c_query', c_key=c_key, queries=queries)

    @client_api('getter', True)
    def get_domains(self, c_key):
        """Get registered domains in a DNS Name Server.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`.
                Note that it must be a Name Server.


        :return:
            List of string, where each string is a domain.
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client(cmd='dns_c_get_domains', c_key=c_key)

    @client_api('getter', True)
    def get_domain_entries(self, c_key, domain):
        """ Get Dns entries for a domain from a DNS Name Server.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`.
                Note that it must be a Name Server.

            domain: str
                Domain whose entries we are looking to get.

        :return:
            List of dictionaries, where each dictionary is a Dns Entry.

                 .. highlight:: python
                 .. code-block:: python

                    [
                        {
                            "type": "A",
                            "class": "IN",
                            "answer": "72.163.4.161"
                        },
                        {
                            "type": "AAAA",
                            "class": "IN",
                            "answer": "2001:420:1101:1::185"
                        }
                    ]
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'domain', 'arg': domain, 't': str, 'must': True}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client(cmd='dns_c_get_domain_entries', c_key=c_key, domain=domain)

    @client_api('command', True)
    def add_domain_entries(self, c_key, domain, entries):
        """ Add domain entries to a DNS Name Server.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`.
                Note that it must be a Name Server.

            domain: str
                Domain whose entries we are looking to add.

            entries: list
                List of Dns Entries.

                 .. highlight:: python
                 .. code-block:: python

                    [
                        {
                            "type": "A",
                            "class": "IN",
                            "answer": "72.163.4.161"
                        },
                        {
                            "type": "AAAA",
                            "class": "IN",
                            "answer": "2001:420:1101:1::185"
                        }
                    ]

        :return:
            None
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'domain', 'arg': domain, 't': str, 'must': True},
                    {'name': 'entries', 'arg': entries, 't': list, 'must': True}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client(cmd='dns_c_add_remove_domain_entries', c_key=c_key, op=False, domain=domain, entries=entries)

    @client_api('command', True)
    def remove_domain_entries(self, c_key, domain, entries):
        """ Remove domain entries from a DNS Name Server.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`.
                Note that it must be a Name Server.

            domain: str
                Domain whose entries we are looking to remove.

            entries: list
                List of Dns Entries.

                 .. highlight:: python
                 .. code-block:: python

                    [
                        {
                            "type": "A",
                            "class": "IN",
                            "answer": "72.163.4.161"
                        },
                        {
                            "type": "AAAA",
                            "class": "IN",
                            "answer": "2001:420:1101:1::185"
                        }
                    ]

        :return:
            None
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'domain', 'arg': domain, 't': str, 'must': True},
                    {'name': 'entries', 'arg': entries, 't': list, 'must': True}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client(cmd='dns_c_add_remove_domain_entries', c_key=c_key, op=True, domain=domain, entries=entries)

    @client_api('getter', True)
    def get_cache(self, c_key):
        """ Get resolved domains cache from a Dns Resolver.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`.
                Note that it must be a Dns Resolver (not Name Server).

        :returns:
            List of dictionaries

                 .. highlight:: python
                 .. code-block:: python

                    [
                        {
                            'dns_class': 'IN',
                            'dns_type': 'A',
                            'answer': '72.163.4.185',
                            'name': 'cisco.com',
                            'ttl': 240,
                            'time_left': 238
                        },
                        {
                            'dns_class': 'PTR',
                            'dns_type': 'A',
                            'answer': 'google.com',
                            'name': '8.8.8.8.in-addr.arpa',
                            'ttl': 40,
                            'time_left': 211
                        }
                    ]
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True}]
        EMUValidator.verify(ver_args)
        params = c_key.conv_to_dict(add_ns = True, to_bytes = True)
        return self.emu_c._get_n_items(cmd = 'dns_c_cache_iter', **params)

    @client_api('command', True)
    def flush_cache(self, c_key):
        """ Flush Dns Resolver cache of resolved domains.

        :parameters:
            c_key: EMUClientKey
                see :class:`trex.emu.trex_emu_profile.EMUClientKey`.
                Note that it must be a Dns Resolver (not Name Server).
        """

        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True}]
        EMUValidator.verify(ver_args)
        self.emu_c._send_plugin_cmd_to_client(cmd="dns_c_cache_flush", c_key=c_key)

    # Plugins methods
    @plugin_api('dns_show_counters', 'emu')
    def dns_show_client_counters_line(self, line):
        """Show DNS client counters.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "dns_show_counters",
                                        self.dns_show_client_counters_line.__doc__,
                                        parsing_opts.EMU_SHOW_CNT_GROUP,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.EMU_DUMPS_OPT
                                        )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.client_data_cnt, opts, req_ns = True)
        return True

    @plugin_api('dns_query', 'emu')
    def dns_query_line(self, line):
        """Dns Query for Dns Resolver.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "dns_query",
                                        self.dns_query_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.DNS_QUERY_NAME,
                                        parsing_opts.DNS_QUERY_TYPE,
                                        parsing_opts.DNS_QUERY_CLASS,
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        query = {
            "name": opts.name,
            "dns_type": opts.dns_type,
            "dns_class": opts.dns_class,
        }
        self.query(c_key, queries=[query])
        self.logger.post_cmd(True) # If we got here, it was successful

    @plugin_api('dns_show_domains', 'emu')
    def dns_show_domains_line(self, line):
        """Show domains a Name Server holds.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "dns_show_domains",
                                        self.dns_show_domains_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        domains = self.get_domains(c_key=c_key)
        if domains:
            domains = listify(domains)
            keys_to_headers = [ {'key': 'number',       'header': 'Number'},
                                {'key': 'domain',       'header': 'Domain'},
            ]
            res = [{'number': i+1, 'domain': domain} for i, domain in enumerate(domains)]
            self.print_table_by_keys(res, keys_to_headers, title="Domains")
        else:
            text_tables.print_colored_line("The Name Server doesn't recognize any domains.", "yellow")

    @plugin_api('dns_show_domain_entries', 'emu')
    def dns_show_domain_entries_line(self, line):
        """Show entries of Dns Domain in the Name Server.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "dns_show_domain_entries",
                                        self.dns_show_domain_entries_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        parsing_opts.DNS_DOMAIN_NAME,
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        entries = self.get_domain_entries(c_key=c_key, domain=opts.domain)
        # We filter out TXTs because the table should be kept compact.
        entries = [entry for entry  in entries if entry["type"] != "TXT"]
        keys_to_headers = [ {'key': 'answer', 'header': 'Answer'},
                            {'key': 'type',   'header': 'Type'},
                            {'key': 'class',  'header': 'Class'},
        ]
        args = {'title': '{} entries'.format(opts.domain), 'empty_msg': 'No entries for {}'.format(opts.domain), 'keys_to_headers': keys_to_headers}
        self.print_table_by_keys(data = entries, **args)

    @plugin_api('dns_show_cache', 'emu')
    def dns_show_cache_line(self, line):
        """Show resolved domains for a Dns Resolver\n"""
        parser = parsing_opts.gen_parser(self,
                                        "dns_show_cache",
                                        self.dns_show_cache_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        )
        opts = parser.parse_args(line.split())
        keys_to_headers = [{'key': 'name',         'header': 'Domain'},
                           {'key': 'answer',       'header': 'Answer'},
                           {'key': 'dns_type',     'header': 'Type'},
                           {'key': 'dns_class',    'header': 'Class'},
                           {'key': 'ttl',          'header': 'Time To Live'},
                           {'key': 'time_left',    'header': 'Time Left'},
        ]
        args = {'title': 'Dns Cache', 'empty_msg': 'No entries in Dns cache.', 'keys_to_headers': keys_to_headers}
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        res = self.get_cache(c_key)
        self.print_table_by_keys(data = res, **args)

    @plugin_api('dns_flush_cache', 'emu')
    def dns_flush_cache_line(self, line):
        """Flush Dns Resolver cache.\n"""
        parser = parsing_opts.gen_parser(self,
                                        "dns_flush_cache",
                                        self.dns_flush_cache_line.__doc__,
                                        parsing_opts.EMU_NS_GROUP,
                                        parsing_opts.MAC_ADDRESS,
                                        )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        self.flush_cache(c_key)
        self.logger.post_cmd(True)
