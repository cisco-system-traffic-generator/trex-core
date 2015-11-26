#!/router/bin/python
from collections import namedtuple, OrderedDict
from client_utils import text_tables
import copy

GLOBAL_STATS = 'g'
PORT_STATS = 'p'
PORT_STATUS = 'ps'
ALL_STATS_OPTS = {GLOBAL_STATS, PORT_STATS, PORT_STATUS}
ExportableStats = namedtuple('ExportableStats', ['raw_data', 'text_table'])


class CTRexInformationCenter(object):

    def __init__(self, connection_info, ports_ref, async_stats_ref):
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

    def generate_single_statistic(self, statistic_type):
        if statistic_type == GLOBAL_STATS:
            return self._generate_global_stats()
        elif statistic_type == PORT_STATS:
            # return generate_global_stats()
            pass
        elif statistic_type == PORT_STATUS:
            pass
        else:
            # ignore by returning empty object
            return {}

    def _generate_global_stats(self):
        stats_obj = self._async_stats.get_general_stats()
        return_stats_data = \
            OrderedDict([("connection", "{host}, Port {port}".format(host=self.connection_info.get("server"),
                                                                     port=self.connection_info.get("sync_port"))),
                         ("version", self.server_version.get("version", "N/A")),
                         ("cpu_util", stats_obj.get("m_cpu_util")),
                         ("total_tx", stats_obj.get("m_tx_bps", format=True, suffix="b/sec")),
                                    # {'m_tx_bps': stats_obj.get("m_tx_bps", format= True, suffix= "b/sec"),
                                    #   'm_tx_pps': stats_obj.get("m_tx_pps", format= True, suffix= "pkt/sec"),
                                    #   'm_total_tx_bytes':stats_obj.get_rel("m_total_tx_bytes",
                                    #                                        format= True,
                                    #                                        suffix = "B"),
                                    #   'm_total_tx_pkts': stats_obj.get_rel("m_total_tx_pkts",
                                    #                                        format= True,
                                    #                                        suffix = "pkts")},
                         ("total_rx", stats_obj.get("m_rx_bps", format=True, suffix="b/sec")),
                                    # {'m_rx_bps': stats_obj.get("m_rx_bps", format= True, suffix= "b/sec"),
                                    #   'm_rx_pps': stats_obj.get("m_rx_pps", format= True, suffix= "pkt/sec"),
                                    #   'm_total_rx_bytes': stats_obj.get_rel("m_total_rx_bytes",
                                    #                                        format= True,
                                    #                                        suffix = "B"),
                                    #   'm_total_rx_pkts': stats_obj.get_rel("m_total_rx_pkts",
                                    #                                        format= True,
                                    #                                        suffix = "pkts")},
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
