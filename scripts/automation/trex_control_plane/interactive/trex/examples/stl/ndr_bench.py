# from __future__ import division
import stl_path
from trex.stl.api import *

import json
import argparse
import sys
import time
import math
import imp
from copy import deepcopy


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
        base_pkt = Ether() / IP(src=src, dst=dst) / UDP(chksum=0)
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

    def is_close(self, rate, rel_tol=0.05, abs_tol=1000000):
        """
        rel_tol: is a relative tolerance, it is multiplied by the greater of the magnitudes of the two arguments; 
                 as the values get larger, so does the allowed difference between them while still considering them equal.
                 Default Value = 5%.

        abs_tol: is an absolute tolerance that is applied as-is in all cases. 
                 If the difference is less than either of those tolerances, the values are considered equal.
                 Default value = 1 Mbit
        """
        return abs(self.rate-rate) <= max(rel_tol * max(abs(self.rate), abs(rate)), abs_tol)


class NdrBenchConfig:
    def __init__(self, ports, pkt_size=64, vm='cached', title='Title', cores=1, latency_rate=1000,
                 iteration_duration=20.00,
                 q_full_resolution=2.00,
                 first_run_duration=20.00, pdr=0.1, pdr_error=1.0, ndr_results=1, max_iterations=10, latency=True,
                 verbose=False, bi_dir=False, plugin_file=None, tunables={},
                 opt_binary_search=False, opt_binary_search_percentage=5, **kwargs):
        self.bi_dir = bi_dir
        # The sleep call divides the duration by 2
        self.iteration_duration = (iteration_duration * 2)
        self.q_full_resolution = q_full_resolution
        self.first_run_duration = first_run_duration
        self.pdr = pdr  # desired percent of drop-rate. pdr = 0 is NO drop-rate
        self.pdr_error = pdr_error
        self.ndr_results = ndr_results
        self.max_iterations = max_iterations
        self.latencyCalculation = latency
        self.verbose = verbose
        self.pkt_size = pkt_size
        self.vm = vm
        self.title = title
        self.latency_rate = latency_rate
        self.cores = cores
        self.ports = list(ports)
        self.transmit_ports = [self.ports[i] for i in range(0, len(self.ports), 2)]
        self.receive_ports  = [self.ports[i] for i in range(1, len(self.ports), 2)]
        if self.bi_dir:
            self.transmit_ports = self.ports
            self.receive_ports  = self.ports
        self.transmit_core_masks = self.get_optimal_core_mask(num_of_cores=self.cores, num_of_ports=len(self.ports))
        self.plugin_file = plugin_file
        self.plugin_enabled = False
        if self.plugin_file is not None:
            self.plugin_enabled = True
            self.plugin = self.load_plugin(self.plugin_file)
        self.tunables = tunables
        self.opt_binary_search = opt_binary_search
        self.opt_binary_search_percentage = opt_binary_search_percentage

    def get_optimal_core_mask(self, num_of_cores, num_of_ports):
        if not self.bi_dir:
            return None
        else:
            # Half of the cores are given to the first port and the second half to second port.
            # In case of an odd number of cores, one core (the middle one) is shared.
            half_of_cores = int(math.ceil(num_of_cores / 2.00))
            first_mask = (1 << num_of_cores) - (1 << (num_of_cores - half_of_cores))
            second_mask = (1 << half_of_cores) - 1
            return [first_mask, second_mask] * int(num_of_ports / 2)

    @classmethod
    def load_plugin(cls, plugin_file):
        """ Load dynamically a plugin module so that we can provide the user with pre and post iteration API."""

        # check filename
        if not os.path.isfile(plugin_file):
            raise TRexError("File '{0}' does not exist".format(plugin_file))

        basedir = os.path.dirname(plugin_file)
        sys.path.insert(0, basedir)

        try:
            file    = os.path.basename(plugin_file).split('.')[0]
            module = __import__(file, globals(), locals(), [], 0)
            imp.reload(module) # reload the update 

            plugin = module.register()

            return plugin

        except Exception as e:
            a, b, tb = sys.exc_info()
            x =''.join(traceback.format_list(traceback.extract_tb(tb)[1:])) + a.__name__ + ": " + str(b) + "\n"

            summary = "\nPython Traceback follows:\n\n" + x
            raise TRexError(summary)


        finally:
            sys.path.remove(basedir)

    def config_to_dict(self):
        config_dict = {'iteration_duration': self.iteration_duration, 'q_full_resolution': self.q_full_resolution,
                       'first_run_duration': self.first_run_duration, 'pdr': self.pdr, 'pdr_error': self.pdr_error,
                       'ndr_results': self.ndr_results, 'max_iterations': self.max_iterations,
                       'latencyCalculation': self.latencyCalculation, 'ports': self.ports,
                       'pkt_size': self.pkt_size, 'vm': self.vm,
                       'cores': self.cores, 'verbose': self.verbose, 'title': self.title,
                       'latency_rate': self.latency_rate, 'bi_dir' : self.bi_dir,
                       'plugin_file': self.plugin_file, 'tunables': self.tunables,
                       'opt_binary_search': self.opt_binary_search,
                       'opt_binary_search_percentage': self.opt_binary_search_percentage}
        return config_dict


class NdrBenchResults:
    def __init__(self, config=None, results={}):
        self.stats = dict(results)
        self.init_time = float(time.time())

    def update(self, updated_dict):
        updated_dict['Elapsed Time'] = (float(time.time()) - self.init_time)
        self.stats.update(updated_dict)

    def convert_rate(self, rate_bps, packet=False):
        converted = float(rate_bps)
        magnitude = 0
        while converted > 1000.00:
            magnitude += 1
            converted /= 1000.00
        converted = round(converted, 2)
        if packet:
            postfix = "PPS"
        else:
            postfix = "bps"
        if magnitude == 0:
            return str(converted) + postfix
        elif magnitude == 1:
            converted = round(converted)
            return str(converted) + " K" + postfix
        elif magnitude == 2:
            return str(converted) + " M" + postfix
        elif magnitude == 3:
            return str(converted) + " G" + postfix

    def print_latency(self):
        try:
            for k in self.stats['latency'].keys():
                print("Latency stats on port             :%d" % k)
                print ("    Latency Rate                  :%0.2f PPS" % self.stats['latency'][k]['latency_rate'])
                print ("    Average                       :%0.2f" % self.stats['latency'][k]['average'])
                print ("    Jitter                        :%0.2f" % self.stats['latency'][k]['jitter'])
                print ("    Total Max                     :%0.2f" % self.stats['latency'][k]['total_max'])
                print ("    Total Min                     :%0.2f" % self.stats['latency'][k]['total_min'])
                print ("    Histogram                     :%s   " % self.stats['latency'][k]['histogram'])
        except TypeError:
            pass

    def print_run_stats(self, latency, bi_dir):
        if bi_dir:
            traffic_dir = "bi-directional "
        else:
            traffic_dir = "uni-directional"
        print("Elapsed Time                      :%0.2f seconds" % self.stats['Elapsed Time'])
        print("BW Per Core                       :%0.2f Gbit/Sec @100%% per core" % float(self.stats['bw_per_core']))
        print("TX PPS %s            :%s" % (traffic_dir, self.convert_rate(float(self.stats['tx_pps']), True)))
        print("RX PPS %s            :%s" % (traffic_dir, self.convert_rate(float(self.stats['rx_pps']), True)))
        print("TX Utilization                    :%0.2f %%" % self.stats['tx_util'])
        print("TRex CPU                          :%0.2f %%" % self.stats['cpu_util'])
        print("Total TX L1 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['total_tx_L1']))))
        print("Total RX L1 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['total_rx_L1']))))
        print("Total TX L2 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['tx_bps']))))
        print("Total RX L2 %s       :%s     " % (traffic_dir, self.convert_rate(float(self.stats['rx_bps']))))
        if 'rate_difference' in self.stats:
            print("Distance from current Optimum     :%0.2f %%" % self.stats['rate_difference'])

        if latency:
            self.print_latency()

    def print_iteration_data(self, latency, bi_dir):
        if 'title' in self.stats:
            print("\nTitle                             :%s" % self.stats['title'])
        if 'iteration' in self.stats:
            print("Iteration                         :{}".format(self.stats['iteration']))
        print("Running Rate                      :%s" % self.convert_rate(float(self.stats['rate_tx_bps'])))
        print("Running Rate (%% of max)           :%0.2f %%" % self.stats['rate_p'])
        print("Max Rate                          :%s       " % self.convert_rate(float(self.stats['max_rate_bps'])))
        print("Drop Rate                         :%0.5f %% of oPackets" % self.stats['drop_rate_percentage'])
        print("Queue Full                        :%0.2f %% of oPackets" % self.stats['queue_full_percentage'])
        self.print_run_stats(latency, bi_dir)

    def print_final(self, latency, bi_dir):
        if bi_dir:
            traffic_dir = "bi-directional "
        else:
            traffic_dir = "uni-directional"
        print("\nTitle                             :%s" % self.stats['title'])
        if 'iteration' in self.stats:
            print("Total Iterations                  :{}".format(self.stats['iteration']))
        print("Max Rate                          :%s       " % self.convert_rate(float(self.stats['max_rate_bps'])))
        print("Optimal P-Drop Rate               :%s" % self.convert_rate(float(self.stats['rate_tx_bps'])))
        print("P-Drop Rate (%% of max)            :%0.2f %%" % self.stats['rate_p'])
        print("Drop Rate at Optimal P-Drop Rate  :%0.5f %% of oPackets" % self.stats['drop_rate_percentage'])
        print("Queue Full at Optimal P-Drop Rate :%0.2f %% of oPackets" % self.stats['queue_full_percentage'])
        self.print_run_stats(latency, bi_dir)
        for x in self.stats['ndr_points']:
            print("NDR(s) %s            :%s " % (traffic_dir, self.convert_rate(x)))

    def to_json(self):
        total_output = {'results': self.stats, 'config': self.config.config_to_dict()}
        return json.dumps(total_output)

    @staticmethod
    def print_state(state, high_bound, low_bound):
        print("\n\nStatus                            :%s" % state)
        if high_bound:
            print("Interval                          :[%d,%d]" % (low_bound, high_bound))

    def human_readable_dict(self):
        hu_dict = {'Queue Full [%]': str(round(self.stats['queue_full_percentage'], 2)) + "%",
                   'BW per core [Gbit/sec @100% per core]': str(
                       round(float(self.stats['bw_per_core']), 2)) + 'Gbit/Sec @100% per core',
                   'RX [MPPS]': self.convert_rate(float(self.stats['rx_pps']), True),
                   'TX [MPPS]': self.convert_rate(float(self.stats['tx_pps']), True),
                   'Line Utilization [%]': str(round(self.stats['tx_util'], 2)),
                   'CPU Utilization [%]': str(round(self.stats['cpu_util'],2)),
                   'Total TX L1': self.convert_rate(float(self.stats['total_tx_L1'])),
                   'Total RX L1': self.convert_rate(float(self.stats['total_rx_L1'])),
                   'TX [bps]': self.convert_rate(float(self.stats['tx_bps'])),
                   'RX [bps]': self.convert_rate(float(self.stats['rx_bps'])),
                   'OPT TX Rate [bps]': self.convert_rate(float(self.stats['rate_tx_bps'])),
                   'OPT RX Rate [bps]': self.convert_rate(float(self.stats['rate_rx_bps'])),
                   'OPT Rate (Multiplier) [%]': str(self.stats['rate_p']),
                   'Max Rate [bps]': self.convert_rate(float(self.stats['max_rate_bps'])),
                   'Drop Rate [%]': str(round(self.stats['drop_rate_percentage'], 2)),
                   'Elapsed Time [Sec]': str(round(self.stats['Elapsed Time'], 2)),
                   'NDR points': [self.convert_rate(float(x)) for x in self.stats['ndr_points']],
                   'Total Iterations': self.stats.get('iteration', None),
                   'Title': self.stats['title'],
                   'latency': dict(self.stats['latency'])}

        return hu_dict


class NdrBench:
    def __init__(self, stl_client, config):
        self.config = config
        self.results = NdrBenchResults(config)
        self.stl_client = stl_client
        self.results.update({'title': self.config.title})
        self.opt_run_stats = {}
        self.opt_run_stats['rate_p'] = 0 # percent

    def plugin_pre_iteration(self, finding_max_rate, run_results=None, **kwargs):
        self.config.plugin.pre_iteration(finding_max_rate, deepcopy(run_results), **kwargs)

    def plugin_post_iteration(self, finding_max_rate, run_results, **kwargs):
        return self.config.plugin.post_iteration(finding_max_rate, deepcopy(run_results), **kwargs)

    def max_iterations_reached(self, current_run_stats, high_bound, low_bound):
        if current_run_stats['iteration'] == self.config.max_iterations:
            self.opt_run_stats.update(current_run_stats)
            if self.config.verbose:
                self.results.print_state("Max Iterations reached. Results might not be fully accurate", high_bound,
                                         low_bound)
            return True
        return False

    def calculate_ndr_points(self):
        ndr_res = [self.results.stats['tx_bps']]
        if self.config.ndr_results > 1:
            ndr_range = range(1, self.config.ndr_results + 1, 1)
            ndr_range.reverse()
            ndr_res = [float((self.results.stats['tx_bps'])) / float(t) for t in ndr_range]
        self.results.update({'ndr_points': ndr_res})

    def update_opt_stats(self, new_stats):
        if new_stats['queue_full_percentage'] <= self.config.q_full_resolution and new_stats['drop_rate_percentage'] <= self.config.pdr:
            if new_stats['rate_p'] > self.opt_run_stats['rate_p']:
                self.opt_run_stats.update(new_stats)

    def optimized_binary_search(self, lost_percentage, lost_allowed_percentage, stat_type):
        max_rate = Rate(self.results.stats['max_rate_bps'])
        plugin_enabled = self.config.plugin_enabled
        plugin_stop = False

        current_run_stats = deepcopy(self.results.stats)

        assumed_ndr_rate = Rate(max_rate.convert_percent_to_rate(100 - lost_percentage))
        upper_bound = min(max_rate.rate, assumed_ndr_rate.convert_percent_to_rate(100 + self.config.opt_binary_search_percentage))
        upper_bound_percentage_of_max_rate = max_rate.convert_rate_to_percent_of_max_rate(upper_bound)

        if not max_rate.is_close(upper_bound):
            # in case we are not close to the max rate, try with the upper bound of the assumed NDR
            current_run_stats['rate_p'] = upper_bound_percentage_of_max_rate
            current_run_stats['rate_tx_bps'] = upper_bound
            current_run_stats['iteration'] = "Upper bound of assumed NDR"
            if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            if self.config.verbose:
                self.results.print_state("Trying upper bound of assumed rate!", None, None)
            current_run_stats.update(self.perf_run(upper_bound_percentage_of_max_rate))
            self.results.update(current_run_stats)
            self.update_opt_stats(current_run_stats)
            if self.config.verbose:
                self.results.print_iteration_data(self.config.latencyCalculation, self.config.bi_dir)
            if plugin_enabled:
                plugin_stop = self.plugin_post_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            if plugin_stop:
                if self.config.verbose:
                    self.results.print_state("Plugin decided to stop trying upper bound of assumed rate!", None, None)
                return current_run_stats

            upper_bound_lost_percentage = current_run_stats[stat_type]
            if upper_bound_lost_percentage <= lost_allowed_percentage:
                if self.config.verbose:
                    self.results.print_state("Upper bound of assumed NDR drops are below desired rate :)",\
                                             100, upper_bound_percentage_of_max_rate)
                return self.perf_run_interval(100, upper_bound_percentage_of_max_rate)

        # if you got here -> upper bound of assumed ndr has too many drops
        lower_bound = assumed_ndr_rate.convert_percent_to_rate(100 - self.config.opt_binary_search_percentage)
        lower_bound_percentage_of_max_rate = max_rate.convert_rate_to_percent_of_max_rate(lower_bound)

        current_run_stats['rate_p'] = lower_bound_percentage_of_max_rate
        current_run_stats['rate_tx_bps'] = lower_bound
        current_run_stats['iteration'] = "Lower bound of assumed NDR"
        if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
        if self.config.verbose:
            self.results.print_state("Trying lower bound of assumed rate!", None, None)
        current_run_stats.update(self.perf_run(lower_bound_percentage_of_max_rate))
        self.results.update(current_run_stats)
        self.update_opt_stats(current_run_stats)
        if self.config.verbose:
                self.results.print_iteration_data(self.config.latencyCalculation, self.config.bi_dir)
        if plugin_enabled:
                plugin_stop = self.plugin_post_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
        if plugin_stop:
            if self.config.verbose:
                self.results.print_state("Plugin decided to stop trying lower bound of assumed rate!", None, None)
            return current_run_stats

        lower_bound_lost_percentage = current_run_stats[stat_type]
        if lower_bound_lost_percentage <= lost_allowed_percentage:
            self.results.print_state("Lower bound of assumed NDR drops are below desired rate :)",\
                                     upper_bound_percentage_of_max_rate, lower_bound_percentage_of_max_rate)
            return self.perf_run_interval(upper_bound_percentage_of_max_rate, lower_bound_percentage_of_max_rate)

        # if you got here -> lower bound of assumed ndr has too many drops
        else:
            self.results.print_state("Lower bound of assumed NDR drops are beyond desired rate :(",\
                                     lower_bound_percentage_of_max_rate, 0)
            return self.perf_run_interval(lower_bound_percentage_of_max_rate, 0)

    def perf_run(self, rate_mb_percent, run_max=False):
        self.stl_client.stop(ports=self.config.ports)
         # allow time for counters to settle from previous runs
        time.sleep(15)
        self.stl_client.clear_stats()
        if run_max:
            self.stl_client.start(ports=self.config.transmit_ports, mult="100%",
                                  duration=self.config.iteration_duration, core_mask=self.config.transmit_core_masks)
            rate_mb_percent = 100
        else:
            m_rate = Rate(self.results.stats['max_rate_bps'])
            if rate_mb_percent == 0:
                rate_mb_percent += 1
            run_rate = m_rate.convert_percent_to_rate(rate_mb_percent)
            self.stl_client.start(ports=self.config.transmit_ports, mult=str(run_rate) + "bps",
                                  duration=self.config.iteration_duration, core_mask=self.config.transmit_core_masks)
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
            for i in stats['latency'].keys():
                if type(i) != int:
                    continue
                latency_dict = stats['latency'][i]['latency']
                latency_dict.update({'latency_rate': self.config.latency_rate})
                latency_groups[i] = latency_dict
        tx_bps = [stats[x]['tx_bps'] for x in self.config.transmit_ports]
        rx_bps = [stats[x]['rx_bps'] for x in self.config.receive_ports]
        tx_util_norm = sum([stats[x]['tx_util'] for x in self.config.transmit_ports]) / len(self.config.transmit_ports)
        run_results = {'queue_full_percentage': q_ful_percentage, 'drop_rate_percentage': lost_p_percentage,
                       'rate_tx_bps': min(tx_bps),
                       'rate_rx_bps': min(rx_bps),
                       'tx_util': tx_util_norm, 'latency': latency_groups,
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
        run_results['max_rate_bps'] = min(float(run_results['rate_tx_bps']), float(
            run_results['rate_rx_bps']))
        self.results.update(run_results)
        if self.results.stats['drop_rate_percentage'] < 0:
            self.results.stats['drop_rate_percentage'] = 0
        if self.config.verbose:
            if run_results['rate_tx_bps'] < run_results['rate_rx_bps']:
                self.results.print_state("TX rate is slower than RX rate", None, None)
            self.results.print_iteration_data(self.config.latencyCalculation, self.config.bi_dir)
        return run_results

    def perf_run_interval(self, high_bound, low_bound):
        """

        Args:
            high_bound: in percents of rate
            low_bound: in percents of rate

        Returns: ndr in interval between low and high bounds (opt), q_full(%) at opt and drop_rate(%) at opt

        """
        current_run_results = NdrBenchResults()
        current_run_stats = self.results.stats
        max_rate = Rate(self.results.stats['max_rate_bps'])
        current_run_stats['max_rate_bps'] = max_rate.rate
        current_run_stats['iteration'] = 0
        plugin_enabled = self.config.plugin_enabled
        plugin_stop = False
        while current_run_stats['iteration'] <= self.config.max_iterations:
            current_run_stats['rate_p'] = float((high_bound + low_bound)) / 2.00
            current_run_stats['rate_tx_bps'] = max_rate.convert_percent_to_rate(current_run_stats['rate_p'])
            if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            current_run_stats.update(self.perf_run(str(current_run_stats['rate_p'])))
            if plugin_enabled:
                plugin_stop = self.plugin_post_iteration(finding_max_rate=False, run_results=current_run_stats, **self.config.tunables)
            lost_p_percentage = current_run_stats['drop_rate_percentage']
            q_full_percentage = current_run_stats['queue_full_percentage']
            current_run_stats['rate_difference'] = abs(current_run_stats['rate_p'] - self.opt_run_stats['rate_p'])
            current_run_results.update(current_run_stats)
            if self.config.verbose:
                if q_full_percentage > self.config.q_full_resolution:
                    current_run_results.print_state("Queue Full Occurred", high_bound, low_bound)
                elif lost_p_percentage > self.config.pdr:
                    current_run_results.print_state("Drops beyond Desired rate occurred", high_bound, low_bound)
                else:
                    current_run_results.print_state("Looking for NDR", high_bound, low_bound)
                current_run_results.print_iteration_data(self.config.latencyCalculation, self.config.bi_dir)

            if plugin_stop:
                if self.config.verbose:
                     current_run_results.print_state("Plugin decided to stop after the iteration!", high_bound, low_bound)
                self.update_opt_stats(current_run_stats)
                break

            if q_full_percentage <= self.config.q_full_resolution and lost_p_percentage <= self.config.pdr:
                if current_run_stats['rate_p'] > self.opt_run_stats['rate_p']:
                    self.opt_run_stats.update(current_run_stats)
                    if current_run_stats['rate_difference'] <= self.config.pdr_error:
                        break
                    low_bound = current_run_stats['rate_p']
                    current_run_stats['iteration'] += 1
                    if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                        break
                    else:
                        continue
                else:
                    break
            else:
                if current_run_stats['rate_difference'] <= self.config.pdr_error:
                    break
            high_bound = current_run_stats['rate_p']
            current_run_stats['iteration'] += 1
            if self.max_iterations_reached(current_run_stats, high_bound, low_bound):
                break

        self.opt_run_stats['iteration'] = current_run_stats['iteration']
        self.opt_run_stats['rate_difference'] = 0
        return self.opt_run_stats

    def find_ndr(self, bi_dir):
        plugin_enabled = self.config.plugin_enabled
        plugin_stop = False
        if plugin_enabled:
                self.plugin_pre_iteration(finding_max_rate=True, run_results=None, **self.config.tunables)
        first_run_results = self.__find_max_rate()
        self.update_opt_stats(first_run_results)
        if plugin_enabled:
            plugin_stop = self.plugin_post_iteration(finding_max_rate=True, run_results=first_run_results, **self.config.tunables)
        if plugin_stop:
            if self.config.verbose:
                self.results.print_state("Plugin decided to stop after trying to find the max rate!", None, None)
            self.calculate_ndr_points()
            return

        drop_percent = self.results.stats['drop_rate_percentage']
        q_full_percent = self.results.stats['queue_full_percentage']

        if drop_percent > self.config.pdr:
            if self.config.opt_binary_search:
                if self.config.verbose:
                    self.results.print_state("Drops happened, searching for NDR with optimized binary search", None, None)
                self.results.update(self.optimized_binary_search(drop_percent, self.config.pdr, 'drop_rate_percentage'))
            else:
                if self.config.verbose:
                    self.results.print_state("Drops happened, searching for NDR", 100, 0)
                self.results.update(self.perf_run_interval(100, 0))

        elif q_full_percent >= self.config.q_full_resolution:
            if self.config.opt_binary_search:
                if self.config.verbose:
                    self.results.print_state("DUT Queue is Full, Looking for no queue full rate with optimized binary search", None, None)
                self.results.update(self.optimized_binary_search(q_full_percent, self.config.q_full_resolution, 'queue_full_percentage'))
            else:
                if self.config.verbose:
                    self.results.print_state("DUT Queue is Full, Looking for no queue full rate", 100, 0)
                self.results.update(self.perf_run_interval(100.00, 0.00))

        else:
            if self.config.verbose:
                self.results.print_state("NDR found at max rate", None, None)

        self.calculate_ndr_points()


if __name__ == '__main__':
    print('Designed to be imported, not as stand-alone script.')
