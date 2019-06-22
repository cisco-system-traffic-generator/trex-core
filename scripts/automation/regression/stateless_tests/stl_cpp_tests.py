from .stl_general_test import CStlGeneral_Test, CTRexScenario
import os
import re

class STLCPP_Test(CStlGeneral_Test):
    '''Tests related to CPP'''

    def setUp(self):
        CStlGeneral_Test.setUp(self)
        print('')
        self.c = CTRexScenario.stl_trex
        self.tx_port, self.rx_port = CTRexScenario.ports_map['bi'][0]

        port_info = self.c.get_port_info(ports = self.rx_port)[0]
        self.drv_name = port_info['driver']


    def test_mlx5_xstats(self):
        if self.drv_name != 'net_mlx5':
            self.skip('Test is only for mlx5, this is: %s' % self.drv_name)

        src_path = CTRexScenario.scripts_path + '/../src/drivers/trex_driver_mlx5.cpp'
        if not os.path.isfile(src_path):
            self.skip('No sources')

        with open(src_path) as f:
            driver_cont = f.read()

        xstat_names_dpdk = self.c.any_port.xstats.names

        stat_parts = driver_cont.split('    COUNT\n    };')
        stat_parts_cnt = len(stat_parts) - 1
        assert stat_parts_cnt == 1, 'Got several stat enums: %s' % stat_parts_cnt

        xstat_parts = driver_cont.split('    XCOUNT\n    };')
        xstat_parts_cnt = len(xstat_parts) - 1
        assert xstat_parts_cnt == 1, 'Got several xstat enums: %s' % xstat_parts_cnt

        stat_enum = stat_parts[0].split('enum {')[-1].strip().replace(',', '').splitlines()
        xstat_enum = xstat_parts[0].split('enum {')[-1].strip().replace(',', '').splitlines()
        stat_enum_len = len(stat_enum)
        xstat_enum_len = len(xstat_enum)
        dpdk_stats_len = len(xstat_names_dpdk)

        def get_stats_error():
            return 'Mismatch between DPDK stats and TRex stats.\n\nDPDK:\n%s\n\nStats:\n%s\n\nXstats:\n%s' % (xstat_names_dpdk, stat_enum, xstat_enum)

        assert dpdk_stats_len > stat_enum_len + xstat_enum_len, get_stats_error()

        per_queue_re = re.compile('(t|r)x_q[0-9]+')

        xstats_offset = dpdk_stats_len - xstat_enum_len
        for i, stat_dpdk in enumerate(xstat_names_dpdk):
            if i < stat_enum_len:
                # stats
                assert stat_dpdk == stat_enum[i].strip(), get_stats_error()
            elif i < xstats_offset:
                # per queue stats
                assert per_queue_re.match(stat_dpdk), get_stats_error()
            else:
                # xstats
                assert stat_dpdk == xstat_enum[i - xstats_offset].strip(), get_stats_error()



