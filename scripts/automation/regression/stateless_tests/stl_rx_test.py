#!/router/bin/python
from .stl_general_test import CStlGeneral_Test, CTRexScenario
from trex_stl_lib.api import *
import os, sys

ERROR_LATENCY_TOO_HIGH = 1

class STLRX_Test(CStlGeneral_Test):
    """Tests for RX feature"""

    def setUp(self):
        per_driver_params = {
                'net_vmxnet3': {
                        'rate_percent': 1,
                        'total_pkts': 50,
                        'rate_latency': 1,
                        'latency_9k_enable': False,
                        },
                'net_ixgbe': {
                        'rate_percent': 30,
                        'total_pkts': 1000,
                        'rate_latency': 1,
                        'latency_9k_enable': True,
                        'latency_9k_max_average': 300,
                        'latency_9k_max_latency': 400,
                        },
                'net_ixgbe_vf': {
                        'rate_percent': 30,
                        'total_pkts': 1000,
                        'rate_latency': 1,
                        'latency_9k_enable': False,
                        },

                'net_i40e': {
                        'rate_percent': 80,
                        'total_pkts': 1000,
                        'rate_latency': 1,
                        'latency_9k_enable': True,
                        'latency_9k_max_average': 100,
                        'latency_9k_max_latency': 250,
                        },
                'net_i40e_vf': {
                        'rate_percent': 80,
                        'total_pkts': 1000,
                        'rate_latency': 1,
                        'latency_9k_enable': False,
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
                        },
                'net_virtio': {
                        'rate_percent': 1,
                        'total_pkts': 50,
                        'rate_latency': 1,
                        'latency_9k_enable': False,
                        'allow_packets_drop_num': 1, # allow 1 pkt drop
                        },

                 'net_mlx5': {
                        'rate_percent': 80,
                        'total_pkts': 1000,
                        'rate_latency': 1,
                        'latency_9k_enable': False if self.is_vf_nics else True,
                        'latency_9k_max_average': 100,
                        'latency_9k_max_latency': 450,    #see latency issue trex-261 
                        },

                 'net_enic': {
                        'rate_percent': 1,
                        'total_pkts': 50,
                        'rate_latency': 1,
                        'latency_9k_enable': False,
                        },

                  
                }

        CStlGeneral_Test.setUp(self)
        assert 'bi' in CTRexScenario.stl_ports_map

        self.c = CTRexScenario.stl_trex

        self.tx_port, self.rx_port = CTRexScenario.stl_ports_map['bi'][0]

        port_info = self.c.get_port_info(ports = self.rx_port)[0]
        self.speed = port_info['speed']

        cap = port_info['rx']['caps']
        if "flow_stats" not in cap or "latency" not in cap:
            self.skip('port {0} does not support RX'.format(self.rx_port))
        self.cap = cap

        drv_name = port_info['driver']
        self.drv_name = drv_name
        if drv_name == 'net_ixgbe':
            self.ipv6_support = False
        else:
            self.ipv6_support = True
        self.rate_percent           = per_driver_params[drv_name]['rate_percent']
        self.total_pkts             = per_driver_params[drv_name]['total_pkts']
        self.rate_lat               = per_driver_params[drv_name].get('rate_latency', self.rate_percent)
        self.latency_9k_enable      = per_driver_params[drv_name]['latency_9k_enable']
        self.latency_9k_max_average = per_driver_params[drv_name].get('latency_9k_max_average')
        self.latency_9k_max_latency = per_driver_params[drv_name].get('latency_9k_max_latency')
        self.allow_drop             = per_driver_params[drv_name].get('allow_packets_drop_num', 0)

        self.lat_pps = 1000
        self.drops_expected = False
        self.c.reset(ports = [self.tx_port, self.rx_port])

        vm = STLScVmRaw( [ STLVmFlowVar ( "ip_src",  min_value="10.0.0.1",
                                          max_value="10.0.0.255", size=4, step=1,op="inc"),
                           STLVmWrFlowVar (fv_name="ip_src", pkt_offset= "IP.src" ), # write ip to packet IP.src
                           STLVmFixIpv4(offset = "IP")                                # fix checksum
                          ]
                         # Latency is bound to one core. We test that this option is not causing trouble
                         ,split_by_field = "ip_src"
                         ,cache_size =255 # Cache is ignored by latency flows. Need to test it is not crashing.
                         );

        self.pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('Your_paylaod_comes_here'))
        self.ipv6pkt = STLPktBuilder(pkt = Ether()/IPv6(dst="2001:0:4137:9350:8000:f12a:b9c8:2815",src="2001:4860:0:2001::68")
                                     /UDP(dport=12,sport=1025)/('Your_paylaod_comes_here'))
        self.large_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*1000))
        self.pkt_9k = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*9000))
        self.vm_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")
                                    / UDP(dport=12,sport=1025)/('Your_paylaod_comes_here')
                                    , vm = vm)
        self.vm_large_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*1000)
                                          , vm = vm)
        self.vm_9k_pkt = STLPktBuilder(pkt = Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)/('a'*9000)
                                       ,vm = vm)
        self.errs = []


    @classmethod
    def tearDownClass(cls):
        if CTRexScenario.stl_init_error:
            return
        # connect back at end of tests
        if not cls.is_connected():
            CTRexScenario.stl_trex.connect()


    def __verify_latency (self, latency_stats,max_latency,max_average):

        error=0;
        err_latency = latency_stats['err_cntrs']
        latency     = latency_stats['latency']

        for key in err_latency :
            error +=err_latency[key]
        if error !=0 :
            pprint.pprint(err_latency)
            tmp = 'RX pkts ERROR - one of the error is on'
            print(tmp)
            assert False, tmp

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



    def __verify_flow (self, pg_id, total_pkts, pkt_len, stats):
        flow_stats = stats['flow_stats'].get(pg_id)
        latency_stats = stats['latency'].get(pg_id)

        if not flow_stats:
            assert False, "no flow stats available"

        tx_pkts  = flow_stats['tx_pkts'].get(self.tx_port, 0)
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
                pprint.pprint(latency_stats)
                tmp='Error packets - dropped:{0}, ooo:{1} dup:{2} seq too high:{3} seq too low:{4}'.format(drops, ooo, dup, sth, stl)
                assert False, tmp

            if (drops > self.allow_drop or sth != 0) and not self.drops_expected:
                pprint.pprint(latency_stats)
                tmp='Error packets - dropped:{0}, ooo:{1} dup:{2} seq too high:{3} seq too low:{4}'.format(drops, ooo, dup, sth, stl)
                assert False, tmp

        if tx_pkts != total_pkts:
            pprint.pprint(flow_stats)
            tmp = 'TX pkts mismatch - got: {0}, expected: {1}'.format(tx_pkts, total_pkts)
            assert False, tmp

        if tx_bytes != (total_pkts * pkt_len):
            pprint.pprint(flow_stats)
            tmp = 'TX bytes mismatch - got: {0}, expected: {1}'.format(tx_bytes, (total_pkts * pkt_len))
            assert False, tmp

        if abs(total_pkts - rx_pkts) > self.allow_drop and not self.drops_expected:
            pprint.pprint(flow_stats)
            tmp = 'RX pkts mismatch - got: {0}, expected: {1}'.format(rx_pkts, total_pkts)
            assert False, tmp

        if "rx_bytes" in self.cap:
            rx_bytes = flow_stats['rx_bytes'].get(self.rx_port, 0)
            if abs(rx_bytes / pkt_len  - total_pkts ) > self.allow_drop and not self.drops_expected:
                pprint.pprint(flow_stats)
                tmp = 'RX bytes mismatch - got: {0}, expected: {1}'.format(rx_bytes, (total_pkts * pkt_len))
                assert False, tmp


    # RX itreation
    def __rx_iteration (self, exp_list):

        self.c.clear_stats()

        self.c.start(ports = [self.tx_port])
        self.c.wait_on_traffic(ports = [self.tx_port])
        stats = self.c.get_stats()

        for exp in exp_list:
            self.__verify_flow(exp['pg_id'], exp['total_pkts'], exp['pkt_len'], stats)


    # one stream on TX --> RX
    def test_one_stream(self):
        total_pkts = self.total_pkts

        try:
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


        except STLError as e:
            assert False , '{0}'.format(e)


    def test_multiple_streams(self):
        if self.is_virt_nics:
            self.skip('Skip this for virtual NICs')

        num_latency_streams = 128
        num_flow_stat_streams = 127
        total_pkts = int(self.total_pkts / (num_latency_streams + num_flow_stat_streams))
        if total_pkts == 0:
            total_pkts = 1
        percent = float(self.rate_lat) / (num_latency_streams + num_flow_stat_streams)

        try:
            streams = []
            exp = []
            # 10 identical streams
            for pg_id in range(1, num_latency_streams):

                streams.append(STLStream(name = 'rx {0}'.format(pg_id),
                                         packet = self.pkt,
                                         flow_stats = STLFlowLatencyStats(pg_id = pg_id),
                                         mode = STLTXSingleBurst(total_pkts = total_pkts+pg_id, percentage = percent)))

                exp.append({'pg_id': pg_id, 'total_pkts': total_pkts+pg_id, 'pkt_len': streams[-1].get_pkt_len()})

            for pg_id in range(num_latency_streams + 1, num_latency_streams + num_flow_stat_streams):

                streams.append(STLStream(name = 'rx {0}'.format(pg_id),
                                         packet = self.pkt,
                                         flow_stats = STLFlowStats(pg_id = pg_id),
                                         mode = STLTXSingleBurst(total_pkts = total_pkts+pg_id, percentage = percent)))

                exp.append({'pg_id': pg_id, 'total_pkts': total_pkts+pg_id, 'pkt_len': streams[-1].get_pkt_len()})

            # add both streams to ports
            self.c.add_streams(streams, ports = [self.tx_port])

            self.__rx_iteration(exp)


        except STLError as e:
            assert False , '{0}'.format(e)

    def test_1_stream_many_iterations (self):
        total_pkts = self.total_pkts

        try:
            streams_data = [
                {'name': 'Flow stat. No latency', 'pkt': self.pkt, 'lat': False},
                {'name': 'Latency, no field engine', 'pkt': self.pkt, 'lat': True},
                {'name': 'Latency, short packet with field engine', 'pkt': self.vm_pkt, 'lat': True},
                {'name': 'Latency, large packet field engine', 'pkt': self.vm_large_pkt, 'lat': True}
            ]
            if self.latency_9k_enable:
                streams_data.append({'name': 'Latency, 9k packet with field engine', 'pkt': self.vm_9k_pkt, 'lat': True})

            if self.ipv6_support:
                streams_data.append({'name': 'IPv6 flow stat. No latency', 'pkt': self.ipv6pkt, 'lat': False})
                streams_data.append({'name': 'IPv6 latency, no field engine', 'pkt': self.ipv6pkt, 'lat': True})

            streams = []
            for data in streams_data:
                if data['lat']:
                    flow_stats = STLFlowLatencyStats(pg_id = 5)
                    mode = STLTXSingleBurst(total_pkts = total_pkts, percentage = self.rate_percent)
                else:
                    flow_stats = STLFlowStats(pg_id = 5)
                    mode = STLTXSingleBurst(total_pkts = total_pkts, pps = self.lat_pps)

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
                exp['pkt_len'] = stream.get_pkt_len()
                for i in range(0, 10):
                    print("Iteration {0}".format(i))
                    self.__rx_iteration( [exp] )
                self.c.remove_all_streams(ports = [self.tx_port])


        except STLError as e:
            assert False , '{0}'.format(e)



    def __9k_stream(self,pgid,ports,precet,max_latency,avg_latency,duration,pkt_size):
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
                               mode = STLTXCont(percentage =precet))


                # add both streams to ports
                self.c.add_streams([s1,s2], ports = [pid])

            self.c.clear_stats()

            self.c.start(ports = s_ports,duration = duration)
            self.c.wait_on_traffic(ports = s_ports,timeout = duration+10,rx_delay_ms = 100)
            stats = self.c.get_stats()

            for pid in s_ports:
                latency_stats = stats['latency'].get(my_pg_id+pid)
                #pprint.pprint(latency_stats)
                if self.__verify_latency (latency_stats,max_latency,avg_latency) !=0:
                    return (ERROR_LATENCY_TOO_HIGH);

            return 0

        except STLError as e:
            assert False , '{0}'.format(e)





    # check low latency when you have stream of 9K stream
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
                                        s_port,90,
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

        try:
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
                    ls = stats['flow_stats'][5 + c_port]
                    self.check_stats(ls['rx_pkts']['total'], pkts, "ls['rx_pkts']['total']")
                    self.check_stats(ls['rx_pkts'][s_port], pkts, "ls['rx_pkts'][%s]" % s_port)
                
                    self.check_stats(ls['tx_pkts']['total'], pkts, "ls['tx_pkts']['total']")
                    self.check_stats(ls['tx_pkts'][c_port], pkts, "ls['tx_pkts'][%s]" % c_port)
                
                    self.check_stats(ls['tx_bytes']['total'], bytes, "ls['tx_bytes']['total']")
                    self.check_stats(ls['tx_bytes'][c_port], bytes, "ls['tx_bytes'][%s]" % c_port)

            if self.errs:
                pprint.pprint(stats)
                msg = 'Stats do not match the expected:\n' + '\n'.join(self.errs)
                raise Exception(msg)

        except STLError as e:
            assert False , '{0}'.format(e)

    def _run_fcs_stream (self,is_vm):
        """ this test send 1 64 byte packet with latency and check that all counters are reported as 64 bytes"""
        try:
            ports = list(CTRexScenario.stl_ports_map['map'].keys())
            for lat in [True, False]:
                print("\nSending from ports: {0}, has latency: {1} ".format(ports, lat))
                self.send_1_burst(ports, lat, 100)
                print('Success.')
            return True
        except Exception as e:
            if is_vm :
                return False
            else:
                raise


# this test sends 1 64 byte packet with latency and check that all counters are reported as 64 bytes
    def test_fcs_stream(self):

        # in case of VM and vSwitch there are drop of packets in some cases, let retry number of times 
        # in this case we just want to check functionality that packet of 64 is reported as 64 in all levels 
        is_vm = self.is_virt_nics or self.is_vf_nics

        tries = 1
        if is_vm:
            tries = 4
        for i in range(tries):
            if self._run_fcs_stream(is_vm):
                return
            print("==> Try number #%d failed ..." % i)
        self.fail('\n'.join(self.errs))

    # this test adds more and more latency streams and re-test with incremental
    def test_incremental_latency_streams (self):
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

