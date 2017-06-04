# from __future__ import division
import stl_path
from trex_stl_lib.api import *

import yaml
import json
from pprint import pprint
import argparse
import sys
import time


class STLBench(object):
    ip_range = {}
    ip_range['src'] = {'start': '16.0.0.0', 'end': '16.0.255.255'}
    ip_range['dst'] = {'start': '48.0.0.0', 'end': '48.0.255.255'}
    ports = {'min': 1234, 'max': 65500}
    pkt_size = {'min': 64, 'max': 9216}
    imix_table = [{'size': 60, 'pps': 28, 'isg': 0},
                  {'size': 590, 'pps': 20, 'isg': 0.1},
                  {'size': 1514, 'pps': 4, 'isg': 0.2}]

    def create_stream(self, size, vm, src, dst, pps=1, isg=0):
        # Create base packet and pad it to size
        base_pkt = Ether() / IP(src=src, dst=dst) / UDP()
        pad = max(0, int(size) - len(base_pkt) - 4) * 'x'

        pkt = STLPktBuilder(pkt=base_pkt / pad,
                            vm=vm)

        return STLStream(packet=pkt,
                         mode=STLTXCont(pps=pps),
                         isg=isg)

    def get_streams(self, size=64, vm=None, direction=0, **kwargs):
        if direction == 0:
            src, dst = self.ip_range['src'], self.ip_range['dst']
        else:
            src, dst = self.ip_range['dst'], self.ip_range['src']

        if not vm or vm == 'none':
            vm_var = None
        elif vm == 'var1':
            vm_var = [
                STLVmFlowVar(name='src', min_value=src['start'], max_value=src['end'], size=4, op='inc'),
                STLVmWrFlowVar(fv_name='src', pkt_offset='IP.src'),
                STLVmFixIpv4(offset='IP')
            ]
        elif vm == 'var2':
            vm_var = [
                STLVmFlowVar(name='src', min_value=src['start'], max_value=src['end'], size=4, op='inc'),
                STLVmWrFlowVar(fv_name='src', pkt_offset='IP.src'),
                STLVmFlowVar(name='dst', min_value=dst['start'], max_value=dst['end'], size=4, op='inc'),
                STLVmWrFlowVar(fv_name='dst', pkt_offset='IP.dst'),
                STLVmFixIpv4(offset='IP')
            ]
        elif vm == 'random':
            vm_var = [
                STLVmFlowVar(name='src', min_value=src['start'], max_value=src['end'], size=4, op='random'),
                STLVmWrFlowVar(fv_name='src', pkt_offset='IP.src'),
                STLVmFixIpv4(offset='IP')
            ]
        elif vm == 'tuple':
            vm_var = [
                STLVmTupleGen(ip_min=src['start'], ip_max=src['end'], port_min=self.ports['min'],
                              port_max=self.ports['max'], name='tuple'),
                STLVmWrFlowVar(fv_name='tuple.ip', pkt_offset='IP.src'),
                STLVmWrFlowVar(fv_name='tuple.port', pkt_offset='UDP.sport'),
                STLVmFixIpv4(offset='IP')
            ]
        elif vm == 'size':
            if size == 'imix':
                raise STLError("Can't use VM of type 'size' with IMIX.")
            size = self.pkt_size['max']
            l3_len_fix = -len(Ether())
            l4_len_fix = l3_len_fix - len(IP())
            vm_var = [
                STLVmFlowVar(name='fv_rand', min_value=(self.pkt_size['min'] - 4), max_value=(self.pkt_size['max'] - 4),
                             size=2, op='random'),
                STLVmTrimPktSize('fv_rand'),
                STLVmWrFlowVar(fv_name='fv_rand', pkt_offset='IP.len', add_val=l3_len_fix),
                STLVmWrFlowVar(fv_name='fv_rand', pkt_offset='UDP.len', add_val=l4_len_fix),
                STLVmFixIpv4(offset='IP')
            ]
        elif vm == 'cached':
            vm_raw = [
                STLVmFlowVar(name='src', min_value=src['start'], max_value=src['end'], size=4, op='inc'),
                STLVmWrFlowVar(fv_name='src', pkt_offset='IP.src'),
                STLVmFixIpv4(offset='IP')
            ]
            vm_var = STLScVmRaw(vm_raw, cache_size=255);
        else:
            raise Exception("VM '%s' not available" % vm)
        if size == 'imix':
            return [
                self.create_stream(p['size'], vm_var, src=src['start'], dst=dst['start'], pps=p['pps'], isg=p['isg'])
                for p in self.imix_table]
        return [self.create_stream(size, vm_var, src=src['start'], dst=dst['start'])]


class Rate:
    def __init__(self, rate):
        self.rate = float(rate)

    def convert_percent_to_rate(self, percent_of_rate):
        return (float(percent_of_rate) / 100.00) * self.rate

    def convert_rate_to_percent_of_max_rate(self, rate_portion):
        return (float(rate_portion) / float(self.rate)) * 100.00


class NdrBenchConfig:
    def __init__(self, ports, pkt_size=64, vm='cached', title='Title', cores=1, latency_rate=1000,
                 iteration_duration=20.00,
                 q_ful_resolution=2.00,
                 first_run_duration=20.00, pdr=0.1, pdr_error=1.0, ndr_results=1, max_iterations=10, latency=True,
                 core_mask=1, drop_rate_interval=10, verbose=False, **kwargs):
        self.iteration_duration = iteration_duration
        self.q_ful_resolution = q_ful_resolution
        self.first_run_duration = first_run_duration
        self.pdr = pdr  # desired percent of drop-rate. pdr = 0 is NO drop-rate
        self.pdr_error = pdr_error
        self.ndr_results = ndr_results
        self.max_iterations = max_iterations
        self.core_mask = core_mask
        self.latencyCalculation = latency
        self.verbose = verbose
        self.ports = list(ports)
        self.transmit_ports = [self.ports[i] for i in range(0, len(self.ports), 2)]
        self.drop_rate_interval = drop_rate_interval
        self.pkt_size = pkt_size
        self.vm = vm
        self.title = title
        self.latency_rate = latency_rate
        self.cores = cores

    def load_yaml(self, filename):
        pass

    def config_to_dict(self):
        config_dict = {'iteration_duration': self.iteration_duration, 'q_ful_resolution': self.q_ful_resolution,
                       'first_run_duration': self.first_run_duration, 'pdr': self.pdr, 'pdr_error': self.pdr_error,
                       'ndr_results': self.ndr_results, 'max_iterations': self.max_iterations,
                       'core_mask': self.core_mask,
                       'latencyCalculation': self.latencyCalculation, 'ports': self.ports,
                       'drop_rate_interval': self.drop_rate_interval, 'pkt_size': self.pkt_size, 'vm': self.vm,
                       'cores': self.cores}
        return config_dict


class NdrBenchResults:
    def __init__(self, config=None, results={}):
        self.stats = dict(results)
        self.init_time = time.time()
        self.config = config

    def update(self, updated_dict):
        self.stats.update(updated_dict)

    def convert_rate(self, rate_bps, packet=False):
        converted = float(rate_bps)
        magnitude = 0
        while converted > 100.00:
            magnitude += 1
            converted /= 1000.00
        converted = round(converted, 2)
        if packet:
            postfix = "PPS"
        else:
            postfix = "bps"
        if magnitude == 0:
            return (str(converted) + postfix)
        elif magnitude == 1:
            return (str(converted) + " K" + postfix)
        elif magnitude == 2:
            return (str(converted) + " M" + postfix)
        elif magnitude == 3:
            return (str(converted) + " G" + postfix)

    def print_latency(self):
        for k in self.stats['latency'].keys():
            print("Latency stats on port             :%d" % k)
            print ("    Latency Rate                  :%0.2f PPS" % self.stats['latency'][k]['latency_rate'])
            print ("    Average                       :%0.2f" % self.stats['latency'][k]['average'])
            print ("    Jitter                        :%0.2f" % self.stats['latency'][k]['jitter'])
            print ("    Total Max                     :%0.2f" % self.stats['latency'][k]['total_max'])
            print ("    Total Min                     :%0.2f" % self.stats['latency'][k]['total_min'])
            print ("    Histogram                     :%s   " % self.stats['latency'][k]['histogram'])

    def print_run_stats(self, latency):
        print("Elapsed Time                      :%0.2f seconds" % (float(time.time()) - self.init_time))
        print("Queue Full                        :%0.2f %% of oPackets" % self.stats['queue_full_percentage'])
        print("BW Per Core                       :%0.2f Gbit/Sec @100%% per core" % float(self.stats['bw_per_core']))
        print("RX PPS                            :%s       " % self.convert_rate(float(self.stats['rx_pps']), True))
        print("TX PPS                            :%s       " % self.convert_rate(float(self.stats['tx_pps']), True))
        print("TX Utilization                    :%0.2f %%" % self.stats['tx_util'])
        print("TRex CPU                          :%0.2f %%" % self.stats['cpu_util'])
        print("Total TX L1                       :%s     " % self.convert_rate(float(self.stats['total_tx_L1'])))
        print("Total RX L1                       :%s     " % self.convert_rate(float(self.stats['total_rx_L1'])))
        print("Total TX L2                       :%s     " % self.convert_rate(float(self.stats['tx_bps'])))
        print("Total RX L2                       :%s     " % self.convert_rate(float(self.stats['rx_bps'])))
        if 'rate_diffrential' in self.stats:
            print("Distance from current Optimum     :%0.2f %%" % self.stats['rate_diffrential'])

        if latency:
            self.print_latency()

    def print_iteration_data(self, latency):
        if 'title' in self.stats:
            print("\nTitle                             :%s" % self.stats['title'])
        if 'iteration' in self.stats:
            print("Iteration                         :%d" % self.stats['iteration'])
        print("Running Rate                      :%s" % self.convert_rate(float(self.stats['rate_tx_bps'])))
        print("Running Rate (%% of max)           :%0.2f %%" % self.stats['rate_p'])
        print("Max Rate                          :%s       " % self.convert_rate(float(self.stats['max_rate_bps'])))
        print("Drop Rate                         :%0.2f %% of oPackets" % self.stats['drop_rate_percentage'])
        self.print_run_stats(latency)

    def print_final(self, latency):
        print("\nTitle                             :%s" % self.stats['title'])
        print("Total Iterations                  :%d" % self.stats['iteration'])
        print("Max Rate                          :%s       " % self.convert_rate(float(self.stats['max_rate_bps'])))
        print("Optimal P-Drop Rate               :%s" % self.convert_rate(float(self.stats['rate_tx_bps'])))
        print("P-Drop Rate (%% of max)            :%0.2f %%" % self.stats['rate_p'])
        print("Drop Rate at Optimal P-Drop Rate  :%0.2f %% of oPackets" % self.stats['drop_rate_percentage'])
        self.print_run_stats(latency)
        for x in self.stats['ndr_points']:
            print("NDR(s)                            :%s " % self.convert_rate(x))
        if self.config:
            print("Packet Size                       :%s " % self.config.pkt_size)
            print("VM                                :%s " % self.config.vm)
            print("Ports                             :%s " % self.config.ports)
            print("Cores                             :%d " % self.config.cores)

    def print_assumed_drop_rate(self, low_bound, high_bound):
        print("\nResult of max rate measurement    :Drops Occured, Assuming NDR in following interval")
        print("Interval                          :[%d,%d]" % (low_bound, high_bound))
        print("Max Rate                          :%s       " % self.convert_rate(
            float(self.stats['max_rate_bps'])))
        print("Drop Rate                         :%0.2f %% of oPackets" % self.stats['drop_rate_percentage'])
        print("Assuming initial no-Drop-Rate     :%0.2f  %% of Max Rate" % self.stats['assumed_drop_rate_percent'])
        print("Assuming initial no-Drop-Rate     :%s" % self.convert_rate(self.stats['assumed_drop_rate']))

    def to_json(self):
        total_output = {'results': self.stats, 'config': self.config.config_to_dict()}
        return json.dumps(total_output)

    @staticmethod
    def print_state(state, high_bound, low_bound):
        print("\n\nStatus                            :%s" % state)
        if high_bound:
            print("Interval                          :[%d,%d]" % (low_bound, high_bound))


class NdrBench:
    def __init__(self, stl_client, config):
        self.config = config
        self.results = NdrBenchResults(config)
        self.stl_client = stl_client
        self.results.update({'title': self.config.title})

    def perf_run(self, rate_mb_percent, run_max=False):
        self.stl_client.clear_stats()
        if run_max:
            self.stl_client.start(ports=self.config.transmit_ports, mult="100%",
                                  duration=self.config.iteration_duration, core_mask=self.config.core_mask,
                                  total=True)
            rate_mb_percent = 100
        else:
            m_rate = Rate(self.results.stats['max_rate_bps'])
            run_rate = m_rate.convert_percent_to_rate(rate_mb_percent)
            self.stl_client.start(ports=self.config.transmit_ports, mult=str(run_rate) + "bps",
                                  duration=self.config.iteration_duration, core_mask=self.config.core_mask,
                                  total=True)
        time.sleep(self.config.iteration_duration / 2)
        stats = self.stl_client.get_stats()
        self.stl_client.stop(ports=self.config.ports)
        opackets = stats['total']['opackets']
        ipackets = stats['total']['ipackets']
        lost_p = opackets - ipackets
        lost_p_percentage = (float(lost_p) / float(opackets)) * 100.00
        if lost_p_percentage < 0:
            lost_p_percentage = 0
        q_ful_packets = stats['global']['queue_full']
        q_ful_percentage = float((q_ful_packets / float(opackets)) * 100.000)
        latency_groups = {}
        if self.config.latencyCalculation:
            latency_groups = {}
            for i in range(0, int(len(self.config.ports) / 2)):
                latency_dict = stats['latency'][i]['latency']
                latency_dict.update({'latency_rate': self.config.latency_rate})
                latency_groups[i] = latency_dict

        run_results = {'queue_full_percentage': q_ful_percentage, 'drop_rate_percentage': lost_p_percentage,
                       'rate_tx_bps': stats['total']['tx_bps'],
                       'rate_rx_bps': stats['total']['rx_bps'],
                       'tx_util': stats['total']['tx_util'], 'latency': latency_groups,
                       'cpu_util': stats['global']['cpu_util'], 'tx_pps': stats['total']['tx_pps'],
                       'bw_per_core': stats['global']['bw_per_core'], 'rx_pps': stats['total']['rx_pps'],
                       'rate_p': float(rate_mb_percent), 'total_tx_L1': stats['total']['tx_bps_L1'],
                       'total_rx_L1': stats['total']['rx_bps_L1'], 'tx_bps': stats['total']['tx_bps'],
                       'rx_bps': stats['total']['rx_bps']}
        return run_results

    def __find_max_rate(self):
        if self.config.verbose:
            self.results.print_state("Calculation of max rate for DUT", None, None)
        run_results = self.perf_run(100, True)
        run_results['max_rate_bps'] = min(float(run_results['rate_tx_bps']), float(run_results['rate_rx_bps']))
        self.results.update(run_results)
        if self.results.stats['drop_rate_percentage'] < 0:
            self.results.stats['drop_rate_percentage'] = 0
        if self.config.verbose:
            if run_results['rate_tx_bps'] < run_results['rate_rx_bps']:
                self.results.print_state("TX rate is slower than RX rate", None, None)
            self.results.print_iteration_data(self.config.latencyCalculation)

    def perf_run_interval(self, high_bound, low_bound):
        """

        Args:
            high_bound: in percents of rate
            low_bound: in percents of rate

        Returns: ndr in interval between low and high bounds (opt), q_ful(%) at opt and drop_rate(%) at opt

        """
        current_run_results = NdrBenchResults()
        current_run_stats = {}
        max_rate = Rate(self.results.stats['max_rate_bps'])
        current_run_stats['max_rate_bps'] = max_rate.rate
        opt_run_stats = {}
        opt_run_stats['rate_p'] = 0  # percent
        current_run_stats['iteration'] = 0
        while current_run_stats['iteration'] <= self.config.max_iterations:
            current_run_stats['rate_p'] = float((high_bound + low_bound)) / 2.00
            current_run_stats['rate_tx_bps'] = max_rate.convert_percent_to_rate(current_run_stats['rate_p'])
            current_run_stats.update(self.perf_run(str(current_run_stats['rate_p'])))
            lost_p_percentage = current_run_stats['drop_rate_percentage']
            q_ful_percentage = current_run_stats['queue_full_percentage']
            current_run_stats['rate_diffrential'] = abs(current_run_stats['rate_p'] - opt_run_stats['rate_p'])
            current_run_results.update(current_run_stats)
            if self.config.verbose:
                if q_ful_percentage > self.config.q_ful_resolution:
                    current_run_results.print_state("Queue Full Occurred", high_bound, low_bound)
                elif lost_p_percentage > self.config.pdr:
                    current_run_results.print_state("Drops beyond Desired rate occurred", high_bound, low_bound)
                else:
                    current_run_results.print_state("Looking for NDR", high_bound, low_bound)
                current_run_results.print_iteration_data(self.config.latencyCalculation)
                if current_run_stats['iteration'] == self.config.max_iterations:
                    opt_run_stats.update(current_run_stats)
                    self.results.print_state("Max Iterations reached. Results might not be fully accurate", high_bound,
                                             low_bound)
            if q_ful_percentage <= self.config.q_ful_resolution and lost_p_percentage <= self.config.pdr:
                if current_run_stats['rate_p'] > opt_run_stats['rate_p']:
                    opt_run_stats.update(current_run_stats)
                    if current_run_stats['rate_diffrential'] <= self.config.pdr_error:
                        break
                    low_bound = current_run_stats['rate_p']
                    current_run_stats['iteration'] += 1
                    continue
                else:
                    break
            else:
                if current_run_stats['rate_diffrential'] <= self.config.pdr_error:
                    break
            high_bound = current_run_stats['rate_p']
            current_run_stats['iteration'] += 1
        opt_run_stats['iteration'] = current_run_stats['iteration']
        return opt_run_stats

    def find_ndr(self):
        self.__find_max_rate()
        drop_percent = self.results.stats['drop_rate_percentage']
        q_ful_percent = self.results.stats['queue_full_percentage']
        max_rate = Rate(self.results.stats['max_rate_bps'])
        if drop_percent > self.config.pdr:
            assumed_rate_percent = 100 - drop_percent  # percent calculation
            assumed_rate_mb = max_rate.convert_percent_to_rate(assumed_rate_percent)
            self.results.update(
                {'assumed_drop_rate_percent': assumed_rate_percent, 'assumed_drop_rate': assumed_rate_mb})
            if self.config.verbose:
                self.results.print_assumed_drop_rate(max(assumed_rate_percent - self.config.drop_rate_interval, 0),
                                                     min(assumed_rate_percent + self.config.drop_rate_interval, 100))
                self.results.update(
                    self.perf_run_interval(min(assumed_rate_percent + self.config.drop_rate_interval, 100),
                                           max(assumed_rate_percent - self.config.drop_rate_interval, 0)))
        elif q_ful_percent >= self.config.q_ful_resolution:
            if self.config.verbose:
                self.results.print_state("DUT Queue is Full, Looking for no queue full rate", 100, 0)
            self.results.update(self.perf_run_interval(100.00, 0.00))
        else:
            if self.config.verbose:
                self.results.print_state("NDR may be found at max rate, running extra iteration for validation", None,
                                         None)
            self.results.update(self.perf_run_interval(100.00, 99.00))
        ndr_res = [self.results.stats['rate_tx_bps']]
        if self.config.ndr_results > 1:
            ndr_range = range(1, self.config.ndr_results + 1, 1)
            ndr_range.reverse()
            ndr_res = [float((self.results.stats['rate_tx_bps'])) / float(t) for t in ndr_range]
        self.results.update({'ndr_points': ndr_res})
