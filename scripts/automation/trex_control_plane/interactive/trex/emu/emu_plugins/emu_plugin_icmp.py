from trex.emu.api import *
from trex.emu.emu_plugins.emu_plugin_base import *
from trex.emu.trex_emu_counters import *
import trex.utils.parsing_opts as parsing_opts
from trex.common.trex_exceptions import *
from trex.utils import text_tables
import json


class ICMPPlugin(EMUPluginBase):
    '''Defines ICMP plugin'''

    plugin_name = 'ICMP'

    # init jsons example for SDK
    INIT_JSON_NS = {'icmp': {}}
    """
    :parameters:
        Empty.
    """

    INIT_JSON_CLIENT = {'icmp': {}}
    """
    :parameters:
        Empty.
    """

    def __init__(self, emu_client):
        super(ICMPPlugin, self).__init__(emu_client, 'icmp_ns_cnt')

    def __icmp_start_ping_print(self, c_key, amount, pace, dst, timeout, pkt_size):
        res = False
        self.emu_c.logger.pre_cmd('Starting to ping : ')
        res = self.start_ping(c_key=c_key, amount=amount, pace=pace, dst=dst, timeout=timeout, pkt_size=pkt_size)
        self.emu_c.logger.post_cmd(res)
        if not res:
            text_tables.print_colored_line("Client is already pinging on in timeout.", 'yellow', buffer=sys.stdout)
        return res


    @client_api('command', True)
    def start_ping(self, c_key, amount=None, pace=None, dst=None, timeout=None, pkt_size=None):
        """
            Start pinging, sending Echo Requests.

            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`

                amount: int
                    Amount of Echo Requests to send.

                pace: float
                    Pace in which to send the packets in pps (packets per second).

                dst: list of bytes
                    Destination IPv4. [1, 1, 1, 3]

                timeout: int
                    Time to collect the results in seconds, starting when the last Echo Request is sent.

                pkt_size: int
                    Size of the Echo Request packet, in bytes.

            :returns:
                Boolean representing the status of the command.

            :raises:
                + :exc:`TRexError`
                    In case of failure.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'amount', 'arg': amount, 't': int, 'must': False},
                    {'name': 'pace', 'arg': pace, 't': float,'must': False},
                    {'name': 'dst', 'arg': dst, 't': 'ipv4', 'must': False},
                    {'name': 'timeout', 'arg': timeout, 't': int, 'must': False},
                    {'name': 'pkt_size', 'arg': pkt_size, 't': int, 'must': False}]


        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client(cmd='icmp_c_start_ping', c_key=c_key, amount=amount, pace=pace, 
                                                     dst=dst, timeout=timeout, pktSize=pkt_size)

    @client_api('command', True)
    def get_ping_stats(self, c_key, zero=True):
        """
            Start pinging, sending Echo Requests.

            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`

                zero: boolean
                    Get values that equal zero aswell.

            :returns:
                (RPC answer, Error), one of the entries is None.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'zero', 'arg': zero, 't': bool, '': False}]
        EMUValidator.verify(ver_args)
        try:
            data = self.emu_c._send_plugin_cmd_to_client(cmd='icmp_c_get_ping_stats', c_key=c_key, zero=zero)
            return data, None
        except TRexError as err:
            return None, err

    @client_api('command', True)
    def stop_ping(self, c_key):
        """
            Stop an ongoing ping.

            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`

            :returns:
                Boolean representing the status of the command.

            :raises:
                + :exc:`TRexError`
                    In case of failure.
        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True}]
        EMUValidator.verify(ver_args)
        return self.emu_c._send_plugin_cmd_to_client('icmp_c_stop_ping', c_key=c_key)

    # Plugins methods
    @plugin_api('icmp_show_counters', 'emu')
    def icmp_show_counters_line(self, line):
        '''Show Icmp counters (per client).\n'''
        parser = parsing_opts.gen_parser(self,
                                         "show_counters_icmp",
                                         self.icmp_show_counters_line.__doc__,
                                         parsing_opts.EMU_SHOW_CNT_GROUP,
                                         parsing_opts.EMU_NS_GROUP,
                                         parsing_opts.EMU_DUMPS_OPT
                                         )

        opts = parser.parse_args(line.split())
        self.emu_c._base_show_counters(self.data_c, opts, req_ns=True)
        return True

    @plugin_api('icmp_start_ping', 'emu')
    def icmp_start_ping(self, line):
        """ICMP start pinging (per client).\n"""
        parser = parsing_opts.gen_parser(self,
                                         "icmp_start_ping",
                                         self.icmp_start_ping.__doc__,
                                         parsing_opts.MAC_ADDRESS,
                                         parsing_opts.EMU_NS_GROUP,
                                         parsing_opts.EMU_ICMP_PING_PARAMS
                                         )
        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)

        self.__icmp_start_ping_print(c_key=c_key, amount=opts.ping_amount, pace=opts.ping_pace, dst=opts.ping_dst,
                            timeout=opts.ping_timeout, pkt_size=opts.ping_size)

    @plugin_api('icmp_start_ping_wait', 'emu')
    def icmp_start_ping_wait(self, line):
        """ICMP start pinging and wait to finish (per client).\n"""
        parser = parsing_opts.gen_parser(self,
                                         "icmp_start_ping_wait",
                                         self.icmp_start_ping_wait.__doc__,
                                         parsing_opts.MAC_ADDRESS,
                                         parsing_opts.EMU_NS_GROUP,
                                         parsing_opts.EMU_ICMP_PING_PARAMS,
                                         parsing_opts.MONITOR_TYPE_VERBOSE
                                         )

        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        res = self.__icmp_start_ping_print(c_key=c_key, amount=opts.ping_amount, pace=opts.ping_pace, dst=opts.ping_dst,
                            timeout=opts.ping_timeout, pkt_size=opts.ping_size)
        amount = opts.ping_amount if opts.ping_amount is not None else 5  # default is 5 packets
        pace = opts.ping_pace if opts.ping_pace is not None else 1  # default is 1
        if res:
            try:
                while True:
                    time.sleep(min(1, 100/pace))
                    stats, err = self.get_ping_stats(c_key=c_key)
                    if err is None:
                        completed = stats['icmp_ping_stats']['requestsSent']
                        percent = completed / float(amount) * 100.0
                        dots = '.' * int(percent / 10)
                        text = "Progress: {0:.2f}% {1}".format(percent, dots)
                        sys.stdout.write('\r' + (' ' * 25) +
                                         '\r')  # clear line
                        sys.stdout.write(format_text(text, 'yellow'))
                        sys.stdout.flush()
                        if opts.verbose:
                            sys.stdout.flush()
                            stats_text = "\n"
                            for key, value in stats['icmp_ping_stats'].items():
                                stats_text += "{0} {1}: {2:.2f}\n".format(
                                    key, ' ' * (20 - len(key)), value)
                            sys.stdout.write(format_text(stats_text, 'green'))
                            sys.stdout.flush()
                        if completed == amount:
                            sys.stdout.write(format_text(
                                '\n\nCompleted\n\n', 'yellow'))
                            sys.stdout.flush()
                            break
                    else:
                        # Could get here if timeout is too short, but shouldn't happen.
                        break
            except KeyboardInterrupt:
                text_tables.print_colored_line(
                    '\nInterrupted by a keyboard signal (probably ctrl + c).', 'yellow', buffer=sys.stdout)
                self.emu_c.logger.pre_cmd('Attempting to stop ping :')
                res_stop = self.stop_ping(c_key=c_key)
                self.emu_c.logger.post_cmd(res_stop)
                return res_stop

    @plugin_api('icmp_get_ping_stats', 'emu')
    def icmp_get_ping_stats(self, line):
        """ICMP get ping stats (per client).\n"""
        parser = parsing_opts.gen_parser(self,
                                         "icmp_get_ping_stats",
                                         self.icmp_get_ping_stats.__doc__,
                                         parsing_opts.MAC_ADDRESS,
                                         parsing_opts.EMU_NS_GROUP,
                                         parsing_opts.EMU_SHOW_CNT_GROUP,
                                         parsing_opts.EMU_DUMPS_OPT
                                         )

        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        data_cnt = DataCounter(self.emu_c.conn, 'icmp_c_get_ping_stats')
        data_cnt.set_add_data(c_key=c_key)
        try:
            if opts.headers:
                data_cnt.get_counters_headers()
            elif opts.clear:
                self.emu_c.logger.pre_cmd("Clearing all counters :")
                data_cnt.clear_counters()
                self.emu_c.logger.post_cmd(True)
            else:
                data = data_cnt.get_counters(
                    opts.tables, opts.cnt_types, opts.zero)
                DataCounter.print_counters(
                    data, opts.verbose, opts.json, opts.yaml)
        except TRexError as err:
            text_tables.print_colored_line(
                '\n' + err.msg, 'yellow', buffer=sys.stdout)

    @plugin_api('icmp_stop_ping', 'emu')
    def icmp_stop_ping(self, line):
        """ICMP stop pinging (per client).\n"""
        parser = parsing_opts.gen_parser(self,
                                         "icmp_stop_ping",
                                         self.icmp_stop_ping.__doc__,
                                         parsing_opts.MAC_ADDRESS,
                                         parsing_opts.EMU_NS_GROUP,
                                         )

        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)
        self.emu_c.logger.pre_cmd('Attempting to stop ping :')
        res = self.stop_ping(c_key=c_key)
        self.emu_c.logger.post_cmd(res)
        if not res:
            text_tables.print_colored_line("No active pinging.", 'yellow', buffer=sys.stdout)
