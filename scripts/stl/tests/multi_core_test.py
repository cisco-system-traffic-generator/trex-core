from trex_stl_lib.api import *
import random

class STLMultiCore(object):

    def __init__ (self):
        # default IMIX properties
        self.streams_def = [ {'size': 300,   'pps': 1,  'isg':0 },
                            #{'size': 590,  'pps': 20,  'isg':0.1 },
                            #{'size': 1514, 'pps': 4,   'isg':0.2 } 
                            ]

        self.tests = {'vars': self.test_var, 'tuple': self.test_tuple, 'topology': self.test_topology}


    def create_stream (self, size, pps, isg, vm = None, mode = "cont", name = None, next = None, self_start = True, pg_id = None):
        # Create base packet and pad it to size
        base_pkt = Ether()/IP()/UDP(sport = 1500, dport = 1500)
        pad = max(0, size - len(base_pkt)) * b'\xff'

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(name = name,
                         next = next,
                         isg = isg,
                         packet = pkt,
                         self_start = self_start,
                         flow_stats = STLFlowLatencyStats(pg_id = pg_id) if pg_id is not None else None,
                         mode = STLTXCont(pps = pps) if mode == "cont" else STLTXSingleBurst(total_pkts = 100, pps = pps))


    def generate_var (self, rng, i, vm, pkt_offset, verbose = False):

        name = "var-{0}".format(i)

        size  = rng.choice([1, 2, 4])
        bound = (1 << (size * 8)) - 1

        min_value = rng.randint(0, bound - 1)
        max_value = rng.randint(min_value, bound)
        step      = rng.randint(1, 1000)
        op        = rng.choice(['inc', 'dec'])
    
        vm.var(name      = name,
               min_value = min_value,
               max_value = max_value,
               size      = size,
               op        = op)
        
        vm.write(fv_name = name, pkt_offset = pkt_offset)

        if verbose:
            print('name: {:}, start: {:}, end: {:}, size: {:}, op: {:}, step {:}'.format(name,
                                                                                         min_value,
                                                                                         max_value,
                                                                                         size,
                                                                                         op,
                                                                                         step))

        return size


    def generate_tuple_var (self, rng, i, vm, pkt_offset, verbose = False):
        name = "tuple-{0}".format(i)

        # ip
        ip_bound = 12
        ip_min = rng.randint(0, ip_bound - 1)
        ip_max = rng.randint(ip_min, ip_bound)
        
        # port
        port_bound = 3
        port_min = rng.randint(0, port_bound - 1)
        port_max = rng.randint(port_min, port_bound - 1)
        
        # 840 is the least common multiple
        limit_flows = 840 * rng.randint(1, 1000)
        
        vm.tuple_var(ip_min = ip_min, ip_max = ip_max, 
                     port_min = port_min, port_max = port_max,
                     name = name,
                     limit_flows = limit_flows)
        
        vm.write(fv_name = name + ".ip", pkt_offset = pkt_offset) # write ip to packet IP.src
        vm.write(fv_name = name + ".port", pkt_offset = (pkt_offset + 4) )
        
        if verbose:
            print('name: {:}, ip_start: {:}, ip_end: {:}, port_start: {:}, port_end: {:}'.format(name,
                                                                                                 ip_min,
                                                                                                 ip_max,
                                                                                                 port_min,
                                                                                                 port_max))

        return 8
        


    def test_var (self):
        pkt_offset = 42

        vm = STLVM()
        for i in range(20):
            pkt_offset += self.generate_var(self.rng, i, vm, pkt_offset)

        return [self.create_stream(300, 1, 0, vm)]


    def test_tuple (self):
        pkt_offset = 42

        vm = STLVM()
        for i in range(5):
            pkt_offset += self.generate_tuple_var(self.rng, i, vm, pkt_offset)

        return [self.create_stream(300, 1, 0, vm)]


    def generate_single_var (self):
        vm = STLVM()
        self.generate_var(self.rng, 1, vm, 42)
        return vm


    def test_topology (self):
        pkt_offset = 42

        # two continous streams
        s1 = self.create_stream(134, 1, 0, self.generate_single_var())
        s2 = self.create_stream(182, 2, 11, self.generate_single_var())

        # two bursts
        s3 = self.create_stream(142, 3, 23,  vm = self.generate_single_var(), mode = "burst")
        s4 = self.create_stream(153, 5, 37,  vm = None, mode = "burst")

        # single burst pointing to cont.
        s5 = self.create_stream(117, 7, 48,  vm = self.generate_single_var(), mode = "burst", name = "s5", next = "s6")
        s6 = self.create_stream(121, 3, 51,  vm = self.generate_single_var(), mode = "burst", name = "s6", self_start = False)

        # latency pointing at each other
        s7 = self.create_stream(113, 7, 48,  vm = self.generate_single_var(), mode = "burst", name = "s7", next = "s8", pg_id = 1)
        s8 = self.create_stream(127, 3, 51,  vm = self.generate_single_var(), mode = "cont", name = "s8", self_start = False, pg_id = 2)

        return [s1, s2, s3, s4, s5, s6, s7, s8]


    def get_streams (self, direction = 0, **kwargs):

        self.rng = random.Random(kwargs.get('seed', 1))

        test_type = kwargs.get('test_type', 'vars')

        func = self.tests.get(test_type, None)
        if not func:
            raise STLError('unknown mutli core test type')

        return func()



# dynamic load - used for trex console or simulator
def register():
    return STLMultiCore()



