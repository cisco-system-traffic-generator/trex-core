from ..common.trex_exceptions import *
from ..utils.common import *
from ..utils.text_tables import TRexTextTable, print_table_with_header
from ..utils import parsing_opts



class MacsIpsMngr(object):
    def __init__(self, client, macs = None, ips = None):
        self.client = client
        self.black_list_macs = macs or set()
        self.black_list_ip = ips or set()
        self.max_size = 1000


    def add_macs_list(self, mac_list, is_str=True):
        if len(self.black_list_macs) + len(mac_list) > 1000:
            raise TRexError("The maximum size of mac's black list is: %s" % (self.max_size))
        for mac in mac_list:
            if is_str:
                mac_str = mac
                if ":" in mac:
                    mac_str = mac2str(mac_str)
                mac_addr = mac_str_to_num(mac_str)
            else:
                if type(mac) != int:
                    raise TRexError("The Mac type is not int")
                mac_addr = mac
            self.black_list_macs.add(mac_addr)


    def _remove_mac_list(self, mac_list, is_str=True):
        for mac in mac_list:
            mac_addr = mac
            if is_str:
                if ":" in mac:
                    mac_addr = mac2str(mac)
                mac_addr = mac_str_to_num(mac_addr)
            if mac_addr not in self.black_list_macs:
                raise TRexError("The list does not contain MAC address: %s" % (int2mac(mac_addr)))
            self.black_list_macs.remove(mac_addr)


    def _clear_mac_str_list(self):
        self.black_list_macs.clear()


    def _upload_mac_list(self):
        json_macs = []
        for mac in self.black_list_macs:
            json_macs.append({'mac_addr' : mac})
        params = {'mac_list' : json_macs}
        self.client.ctx.logger.pre_cmd("setting ignored mac list" + '.\n')
        rc = self.client._transmit("set_ignored_macs", params = params)
        self.client.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())
        return rc


    def _mac_list_to_str(self, mac_list):
        str_mac_list = []
        for mac in mac_list:
            str_mac_list.append(int2mac(mac))
        return str_mac_list


    def _get_mac_list_from_server(self):
        rc = self.client._transmit("get_ignored_macs")
        self.client.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())
        data = rc.data()
        mac_list = []
        for mac_dict in data:
            for _ , mac in mac_dict.items():
                mac_list.append(mac)
        return mac_list


    def get_mac_list(self, from_server=True, to_str=True):
        if from_server:
            mac_list =  self._get_mac_list_from_server()
        else:
            mac_list = list(self.black_list_macs)
        if to_str:
            mac_list = self._mac_list_to_str(mac_list)
        return mac_list


    def add_ips_list(self, ips_list, is_str=True):
        if len(self.black_list_ip) + len(ips_list) > 1000:
            raise TRexError("The maximum size of IP addresses black list is: %s" % (self.max_size))
        for ip in ips_list:
            ip_addr = ip
            if is_str:
                ip_addr = ip2int(ip)
            else:
                if type(ip) != int:
                    raise TRexError("The IP type is not int")
            self.black_list_ip.add(ip_addr)


    def _remove_ip_list(self, ip_list, is_str=True):
        for ip in ip_list:
            if is_str:
                ip_addr = ip2int(ip)
            else:
                ip_addr = ip
            if ip_addr not in self.black_list_ip:
                raise TRexError("The list does not contain IPv4 address: %s" % (int2ip(ip_addr)))
            self.black_list_ip.remove(ip_addr)


    def _clear_ips_list(self):
        self.black_list_ip.clear()


    def _upload_ips_list(self):
        json_ips = []
        for ip in self.black_list_ip:
            json_ips.append({'ip' : ip})
        params = {'ip_list' : json_ips}
        self.client.ctx.logger.pre_cmd("setting ignored ip list" + '.\n')
        rc = self.client._transmit("set_ignored_ips", params = params)
        self.client.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())
        return rc


    def _ip_list_to_str(self, ip_list):
        str_ip_list = []
        for ip in ip_list:
            str_ip_list.append(int2ip(ip))
        return str_ip_list


    def _get_ips_list_from_server(self):
        rc = self.client._transmit("get_ignored_ips")
        if not rc:
            raise TRexError(rc.err())
        data = rc.data()
        ip_list = []
        for ip_dict in data:
            for _ , ip in ip_dict.items():
                ip_list.append(ip)
        return ip_list


    def get_ips_list(self, from_server=True, to_str=True):
        if from_server:
            ip_list = self._get_ips_list_from_server()
        else:
            ip_list = list(self.black_list_ip)
        if to_str:
            ip_list = self._ip_list_to_str(ip_list)
        return ip_list


    def _flush_all(self):
        self._upload_mac_list()
        self._upload_ips_list()


    def _clear_all(self):
        self._clear_mac_str_list()
        self._clear_ips_list()


    def set_ignored_macs_ips(self, mac_list = None, ip_list=None, upload_to_server=True, is_str=True, to_override=True):
        if to_override:
            self._clear_all()
        rc = True
        if mac_list:
            self.add_macs_list(mac_list, is_str=is_str)
        if ip_list:
            self.add_ips_list(ip_list, is_str=is_str)
        if upload_to_server:
            rc = self._flush_all()
        return rc


    def _remove_ignored_macs_ips(self, mac_list = None, ip_list=None, upload_to_server=False, is_str=True):
        rc = True
        if mac_list:
            self._remove_mac_list(mac_list, is_str=is_str)
        if ip_list:
            self._remove_ip_list(ip_list, is_str=is_str)
        if upload_to_server:
            rc = self._flush_all()
        return rc


    def _show_macs_table(self):
        server_mac_set = set(self._get_mac_list_from_server())
        table_name = "Mac's black list"
        if server_mac_set != self.black_list_macs:
            table_name += " (Not sync with server)"
        else:
            table_name += " (Sync with server)"
        macs_table = TRexTextTable(table_name)
        macs_table.set_cols_align(['c'] * 3)
        macs_table.set_cols_dtype(['t'] * 3)
        macs_table.header(['Mac_start', 'Mac_end', 'Is-Sync'])

        sorted_mac_list = sorted(self.black_list_macs)
        start_mac = None
        end_mac = None
        is_mac_sync = True
        max_len = len("Mac_start")
        length = len(sorted_mac_list)
        for idx, mac in enumerate(sorted_mac_list):
            if mac not in server_mac_set:
                is_mac_sync = False
            if not start_mac:
                start_mac = mac
            if idx != length - 1:
                next_mac = sorted_mac_list[idx + 1]
            else:
                next_mac = mac + 2
            if next_mac - mac > 1:
                end_mac = mac
                start_mac_str = int2mac(start_mac)
                end_mac_str = int2mac(end_mac)
                macs_table.add_row([start_mac_str, end_mac_str, is_mac_sync])
                max_len = max (max_len, max(len(start_mac_str), len(end_mac_str)))
                is_mac_sync = True
                start_mac = None

        macs_table.set_cols_width([max_len, max_len, 7])
        print_table_with_header(macs_table, untouched_header = macs_table.title, buffer = sys.stdout)


    def _show_ips_table(self):
        server_ip_set = set(self._get_ips_list_from_server())
        table_name = "IP's black list"
        if server_ip_set != self.black_list_ip:
            table_name += " (Not sync with server)"
        else:
            table_name += " (Sync with server)"
        ips_table = TRexTextTable(table_name)
        ips_table.set_cols_align(['c'] * 3)
        ips_table.set_cols_dtype(['t'] * 3)
        ips_table.header(['IP_start', 'IP_end', 'Is-Sync'])

        sorted_ip_list = sorted(self.black_list_ip)
        start_ip = None
        end_ip = None
        is_ip_sync = True
        max_len = len("IP_start")
        length = len(sorted_ip_list)
        for idx, ip in enumerate(sorted_ip_list):
            if ip not in server_ip_set:
                is_ip_sync = False
            if not start_ip:
                start_ip = ip
            if idx != length - 1:
                next_ip = sorted_ip_list[idx + 1]
            else:
                next_ip = ip + 2
            if next_ip - ip > 1:
                end_ip = ip
                start_ip_str = int2ip(start_ip)
                end_ip_str = int2ip(end_ip)
                ips_table.add_row([start_ip_str, end_ip_str, is_ip_sync])
                max_len = max (max_len, max(len(start_ip_str), len(end_ip_str)))
                is_ip_sync = True
                start_ip = None

        ips_table.set_cols_width([max_len, max_len, 7])
        print_table_with_header(ips_table, untouched_header = ips_table.title, buffer = sys.stdout)


    def sync_with_server(self):
        self._clear_all()
        self.black_list_ip.update(self._get_ips_list_from_server())
        self.black_list_macs.update(self._get_mac_list_from_server())


    def _show(self):
        self._show_macs_table()
        self._show_ips_table()