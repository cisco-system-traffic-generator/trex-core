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


    def generate_var (self, rng, i, vm, pkt_offset):

        name = "var-{0}".format(i)

        size  = rng.choice([1, 2, 4])
        bound = (1 << (size * 8)) - 1

        min_value = rng.randint(0, bound - 1)
        max_value = rng.randint(min_value, bound)
        step      = rng.randint(1, 1000)
        op        = rng.choice(['inc', 'dec'])
        
        vm += [STLVmFlowVar(name      = str(i),
                            min_value = min_value,
                            max_value = max_value,
                            size      = size,
                            op        = op),
               STLVmWrFlowVar(fv_name = name, pkt_offset = pkt_offset),
               ]

        print('name: {:}, start: {:}, end: {:}, size: {:}, op: {:}, step {:}'.format(name,
                                                                                     min_value,
                                                                                     max_value,
                                                                                     size,
                                                                                     op,
                                                                                     step))

        return size


    def generate_tuple_var (self, rng, i, vm, pkt_offset):
        name = "tuple-{0}".format(i)

        # ip
        ip_bound = (1 << (4 * 8)) - 1
        ip_min = rng.randint(0, ip_bound - 1)
        ip_max = rng.randint(ip_min, ip_bound)

        # port
        port_bound = (1 << (2 * 8)) - 1
        port_min = rng.randint(0, port_bound - 1)
        port_max = rng.randint(port_min, port_bound - 1)

        vm += [STLVmTupleGen(ip_min = ip_min, ip_max = ip_max, 
                             port_min = port_min, port_max = port_max,
                             name = name),
               STLVmWrFlowVar (fv_name = name + ".ip", pkt_offset = pkt_offset ), # write ip to packet IP.src]
               STLVmWrFlowVar (fv_name = name + ".port", pkt_offset = (pkt_offset + 4) ),
               ]

        print('name: {:}, ip_start: {:}, ip_end: {:}, port_start: {:}, port_end: {:}'.format(name,
                                                                                             ip_min,
                                                                                             ip_max,
                                                                                             port_min,
                                                                                             port_max))

        return 8
        



    def get_streams (self, direction = 0, **kwargs):
      
        rng = random.Random(kwargs.get('seed', 1))

        var_type = kwargs.get('var_type', 'plain')


        var_type = 'tuple'
        vm = []
        # base offset
        pkt_offset = 42
        print("\nusing the following vars:\n")

        if var_type == 'plain':
            for i in range(20):
                pkt_offset += self.generate_var(rng, i, vm, pkt_offset)
        else:
            for i in range(5):
                pkt_offset += self.generate_tuple_var(rng, i, vm, pkt_offset)




        print("\n")
        # create imix streams
        return [self.create_stream(x['size'], x['pps'],x['isg'] , vm) for x in self.streams_def]



# dynamic load - used for trex console or simulator
def register():
    return STLMultiCore()



