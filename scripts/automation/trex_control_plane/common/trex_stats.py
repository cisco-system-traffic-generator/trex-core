#!/router/bin/python
from collections import namedtuple, OrderedDict
from client_utils import text_tables
from common.text_opts import format_text
from client.trex_async_client import CTRexAsyncStats
import copy

GLOBAL_STATS = 'g'
PORT_STATS = 'p'
PORT_STATUS = 'ps'
ALL_STATS_OPTS = {GLOBAL_STATS, PORT_STATS, PORT_STATUS}
ExportableStats = namedtuple('ExportableStats', ['raw_data', 'text_table'])


class CTRexInformationCenter(object):

    def __init__(self, username, connection_info, ports_ref, async_stats_ref):
        self.user = username
        self.connection_info = connection_info
        self.server_version = None
        self.system_info = None
        self._ports = ports_ref
        self._async_stats = async_stats_ref

    # def __getitem__(self, item):
    #     stats_obj = getattr(self, item)
    #     if stats_obj:
    #         return stats_obj.get_stats()
    #     else:
    #         return None

    def clear(self, port_id_list):
        self._async_stats.get_general_stats().clear()
        for port_id in port_id_list:
            self._async_stats.get_port_stats(port_id).clear()
        pass

    def generate_single_statistic(self, port_id_list, statistic_type):
        if statistic_type == GLOBAL_STATS:
            return self._generate_global_stats()
        elif statistic_type == PORT_STATS:
            return self._generate_port_stats(port_id_list)
            pass
        elif statistic_type == PORT_STATUS:
            return self._generate_port_status(port_id_list)
        else:
            # ignore by returning empty object
            return {}

    def _generate_global_stats(self):
        stats_obj = self._async_stats.get_general_stats()
        return_stats_data = \
            OrderedDict([("connection", "{host}, Port {port}".format(host=self.connection_info.get("server"),
                                                                     port=self.connection_info.get("sync_port"))),
                         ("version", "{ver}, UUID: {uuid}".format(ver=self.server_version.get("version", "N/A"),
                                                                  uuid="N/A")),
                         ("cpu_util", "{0}%".format(stats_obj.get("m_cpu_util"))),
                         ("total_tx", stats_obj.get("m_tx_bps", format=True, suffix="b/sec")),
                         ("total_rx", stats_obj.get("m_rx_bps", format=True, suffix="b/sec")),
                         ("total_pps", stats_obj.format_num(stats_obj.get("m_tx_pps") + stats_obj.get("m_rx_pps"),
                                                           suffix="pkt/sec")),
                         ("total_streams", sum([len(port.streams)
                                               for port in self._ports])),
                         ("active_ports", sum([port.is_active()
                                              for port in self._ports]))
                         ]
                        )

        # build table representation
        stats_table = text_tables.TRexTextInfo()
        stats_table.set_cols_align(["l", "l"])
        stats_table.add_rows([[k.replace("_", " ").title(), v]
                              for k, v in return_stats_data.iteritems()],
                             header=False)

        return {"global_statistics": ExportableStats(return_stats_data, stats_table)}

    def _generate_port_stats(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_stats = OrderedDict([("owner", []),
                                       ("active", []),
                                       ("tx-bytes", []),
                                       ("rx-bytes", []),
                                       ("tx-pkts", []),
                                       ("rx-pkts", []),
                                       ("tx-errors", []),
                                       ("rx-errors", []),
                                       ("tx-BW", []),
                                       ("rx-BW", [])
                                      ]
                                      )

        for port_obj in relevant_ports:
            # fetch port data
            port_stats = self._async_stats.get_port_stats(port_obj.port_id)

            owner = self.user
            active = "YES" if port_obj.is_active() else "NO"
            tx_bytes = port_stats.get_rel("obytes", format = True, suffix = "B")
            rx_bytes = port_stats.get_rel("ibytes", format = True, suffix = "B")
            tx_pkts = port_stats.get_rel("opackets", format = True, suffix = "pkts")
            rx_pkts = port_stats.get_rel("ipackets", format = True, suffix = "pkts")
            tx_errors = port_stats.get_rel("oerrors", format = True)
            rx_errors = port_stats.get_rel("ierrors", format = True)
            tx_bw = port_stats.get("m_total_tx_bps", format = True, suffix = "bps")
            rx_bw = port_stats.get("m_total_rx_bps", format = True, suffix = "bps")

            # populate to data structures
            return_stats_data[port_obj.port_id] = {"owner": owner,
                                                   "active": active,
                                                   "tx-bytes": tx_bytes,
                                                   "rx-bytes": rx_bytes,
                                                   "tx-pkts": tx_pkts,
                                                   "rx-pkts": rx_pkts,
                                                   "tx-errors": tx_errors,
                                                   "rx-errors": rx_errors,
                                                   "Tx-BW": tx_bw,
                                                   "Rx-BW": rx_bw
                                                   }
            per_field_stats["owner"].append(owner)
            per_field_stats["active"].append(active)
            per_field_stats["tx-bytes"].append(tx_bytes)
            per_field_stats["rx-bytes"].append(rx_bytes)
            per_field_stats["tx-pkts"].append(tx_pkts)
            per_field_stats["rx-pkts"].append(rx_pkts)
            per_field_stats["tx-errors"].append(tx_errors)
            per_field_stats["rx-errors"].append(rx_errors)
            per_field_stats["tx-BW"].append(tx_bw)
            per_field_stats["rx-BW"].append(rx_bw)

        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["r"]*len(relevant_ports))
        stats_table.add_rows([[k] + v
                              for k, v in per_field_stats.iteritems()],
                             header=False)
        stats_table.header(["port"] + [port.port_id
                                       for port in relevant_ports])

        return {"port_statistics": ExportableStats(return_stats_data, stats_table)}

    def _generate_port_status(self, port_id_list):
        relevant_ports = self.__get_relevant_ports(port_id_list)

        return_stats_data = {}
        per_field_status = OrderedDict([("port-type", []),
                                        ("maximum", []),
                                        ("port-status", [])
                                        ]
                                       )

        for port_obj in relevant_ports:
            # fetch port data
            port_stats = self._async_stats.get_port_stats(port_obj.port_id)


            port_type = port_obj.driver
            maximum = "{speed} Gb/s".format(speed=port_obj.speed)#CTRexAsyncStats.format_num(port_obj.get_speed_bps(), suffix="bps")
            port_status = port_obj.get_port_state_name()

            # populate to data structures
            return_stats_data[port_obj.port_id] = {"port-type": port_type,
                                                   "maximum": maximum,
                                                   "port-status": port_status,
                                                   }
            per_field_status["port-type"].append(port_type)
            per_field_status["maximum"].append(maximum)
            per_field_status["port-status"].append(port_status)

        stats_table = text_tables.TRexTextTable()
        stats_table.set_cols_align(["l"] + ["c"]*len(relevant_ports))
        stats_table.add_rows([[k] + v
                              for k, v in per_field_status.iteritems()],
                             header=False)
        stats_table.header(["port"] + [port.port_id
                                       for port in relevant_ports])

        return {"port_status": ExportableStats(return_stats_data, stats_table)}

    def __get_relevant_ports(self, port_id_list):
        # fetch owned ports
        ports = [port_obj
                 for port_obj in self._ports
                 if port_obj.is_acquired() and port_obj.port_id in port_id_list]
        # display only the first FOUR options, by design
        if len(ports) > 4:
            print format_text("[WARNING]: ", 'magenta', 'bold'), format_text("displaying up to 4 ports", 'magenta')
            ports = ports[:4]
        return ports




class CTRexStatsManager(object):

    def __init__(self, *args):
        for stat_type in args:
            # register stat handler for each stats type
            setattr(self, stat_type, CTRexStatsManager.CSingleStatsHandler())

    def __getitem__(self, item):
        stats_obj = getattr(self, item)
        if stats_obj:
            return stats_obj.get_stats()
        else:
            return None

    class CSingleStatsHandler(object):

        def __init__(self):
            self._stats = {}

        def update(self, obj_id, stats_obj):
            assert isinstance(stats_obj, CTRexStats)
            self._stats[obj_id] = stats_obj

        def get_stats(self, obj_id=None):
            if obj_id:
                return copy.copy(self._stats.pop(obj_id))
            else:
                return copy.copy(self._stats)


class CTRexStats(object):
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)


class CGlobalStats(CTRexStats):
    def __init__(self, **kwargs):
        super(CGlobalStats, self).__init__(kwargs)
        pass


class CPortStats(CTRexStats):
    def __init__(self, **kwargs):
        super(CPortStats, self).__init__(kwargs)
        pass


class CStreamStats(CTRexStats):
    def __init__(self, **kwargs):
        super(CStreamStats, self).__init__(kwargs)
        pass


if __name__ == "__main__":
    pass
