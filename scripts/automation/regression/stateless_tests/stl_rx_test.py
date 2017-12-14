#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from functools import wraps
from trex_stl_lib.api import *
import os, sys
import copy

ERROR_LATENCY_TOO_HIGH = 1
ERROR_CNTR_NOT_0 = 2

class STLRX_Test(CStlGeneral_Test):
    """Tests for RX feature"""

    def setUp(self):
        per_driver_params = {
            'net_vmxnet3': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'no_vlan_even_in_software_mode': True,
            },
            'net_ixgbe': {
                'rate_percent': 30,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': True,
                'latency_9k_max_average': 300,
                'latency_9k_max_latency': 400,
                'no_vlan': True,
                'no_ipv6': True,
            },
            'net_ixgbe_vf': {
                'rate_percent': 30,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'no_vlan': True,
                'no_ipv6': True,
                'no_vlan_even_in_software_mode': True,
                'max_pkt_size': 2000, # temporary, until we fix this
            },

            'net_i40e': {
                'rate_percent': 80,
                'rate_percent_soft': 10,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': True,
                'latency_9k_max_average': 100,
                'latency_9k_max_latency': 250,
            },
            'net_i40e_vf': {
                'rate_percent': 10,
                'rate_percent_soft': 1,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'no_vlan_even_in_software_mode': True,
            },
            'net_e1000_igb': {
                'rate_percent': 80,
                'total_pkts': 500,
                'rate_latency': 1,
                'latency_9k_enable': False,

            },
            'net_e1000_em': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'no_vlan_even_in_software_mode': True,
            },
            'net_virtio': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
            },

            'net_mlx5': {
                'rate_percent': 40,
                'rate_percent_soft': 0.01 if self.is_vf_nics else 1,
                'total_pkts': 1000,
                'rate_latency': 0.01 if self.is_vf_nics else 1,
                'latency_9k_enable': False if self.is_vf_nics else True,
                'latency_9k_max_average': 200,
                'latency_9k_max_latency': 200,    
            },

            'net_enic': {
                'rate_percent': 1,
                'total_pkts': 50,
                'rate_latency': 1,
                'latency_9k_enable': False,
                'rx_bytes_fix': True,
                'no_vlan_even_in_software_mode': True,
            },
            'net_ntacc': {
                'rate_percent': 10,
                'total_pkts': 1000,
                'rate_latency': 1,
                'latency_9k_enable': True,
                'latency_9k_max_average': 150,
                'latency_9k_max_latency': 350,
                'no_vlan': True,
                'no_ipv6': True,
            },
        }

        CStlGeneral_Test.setUp(self)
        assert 'bi' in CTRexScenario.stl_ports_map

        self.c = CTRexScenario.stl_trex;

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]

        port_info = self.c.get_port_info(ports = self.rx_port)[0]
        self.speed = port_info['speed']

        cap = port_info['rx']['caps']
        if "flow_stats" not in cap or "latency" not in cap:
            self.skip('port {0} does not support RX'.format(self.rx_port))
        self.cap = cap

        self.is_VM = True if 'VM' in self.modes else False

        self.max_flow_stats = port_info['rx']['counters']
        if self.max_flow_stats == 1023:
            # hack - to identify if --software flag was used on server
            software_mode = True
        else:
            software_mode = False

        software_mode = False # fix: need good way to identify software_mode

        drv_name = port_info['driver']
        self.drv_name = drv_name

        self.num_cores = self.c.get_server_system_info().get('dp_core_count', 'Unknown')
        mbufs = self.c.get_util_stats()['mbuf_stats']
        # currently in MLX drivers, we use 9k mbufs for RX, so we can't use all of them for TX.
        if self.drv_name == 'net_mlx5':
            self.k9_mbufs = 20
            self.k4_mbufs = 20
        else:
            self.k9_mbufs = 10000
            self.k4_mbufs = 10000

        for key in mbufs:
            if mbufs[key]['9kb'][1] < self.k9_mbufs:
                self.k9_mbufs = mbufs[key]['9kb'][1]
            if mbufs[key]['4096b'][1] < self.k4_mbufs:
                self.k4_mbufs = mbufs[key]['4096b'][1]

        print("")
        print ("num cores {0} num 9k mbufs {1} num 4k mbufs {2}".format(self.num_cores, self.k9_mbufs, self.k4_mbufs))

        if 'no_vlan' in per_driver_params[drv_name] and not software_mode:
            self.vlan_support = False
        else:
            self.vlan_support = True

        if 'no_ipv6' in per_driver_params[drv_name] and not software_mode:
            self.ipv6_support = False
        else:
            self.ipv6_support = True

        if 'max_pkt_size' in per_driver_params[drv_name]:
            self.max_pkt_size = per_driver_params[drv_name]['max_pkt_size']
        else:
            self.max_pkt_size = 9000

        self.rate_percent           = per_driver_params[drv_name]['rate_percent']
        self.total_pkts             = per_driver_params[drv_name]['total_pkts']
        self.rate_lat               = per_driver_params[drv_name].get('rate_latency', self.rate_percent)
        self.rate_fstat             = per_driver_params[drv_name].get('rate_percent_soft', self.rate_percent)
        self.latency_9k_enable      = per_driver_params[drv_name]['latency_9k_enable']
        self.latency_9k_max_average = per_driver_params[drv_name].get('latency_9k_max_average')
        self.latency_9k_max_latency = per_driver_params[drv_name].get('latency_9k_max_latency')
        if self.is_VM:
            max_drop_allowed = 5
        else:
            max_drop_allowed = 0
        self.allow_drop             = per_driver_params[drv_name].get('allow_packets_drop_num', max_drop_allowed)
        self.lat_pps = 1000
        self.drops_expected = False
        self.c.reset(ports = [self.tx_port, self.rx_port])
        if 'rx_bytes_fix' in per_driver_params[drv_name] and per_driver_params[drv_name]['rx_bytes_fix'] == True:
            self.fix_rx_byte_count = True
        else:
            self.fix_rx_byte_count = False

        if software_mode:
            self.qinq_support = True
        else:
            self.qinq_support = False

        # hack for enic
        if 'no_vlan_even_in_software_mode' in per_driver_params[drv_name]:
            self.vlan_support = False
            self.qinq_support = False

        # currently we are not configuring router to vlans
        if not self.is_loopback:
            self.vlan_support = False
            self.qinq_support = False

        vm = STLScVmRaw( [ STLVmFlowVar ( "ip_src",  min_value="10.0.0.1",
                                          max_value="10.0.0.255", size=4, step=1,op="inc"),
                           STLVmWrFlowVar (fv_name="ip_src", pkt_offset= "IP.src" ), # write ip to packet IP.src
                           STLVmFixIpv4(offset = "IP")                                # fix checksum
                          ]
                         # Latency is bound to one core. We test that this option is not causing trouble
                         ,cache_size =255 # Cache is ignored by latency flows. Need to test it is not crashing.
                         );

        vm_random_size = STLScVmRaw( [ STLVmFlowVar(name="fv_rand", min_value=100, max_value=1500, size=2, op="random"),
                           STLVmTrimPktSize("fv_rand"), # total packet size
                           STLVmWrFlowVar(fv_name="fv_rand", pkt_offset= "IP.len", add_val=-14), # fix ip len
                           STLVmFixIpv4(offset = "IP"),                                # fix checksum
                           STLVmWrFlowVar(fv_name="fv_rand", pkt_offset= "UDP.len", add_val=-34) # fix udp len
                          ]
                       )

        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('Your_paylaod_comes_here'))
        self.vlan_pkt = STLPktBuilder(pkt = Ether()/Dot1Q()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('Your_paylaod_comes_here'))
        self.qinq_pkt = STLPktBuilder(pkt = Ether(type=0x88A8)/Dot1Q(vlan=19)/Dot1Q(vlan=11)/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('Your_paylaod_comes_here'))
        self.ipv6pkt = STLPktBuilder(pkt = Ether()/IPv6(dst="2001:0:4137:9350:8000:f12a:b9c8:2815",src="2001:4860:0:2001::68")
                                     /UDP(dport=12,sport=1025)/('Your_paylaod_comes_here'))
        self.large_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*1000))
        self.pkt_9k = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*9000))
        self.vm_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")
                                    / UDP(dport=12,sport=1025)/('Your_paylaod_comes_here')
                                    , vm = vm)
        self.vm_large_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*1000)
                                          , vm = vm)
        self.vm_rand_size_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*1500)
                                              , vm = vm_random_size)
        # Packet size is 8202, with 2k mbuf size in RX, this makes 4 2K mbufs, plus leftover of 10 bytes in 5th mbuf
        # This test that latency code can handle the situation where latency data is not contiguous in memory
        self.vm_9k_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*8160)
                                       ,vm = vm)

        self.mlx5_defect_dpdk1711 = False
        self.mlx5_defect_dpdk1711_2 = False

        # skip mlx5 VF
        self.mlx5_defect_dpdk1711_3 = CTRexScenario.setup_name in ['trex23']
        #self.mlx5_defect_dpdk1711_3 =False


        # the setup is like that 
        #
        #  p0(VF) p1(VF) p2(VF)     p3 (VF)
        #     PF0      PF1
        #      ---------
        # we don't have control on the PF that change the way it count the packets +CRC so we disable the test
        #
        self.i40e_vf_setup_disable = CTRexScenario.setup_name in ['trex22']

        self.errs = []


    @classmethod
    def tearDownClass(cls):
        if CTRexScenario.stl_init_error:
            return
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()

    # Can use this to run a test multiple times
    def run_many_times(func):
        @wraps(func)
        def wrapped(self, *args, **kwargs):
            num_failed = 0
            max_tries = 100
            num_tries = 1

            while num_tries <= max_tries:
                try:
                    func(self, *args, **kwargs)
                    print ("Try {0} - OK".format(num_tries))
                    num_tries += 1
                    continue
                except STLError as e:
                    print ("Try {0} failed ********************".format(num_tries))
                    num_tries += 1
                    num_failed += 1
                    #assert False , '{0}'.format(e)
            print("Failed {0} times out of {1} tries".format(num_failed, num_tries-1))

        return wrapped

    def try_few_times_on_vm(func):
        @wraps(func)
        def wrapped(self, *args, **kwargs):
            # we see random failures with mlx, so do retries on it as well
            if self.is_VM:
                max_tries = 4
            else:
                max_tries = 1
            num_tries = 1

            while num_tries <= max_tries:
                try:
                    func(self, *args, **kwargs)
                    break
                except STLError as e:
                    num_tries += 1
                    if num_tries > max_tries:
                        print ("Try {0} failed. Giving up".format(num_tries))
                        assert False , '{0}'.format(e)
                    else:
                        print ("Try {0} failed. Retrying".format(num_tries))
                        print("({0}".format(e))

        return wrapped

    def __verify_latency (self, latency_stats,max_latency,max_average):

        error=0;
        err_latency = latency_stats['err_cntrs']
        latency     = latency_stats['latency']

        for key in err_latency :
            error +=err_latency[key]
        if error !=0 :
            pprint.pprint(err_latency)
            return ERROR_CNTR_NOT_0

        tmp = 'Latency results, Average {0} usec, ,Max {1} usec. '.format(int(latency['average']), int(latency['total_max']))
        print(tmp)

        if latency['average']> max_average:
            pprint.pprint(latency_stats)
            tmp = 'Average latency is too high {0} {1} '.format(latency['average'], max_average)
            print(tmp)
            return ERROR_LATENCY_TOO_HIGH

        if latency['total_max']> max_latency:
            pprint.pprint(latency_stats)
            tmp = 'Max latency is too high {0} {1} '.format(latency['total_max'], max_latency)
            print(tmp)
            return ERROR_LATENCY_TOO_HIGH

        return 0


    def __exit_with_error(self, stats, xstats, err, pkt_len=0, pkt_type=""):
        if pkt_len != 0:
            print("Failed with packet: type {0}, len {1}".format(pkt_type, pkt_len))
        pprint.pprint(stats)
        pprint.pprint(xstats)
        raise STLError(err)

    def __verify_flow (self, pg_id, total_pkts, pkt_len, pkt_type, stats, xstats):
        flow_stats = stats['flow_stats'].get(pg_id)
        if 'latency' in stats and stats['latency'] is not None:
            latency_stats = stats['latency'].get(pg_id)
        else:
            latency_stats = None

        if not flow_stats:
            assert False, "no flow stats available"

        tx_pkts  = flow_stats['tx_pkts'].get(self.tx_port, 0)
        # for continues tests, we do not know how many packets were sent
        if total_pkts == 0:
            total_pkts = tx_pkts
        tx_bytes = flow_stats['tx_bytes'].get(self.tx_port, 0)
        rx_pkts  = flow_stats['rx_pkts'].get(self.rx_port, 0)
        if latency_stats is not None:
            drops = latency_stats['err_cntrs']['dropped']
            ooo = latency_stats['err_cntrs']['out_of_order']
            dup = latency_stats['err_cntrs']['dup']
            sth = latency_stats['err_cntrs']['seq_too_high']
            stl = latency_stats['err_cntrs']['seq_too_low']
            lat = latency_stats['latency']
            if ooo != 0 or dup != 0 or stl != 0:
                self.__exit_with_error(latency_stats, xstats,
                                  'Error packets - dropped:{0}, ooo:{1} dup:{2} seq too high:{3} seq too low:{4}'.format(drops, ooo, dup, sth, stl)
                                  , pkt_len, pkt_type)

            if (drops > self.allow_drop or sth > self.allow_drop) and not self.drops_expected:
                self.__exit_with_error(latency_stats, xstats,
                                  'Error packets - dropped:{0}, ooo:{1} dup:{2} seq too high:{3} seq too low:{4}'.format(drops, ooo, dup, sth, stl)
                                  , pkt_len, pkt_type)

        if tx_pkts != total_pkts:
            pprint.pprint(flow_stats)
            self.__exit_with_error(flow_stats, xstats
                              , 'TX pkts mismatch - got: {0}, expected: {1}'.format(tx_pkts, total_pkts)
                              , pkt_len, pkt_type)
        # pkt_len == 0, means do not compare pkt length (used for streams with random length)
        if pkt_len != 0 and tx_bytes != (total_pkts * pkt_len):
            self.__exit_with_error(flow_stats, xstats
                              , 'TX bytes mismatch - got: {0}, expected: {1}'.format(tx_bytes, (total_pkts * pkt_len))
                              , pkt_len, pkt_type)

        if abs(total_pkts - rx_pkts) > self.allow_drop and not self.drops_expected:
            self.__exit_with_error(flow_stats, xstats
                              , 'RX pkts mismatch - got: {0}, expected: {1}'.format(rx_pkts, total_pkts)
                              , pkt_len, pkt_type)

        if pkt_len != 0:
            rx_pkt_len = pkt_len
            if self.fix_rx_byte_count:
                # Patch. Vic card always add vlan, so we should expect 4 extra bytes in each packet
                rx_pkt_len += 4

            if "rx_bytes" in self.cap:
                rx_bytes = flow_stats['rx_bytes'].get(self.rx_port, 0)
                if abs(rx_bytes / rx_pkt_len  - total_pkts ) > self.allow_drop and not self.drops_expected:
                    self.__exit_with_error(flow_stats, xstats
                                           , 'RX bytes mismatch - got: {0}, expected: {1}'.format(rx_bytes, (total_pkts * rx_pkt_len))
                                           , pkt_len, pkt_type)

    # RX itreation
    def __rx_iteration (self, exp_list, duration=0):

        self.c.clear_stats()

        if duration != 0:
            self.c.start(ports = [self.tx_port], duration=duration)
            self.c.wait_on_traffic(ports = [self.tx_port],timeout = duration+10,rx_delay_ms = 100)
        else:
            self.c.start(ports = [self.tx_port])
            self.c.wait_on_traffic(ports = [self.tx_port])
        stats = self.get_stats()
        xstats = self.c.get_xstats(self.rx_port)

        total_tx = 0
        total_rx = 0
        for exp in exp_list:
            add_tx,add_rx = self.count_tx_pkts(exp['pg_id'], stats)
            total_tx += add_tx
            total_rx += add_rx

        if total_tx != total_rx:
            print("Total TX packets: {0}, total RX: {1} diff: {2}".format(total_tx, total_rx, total_tx-total_rx))

        for exp in exp_list:
            if 'pkt_type' in exp:
                pkt_type = exp['pkt_type']
            else:
                pkt_type = "not specified"
            self.__verify_flow(exp['pg_id'], exp['total_pkts'], exp['pkt_len'], pkt_type, stats, xstats)

    def count_tx_pkts(self, pg_id, stats):
        flow_stats = stats['flow_stats'].get(pg_id)

        if not flow_stats:
            assert False, "no flow stats available"
        tx_pkts  = flow_stats['tx_pkts'].get(self.tx_port, 0)
        rx_pkts  = flow_stats['rx_pkts'].get(self.rx_port, 0)
        return tx_pkts, rx_pkts

    # one stream on TX --> RX
    @try_few_times_on_vm
    def test_one_stream(self):
        if self.mlx5_defect_dpdk1711:
            self.skip('not running due to defect trex-505')
        total_pkts = self.total_pkts
        s1 = STLStream(name = 'rx',
                       packet = self.pkt,
                       flow_stats = STLFlowLatencyStats(pg_id = 5),
                       mode = STLTXSingleBurst(total_pkts = total_pkts,
                                               percentage = self.rate_lat
                       ))

        # add both streams to ports
        self.c.add_streams([s1], ports = [self.tx_port])

        print("\ninjecting {0} packets on port {1}\n".format(total_pkts, self.tx_port))

        exp = {'pg_id': 5, 'total_pkts': total_pkts, 'pkt_len': s1.get_pkt_len()}

        self.__rx_iteration( [exp] )

    @try_few_times_on_vm
    def test_multiple_streams(self):
        if self.mlx5_defect_dpdk1711:
            self.skip('not running due to defect trex-505')
        self._test_multiple_streams(False)

    @try_few_times_on_vm
    def test_multiple_streams_random(self):
        if self.mlx5_defect_dpdk1711_2:
            self.skip('defect dpdk17_11 mlx5')

        if self.drv_name == 'net_i40e_vf':
            self.skip('Not running on i40 vf currently')
        self._test_multiple_streams(True)

    def _test_multiple_streams(self, is_random):
        if self.is_virt_nics:
            self.skip('Skip this for virtual NICs')

        all_ports = list(CTRexScenario.stl_ports_map['map'].keys());
        self.c.reset(ports = all_ports)

        if is_random:
            num_latency_streams = random.randint(1, 128);
            num_flow_stat_streams = random.randint(1, self.max_flow_stats);
            all_pkts = [self.pkt]
            if self.ipv6_support:
                all_pkts.append(self.ipv6pkt)
            if self.vlan_support:
                all_pkts.append(self.vlan_pkt)
            if self.qinq_support:
                all_pkts.append(self.qinq_pkt)
        else:
            num_latency_streams = 128
            num_flow_stat_streams = self.max_flow_stats
        total_pkts = int(self.total_pkts / (num_latency_streams + num_flow_stat_streams))
        if total_pkts == 0:
            total_pkts = 1
        percent_lat = float(self.rate_lat) / num_latency_streams
        percent_fstat = float(self.rate_fstat) / num_flow_stat_streams

        print("num_latency_streams:{0}".format(num_latency_streams))
        if is_random:
            print("  total percent:{0} ({1} per stream)".format(percent_lat * num_latency_streams, percent_lat))

        print("num_flow_stat_streams:{0}".format(num_flow_stat_streams))
        if is_random:
            print("  total percent:{0} ({1} per stream)".format(percent_fstat * num_flow_stat_streams, percent_fstat))

        streams = []
        exp = []
        num_9k = 0
        num_4k = 0
        for pg_id in range(1, num_latency_streams):

            if is_random:
                pkt = copy.deepcopy(all_pkts[random.randint(0, len(all_pkts) - 1)])
                pkt.set_packet(pkt.pkt / ('a' * random.randint(0, self.max_pkt_size - len(pkt.pkt))))
                # don't use more than half of the 9k mbufs. If we are out of 4k, we can use 9k, so limit 4k accordingly
                if len(pkt.pkt) >= 4 * 1024:
                    num_9k += self.num_cores
                    if num_9k > self.k9_mbufs / 2:
                        self.max_pkt_size = 4000

                if len(pkt.pkt) >= 2 * 1024 and len(pkt.pkt) < 4 * 1024:
                    num_4k += self.num_cores
                    if num_4k + num_4k > self.k4_mbufs + self.k9_mbufs / 2:
                        self.max_pkt_size = 2000


                send_mode = STLTXCont(percentage = percent_lat)
            else:
                pkt = self.pkt
                send_mode = STLTXSingleBurst(total_pkts = total_pkts+pg_id, percentage = percent_lat)

            streams.append(STLStream(name = 'rx {0}'.format(pg_id),
                                     packet = pkt,
                                     flow_stats = STLFlowLatencyStats(pg_id = pg_id),
                                     mode = send_mode))

            if is_random:
                exp.append({'pg_id': pg_id, 'total_pkts': 0, 'pkt_len': streams[-1].get_pkt_len()
                            , 'pkt_type': pkt.pkt.sprintf("%Ether.type%")})
            else:
                exp.append({'pg_id': pg_id, 'total_pkts': total_pkts+pg_id, 'pkt_len': streams[-1].get_pkt_len()})

        for pg_id in range(num_latency_streams + 1, num_latency_streams + num_flow_stat_streams):

            if is_random:
                pkt = copy.deepcopy(all_pkts[random.randint(0, len(all_pkts) - 1)])
                pkt.set_packet(pkt.pkt / ('a' * random.randint(0, self.max_pkt_size - len(pkt.pkt))))
                # don't use more than half of the 9k mbufs. If we are out of 4k, we can use 9k, so limit 4k accordingly
                if len(pkt.pkt) >= 4 * 1024:
                    num_9k += self.num_cores
                    if num_9k > self.k9_mbufs / 2:
                        self.max_pkt_size = 4000

                if len(pkt.pkt) >= 2000 and len(pkt.pkt) < 4 * 1024:
                    num_4k += self.num_cores
                    if num_4k + num_9k > self.k4_mbufs + self.k9_mbufs / 2:
                        self.max_pkt_size = 2000

                send_mode = STLTXCont(percentage = percent_fstat)
            else:
                pkt = self.pkt
                send_mode = STLTXSingleBurst(total_pkts = total_pkts+pg_id, percentage = percent_fstat)

            streams.append(STLStream(name = 'rx {0}'.format(pg_id),
                                     packet = pkt,
                                     flow_stats = STLFlowStats(pg_id = pg_id),
                                     mode = send_mode))
            if is_random:
                exp.append({'pg_id': pg_id, 'total_pkts': 0, 'pkt_len': streams[-1].get_pkt_len()
                            , 'pkt_type': pkt.pkt.sprintf("%Ether.type%")})
            else:
                exp.append({'pg_id': pg_id, 'total_pkts': total_pkts+pg_id, 'pkt_len': streams[-1].get_pkt_len()})

        # add both streams to ports
        self.c.add_streams(streams, ports = [self.tx_port])

        if is_random:
            duration = 60
            print("Duration: {0}".format(duration))
        else:
            duration = 0
        self.__rx_iteration(exp, duration = duration)


    @try_few_times_on_vm
    def test_1_stream_many_iterations (self):
        if self.i40e_vf_setup_disable:
            self.skip('i40e_vf_setup_disable')

        total_pkts = self.total_pkts
        streams_data = [
            {'name': 'Latency, with field engine of random packet size', 'pkt': self.vm_rand_size_pkt, 'lat': True},
            {'name': 'Flow stat. No latency', 'pkt': self.pkt, 'lat': False},
            {'name': 'Latency, no field engine', 'pkt': self.pkt, 'lat': True},
            {'name': 'Latency, short packet with field engine', 'pkt': self.vm_pkt, 'lat': True},
            {'name': 'Latency, large packet field engine', 'pkt': self.vm_large_pkt, 'lat': True},
            {'name': 'Latency, 9k packet with field engine', 'pkt': self.vm_9k_pkt, 'lat': True}
        ]
        if self.vlan_support:
            streams_data.append({'name': 'Flow stat with vlan. No latency', 'pkt': self.vlan_pkt, 'lat': False})

        if self.qinq_support:
            streams_data.append({'name': 'Flow stat qinq. No latency', 'pkt': self.qinq_pkt, 'lat': False})

        if self.ipv6_support:
            streams_data.append({'name': 'IPv6 flow stat. No latency', 'pkt': self.ipv6pkt, 'lat': False})
            streams_data.append({'name': 'IPv6 latency, no field engine', 'pkt': self.ipv6pkt, 'lat': True})

        streams = []
        for data in streams_data:
            if data['lat']:
                flow_stats = STLFlowLatencyStats(pg_id = 5)
                mode = STLTXSingleBurst(total_pkts = total_pkts, pps = self.lat_pps)
            else:
                flow_stats = STLFlowStats(pg_id = 5)
                mode = STLTXSingleBurst(total_pkts = total_pkts, percentage = self.rate_percent)

            s = STLStream(name = data['name'],
                          packet = data['pkt'],
                          flow_stats = flow_stats,
                          mode = mode
                         )
            streams.append(s)

        print("\ninjecting {0} packets on port {1}".format(total_pkts, self.tx_port))
        exp = {'pg_id': 5, 'total_pkts': total_pkts}

        for stream in streams:
            self.c.add_streams([stream], ports = [self.tx_port])
            print("Stream: {0}".format(stream.name))
            if 'random packet size' in stream.name:
                # hack for not trying to check match in received byte len when using random size packets
                exp['pkt_len'] = 0
            else:
                exp['pkt_len'] = stream.get_pkt_len()
            if self.is_VM:
                num_repeats = 1
            else:
                num_repeats = 10
            for i in range(1, num_repeats + 1):
                if num_repeats > 1:
                    print("Iteration {0}".format(i))
                try:
                    self.__rx_iteration( [exp] )
                except STLError as e:
                    self.c.remove_all_streams(ports = [self.tx_port])
                    raise e
            self.c.remove_all_streams(ports = [self.tx_port])


    def __9k_stream(self, pgid, ports, percent, max_latency, avg_latency, duration, pkt_size):
        my_pg_id=pgid
        s_ports=ports;
        all_ports=list(CTRexScenario.stl_ports_map['map'].keys());
        if ports == None:
            s_ports=all_ports
        assert( type(s_ports)==list)

        stream_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*pkt_size))

        try:
            # reset all ports
            self.c.reset(ports = all_ports)


            for pid in s_ports:
                s1  = STLStream(name = 'rx',
                               packet = self.pkt,
                               flow_stats = STLFlowLatencyStats(pg_id = my_pg_id+pid),
                               mode = STLTXCont(pps = 1000))

                s2 = STLStream(name = 'bulk',
                               packet = stream_pkt,
                               mode = STLTXCont(percentage = percent))


                # add both streams to ports
                self.c.add_streams([s1,s2], ports = [pid])

            self.c.clear_stats()

            self.c.start(ports = s_ports,duration = duration)
            self.c.wait_on_traffic(ports = s_ports,timeout = duration+10,rx_delay_ms = 100)
            stats = self.get_stats()

            for pid in s_ports:
                latency_stats = stats['latency'].get(my_pg_id+pid)
                err = self.__verify_latency(latency_stats, max_latency, avg_latency)
                if  err != 0:
                    flow_stats = stats['flow_stats'].get(my_pg_id + pid)
                    xstats = self.c.get_xstats(self.rx_port)
                    pprint.pprint(flow_stats)
                    pprint.pprint(xstats)
                    if err != ERROR_LATENCY_TOO_HIGH:
                        assert False, 'RX pkts error - one of the error counters is not 0'
                return err

        except STLError as e:
            assert False , '{0}'.format(e)


    # Verify that there is low latency with random packet size,duration and ports
    @try_few_times_on_vm
    def test_9k_stream(self):
        if self.is_virt_nics:
            self.skip('Skip this for virtual NICs')

        if self.latency_9k_enable == False:
           print("SKIP")
           return

        for i in range(0,5):
            print("Iteration {0}".format(i));
            duration=random.randint(10, 70);
            pgid=random.randint(1, 65000);
            pkt_size=random.randint(1000, 9000);
            all_ports = list(CTRexScenario.stl_ports_map['map'].keys());
            s_port=random.sample(all_ports, random.randint(1, len(all_ports)) )
            s_port=sorted(s_port)

            if ((self.speed == 40) or (self.speed == 100)):
                # the NIC does not support all full rate in case both port works let's filter odd ports
                s_port=list(filter(lambda x: x % 2==0, s_port))
                if len(s_port)==0:
                    s_port=[0];


            error=1;
            for j in range(0,5):
               print(" {4} - duration {0} pgid {1} pkt_size {2} s_port {3} ".format(duration,pgid,pkt_size,s_port,j));
               if self.__9k_stream(pgid,
                                        s_port, self.rate_percent,
                                        self.latency_9k_max_latency,
                                        self.latency_9k_max_average,
                                        duration,
                                        pkt_size)==0:
                   error=0;
                   break;

            if error:
                assert False , "Latency too high"
            else:
                print("===>Iteration {0} PASS {1}".format(i,j));


    def check_stats(self, a, b, err):
        if a != b:
            tmp = 'ERROR field : {0}, read : {1} != expected : {2} '.format(err,a,b)
            print(tmp)
            self.errs.append(tmp)


    def send_1_burst(self, client_ports, is_latency, pkts):
        self.errs = []
        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = (60 - len(base_pkt)) * 'x'
        stream_pkt = STLPktBuilder(pkt = base_pkt/pad)

        # reset all ports
        self.c.reset()


        for c_port in client_ports:
            if is_latency:
                s1  = STLStream(name = 'rx',
                           packet = stream_pkt,
                           flow_stats = STLFlowLatencyStats(pg_id = 5 + c_port),
                           mode = STLTXSingleBurst(total_pkts = pkts, pps = 1000))
            else:
                s1  = STLStream(name = 'rx',
                           packet = stream_pkt,
                           mode = STLTXSingleBurst(total_pkts = pkts, pps = 1000))


            # add both streams to ports
            self.c.add_streams(s1, ports = [c_port])

        self.c.clear_stats()

        self.c.start(ports = client_ports)
        self.c.wait_on_traffic(ports = client_ports)
        pgid_stats = self.get_stats()
        stats = self.c.get_stats()

        bytes = pkts * 64
        total_pkts = pkts * len(client_ports)
        total_bytes = total_pkts * 64

        tps = stats['total']
        self.check_stats(tps['ibytes'], total_bytes, "tps[ibytes]")
        self.check_stats(tps['obytes'], total_bytes, "tps[obytes]")
        self.check_stats(tps['ipackets'], total_pkts, "tps[ipackets]")
        self.check_stats(tps['opackets'], total_pkts, "tps[opackets]")

        for c_port in client_ports:
            s_port = CTRexScenario.stl_ports_map['map'][c_port]

            ips = stats[s_port]
            ops = stats[c_port]

            self.check_stats(ops["obytes"], bytes, "stats[%s][obytes]" % c_port)
            self.check_stats(ops["opackets"], pkts, "stats[%s][opackets]" % c_port)

            self.check_stats(ips["ibytes"], bytes, "stats[%s][ibytes]" % s_port)
            self.check_stats(ips["ipackets"], pkts, "stats[%s][ipackets]" % s_port)

            if is_latency:
                ls = pgid_stats['flow_stats'][5 + c_port]
                self.check_stats(ls['rx_pkts']['total'], pkts, "ls['rx_pkts']['total']")
                self.check_stats(ls['rx_pkts'][s_port], pkts, "ls['rx_pkts'][%s]" % s_port)

                self.check_stats(ls['tx_pkts']['total'], pkts, "ls['tx_pkts']['total']")
                self.check_stats(ls['tx_pkts'][c_port], pkts, "ls['tx_pkts'][%s]" % c_port)

                self.check_stats(ls['tx_bytes']['total'], bytes, "ls['tx_bytes']['total']")
                self.check_stats(ls['tx_bytes'][c_port], bytes, "ls['tx_bytes'][%s]" % c_port)

        if self.errs:
            pprint.pprint(stats)
            msg = 'Stats do not match the expected:\n' + '\n'.join(self.errs)
            raise STLError(msg)


    @try_few_times_on_vm
    def test_fcs_stream(self):
        if self.i40e_vf_setup_disable:
            self.skip('Skip for vf_setup')

        """ this test send 1 64 byte packet with latency and check that all counters are reported as 64 bytes"""
        ports = list(CTRexScenario.stl_ports_map['map'].keys())
        for lat in [True, False]:
            print("\nSending from ports: {0}, has latency: {1} ".format(ports, lat))
            self.send_1_burst(ports, lat, 100)

    # this test adds more and more latency streams and re-test with incremental
    @try_few_times_on_vm
    def test_incremental_latency_streams (self):
        if self.mlx5_defect_dpdk1711_3:
            self.skip('Skip for mlx5_defect_dpdk1711_3')

        if self.i40e_vf_setup_disable:
            self.skip('Skip for vf_setup')

        if self.is_virt_nics:
            self.skip('Skip this for virtual NICs')

        total_pkts = self.total_pkts
        percent = 0.5

        try:
            # We run till maximum streams allowed. At some point, expecting drops, because rate is too high.
            # then run with less streams again, to see that system is still working.
            for num_iter in [128, 5]:
                exp = []
                for i in range(1, num_iter):
                    # mix small and large packets
                    if i % 2 != 0:
                        my_pkt = self.pkt
                    else:
                        my_pkt = self.large_pkt
                    s1 = STLStream(name = 'rx',
                                   packet = my_pkt,
                                   flow_stats = STLFlowLatencyStats(pg_id = i),
                                   mode = STLTXSingleBurst(total_pkts = total_pkts,
                                                           percentage = percent
                                                       ))

                    # add both streams to ports
                    self.c.add_streams([s1], ports = [self.tx_port])
                    total_percent = i * percent
                    if total_percent > self.rate_lat:
                        self.drops_expected = True
                    else:
                        self.drops_expected = False

                    print("port {0} : {1} streams at {2}% of line rate\n".format(self.tx_port, i, total_percent))

                    exp.append({'pg_id': i, 'total_pkts': total_pkts, 'pkt_len': s1.get_pkt_len()})

                    self.__rx_iteration( exp )

                self.c.remove_all_streams(ports = [self.tx_port])

        except STLError as e:
            assert False , '{0}'.format(e)


    # counters get stuck in i40e when they are getting to limit.
    # this test checks our workaround to this issue
    def test_x710_counters_wraparound(self):
        if self.drv_name != 'net_i40e':
            self.skip('Test is only for i40e.')

        percent = min(20, self.speed * 0.8) # 8G at X710 and 20G at XL710
        total_pkts = 300000000              # send 300 million packets to ensure getting to threshold of reset several times

        s1 = STLStream(name = 'wrapping_stream',
                       packet = self.pkt,
                       flow_stats = STLFlowStats(pg_id = 5),
                       mode = STLTXSingleBurst(total_pkts = total_pkts,
                                               percentage = percent))

        # add both streams to ports
        self.c.add_streams([s1], ports = [self.tx_port])

        print("\ninjecting {0} packets on port {1}\n".format(total_pkts, self.tx_port))

        exp = {'pg_id': 5, 'total_pkts': total_pkts, 'pkt_len': s1.get_pkt_len()}

        self.__rx_iteration( [exp] )

    def get_stats(self):
        old_stats = self.c.get_stats()
        new_stats = self.c.get_pgid_stats()
        if  'latency' in new_stats:
            if old_stats['latency'] != new_stats['latency']:
                print ("New and old stats differ in latency")
                print(old_stats['latency'])
                print(new_stats['latency'])
                assert False , "New and old stats differ in latency"
        if 'flow_stats' in new_stats:
            for pg_id in old_stats['flow_stats']:
                for field in ['rx_pkts', 'tx_pkts', 'tx_bytes']:
                    if pg_id in old_stats['flow_stats'] and pg_id in new_stats['flow_stats']:
                        if field in old_stats['flow_stats'][pg_id] and field in new_stats['flow_stats'][pg_id]:
                            if old_stats['flow_stats'][pg_id][field] != new_stats['flow_stats'][pg_id][field]:
                                print ("New and old stats differ in flow_stats")
                                print("**********************")
                                print(old_stats['flow_stats'][pg_id][field])
                                print("**********************")
                                print(new_stats['flow_stats'][pg_id][field])
                                print("**********************")
                                assert False , "New and old stats differ in flow stat"

        return new_stats
