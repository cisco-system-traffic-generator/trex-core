from trex_stl_lib.api import *
import random

class STLMultiCore(object):

    def __init__ (self):
        # default IMIX properties
        self.streams_def = [ {'size': 300,   'pps': 2884,  'isg':0 },
                            #{'size': 590,  'pps': 20,  'isg':0.1 },
                            #{'size': 1514, 'pps': 4,   'isg':0.2 } 
                            ]


    def create_stream (self, size, pps, isg, vm ):
        # Create base packet and pad it to size
        base_pkt = Ether()/IP()/UDP(sport = 1500, dport = 1500)
        pad = max(0, size - len(base_pkt)) * b'\xff'

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(isg = isg,
                         packet = pkt,
                         mode = STLTXCont(pps = pps))


    def generate_var (self, rng, i, vm, pkt_offset, verbose = False):

        name = "var-{0}".format(i)

        size  = rng.choice([1, 2, 4])
        bound = (1 << (size * 8)) - 1

        min_value = rng.randint(0, bound - 1)
        max_value = rng.randint(min_value, bound)
        step      = rng.randint(1, 1000)
        op        = rng.choice(['inc', 'dec'])
        
        vm += [STLVmFlowVar(name      = name,
                            min_value = min_value,
                            max_value = max_value,
                            size      = size,
                            op        = op),
               STLVmWrFlowVar(fv_name = name, pkt_offset = pkt_offset),
               ]

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
        vm += [STLVmTupleGen(ip_min = ip_min, ip_max = ip_max, 
                             port_min = port_min, port_max = port_max,
                             name = name,
                             limit_flows = limit_flows),

               STLVmWrFlowVar (fv_name = name + ".ip", pkt_offset = pkt_offset ), # write ip to packet IP.src]
               STLVmWrFlowVar (fv_name = name + ".port", pkt_offset = (pkt_offset + 4) ),
               ]

        if verbose:
            print('name: {:}, ip_start: {:}, ip_end: {:}, port_start: {:}, port_end: {:}'.format(name,
                                                                                                 ip_min,
                                                                                                 ip_max,
                                                                                                 port_min,
                                                                                                 port_max))

        return 8
        



    def get_streams (self, direction = 0, **kwargs):
      
        rng = random.Random(kwargs.get('seed', 1))
        test_type = kwargs.get('test_type', 'plain')

        # base offset
        pkt_offset = 42

        vm = []

        if test_type == 'plain':
            for i in range(20):
                pkt_offset += self.generate_var(rng, i, vm, pkt_offset)
        elif test_type == 'tuple':
            for i in range(5):
                pkt_offset += self.generate_tuple_var(rng, i, vm, pkt_offset)
        else:
            raise STLError('unknown mutli core test type')


        # create imix streams
        return [self.create_stream(x['size'], x['pps'],x['isg'] , vm) for x in self.streams_def]



# dynamic load - used for trex console or simulator
def register():
    return STLMultiCore()



