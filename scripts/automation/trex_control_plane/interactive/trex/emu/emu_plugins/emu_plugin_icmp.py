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

    MIN_ROWS = 5
    MIN_COLS = 120

    def __init__(self, emu_client):
        super(ICMPPlugin, self).__init__(emu_client, ns_cnt_rpc_cmd='icmp_ns_cnt')

    # API methods
    @client_api('getter', True)
    @update_docstring(EMUPluginBase._get_ns_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def get_counters(self, ns_key, cnt_filter=None, zero=True, verbose=True):
        return self._get_ns_counters(ns_key, cnt_filter, zero, verbose)

    @client_api('command', True)
    @update_docstring(EMUPluginBase._clear_ns_counters.__doc__.replace("$PLUGIN_NAME", plugin_name))
    def clear_counters(self, ns_key):
        return self._clear_ns_counters(ns_key)

    @client_api('command', True)
    def start_ping(self, c_key, amount=None, pace=None, dst=None, timeout=None, payload_size=None):
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
                    Destination IPv4. For example: [1, 1, 1, 3]

                timeout: int
                    Time to collect the results in seconds, starting when the last Echo Request is sent.

                payload_size: int
                    Size of the ICMP payload, in bytes.

            :returns:
                (RPC Response, TRexError), one of the entries is None.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'amount', 'arg': amount, 't': int, 'must': False},
                    {'name': 'pace', 'arg': pace, 't': float,'must': False},
                    {'name': 'dst', 'arg': dst, 't': 'ipv4', 'must': False},
                    {'name': 'timeout', 'arg': timeout, 't': int, 'must': False},
                    {'name': 'payload_size', 'arg': payload_size, 't': int, 'must': False}]
        EMUValidator.verify(ver_args)
        try:
            success = self.emu_c._send_plugin_cmd_to_client(cmd='icmp_c_start_ping', c_key=c_key, amount=amount, pace=pace,
                                                            dst=dst, timeout=timeout, payloadSize=payload_size)
            return success, None
        except TRexError as err:
            return None, err

    @client_api('command', True)
    def get_ping_stats(self, c_key, zero=True):
        """
            Get the stats of an active ping.

            :parameters:
                c_key: EMUClientKey
                    see :class:`trex.emu.trex_emu_profile.EMUClientKey`

                zero: boolean
                    Get values that equal zero aswell.

            :returns:
                (RPC Response, TRexError), one of the entries is None.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True},
                    {'name': 'zero', 'arg': zero, 't': bool, 'must': False}]
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
                (RPC Response, TRexError), one of the entries is None.

        """
        ver_args = [{'name': 'c_key', 'arg': c_key, 't': EMUClientKey, 'must': True}]
        EMUValidator.verify(ver_args)
        try:
            success = self.emu_c._send_plugin_cmd_to_client('icmp_c_stop_ping', c_key=c_key)
            return success, None
        except TRexError as err:
            return None, err

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
        self.emu_c._base_show_counters(self.ns_data_cnt, opts, req_ns=True)
        return True

    @plugin_api('icmp_ping', 'emu')
    def icmp_ping(self, line):
        """ICMP ping utility (per client).\n"""
        parser = parsing_opts.gen_parser(self,
                                         "icmp_ping",
                                         self.icmp_ping.__doc__,
                                         parsing_opts.MAC_ADDRESS,
                                         parsing_opts.EMU_NS_GROUP,
                                         parsing_opts.EMU_ICMP_PING_PARAMS,
                                         )

        rows, cols = os.popen('stty size', 'r').read().split()
        if (int(rows) < self.MIN_ROWS) or (int(cols) < self.MIN_COLS):
            msg = "\nPing requires console screen size of at least {0}x{1}, current is {2}x{3}".format(self.MIN_COLS,
                                                                                                    self.MIN_ROWS,
                                                                                                    cols,
                                                                                                    rows)
            text_tables.print_colored_line(msg, 'red', buffer=sys.stdout)
            return

        opts = parser.parse_args(line.split())
        ns_key = EMUNamespaceKey(opts.port, opts.vlan, opts.tpid)
        c_key = EMUClientKey(ns_key, opts.mac)

        self.emu_c.logger.pre_cmd('Starting to ping : ')
        success, err = self.start_ping(c_key=c_key, amount=opts.ping_amount, pace=opts.ping_pace, dst=opts.ping_dst,
                                       payload_size=opts.ping_size, timeout=1)
        if err is not None:
            self.emu_c.logger.post_cmd(False)
            text_tables.print_colored_line(err.msg, 'yellow', buffer=sys.stdout)
        else:
            self.emu_c.logger.post_cmd(True)
            amount = opts.ping_amount if opts.ping_amount is not None else 5  # default is 5 packets
            try:
                while True:
                    time.sleep(1) # Don't get statistics too often as RPC requests might overload the Emu Server.
                    stats, err = self.get_ping_stats(c_key=c_key)
                    if err is None:
                        stats = stats['icmp_ping_stats']
                        sent = stats['requestsSent']
                        rcv = stats['repliesInOrder']
                        percent = sent / float(amount) * 100.0
                        # the latency from the server is in usec
                        min_lat_msec = float(stats['minLatency']) / 1000
                        max_lat_msec = float(stats['maxLatency']) / 1000
                        avg_lat_msec = float(stats['avgLatency']) / 1000
                        err = int(stats['repliesOutOfOrder']) + int(stats['repliesMalformedPkt']) + \
                              int(stats['repliesBadLatency']) + int(stats['repliesBadIdentifier']) + \
                              int (stats['dstUnreachable'])

                        text = "Progress: {0:.2f}%, Sent: {1}/{2}, Rcv: {3}/{2}, Err: {4}/{2}, RTT min/avg/max = {5:.2f}/{6:.2f}/{7:.2f} ms" \
                                                            .format(percent, sent, amount, rcv, err, min_lat_msec, avg_lat_msec, max_lat_msec)

                        sys.stdout.write('\r' + (' ' * self.MIN_COLS) +'\r')  # clear line
                        sys.stdout.write(format_text(text, 'yellow'))
                        sys.stdout.flush()
                        if sent == amount:
                            sys.stdout.write(format_text('\n\nCompleted\n\n', 'yellow'))
                            sys.stdout.flush()
                            break
                    else:
                        # Trying to collect stats after completion.
                        break
            except KeyboardInterrupt:
                text_tables.print_colored_line('\nInterrupted by a keyboard signal (probably ctrl + c).', 'yellow', buffer=sys.stdout)
                self.emu_c.logger.pre_cmd('Attempting to stop ping : ')
                success, err = self.stop_ping(c_key=c_key)
                if err is None:
                    self.emu_c.logger.post_cmd(True)
                else:
                    self.emu_c.logger.post_cmd(False)
                    text_tables.print_colored_line(err.msg, 'yellow', buffer=sys.stdout)



