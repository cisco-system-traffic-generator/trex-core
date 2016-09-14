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
        base_pkt = Ether()/IP()/UDP()
        pad = max(0, size - len(base_pkt)) * 'x'

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(isg = isg,
                         packet = pkt,
                         mode = STLTXCont(pps = pps))


    def generate_var (self, rng, i):

        d = {'name': str(i)}

        d['size']  = rng.choice([1, 2, 4])
        max_val = (1 << d['size'] * 8)

        d['start'] = rng.randint(0, max_val - 1)
        d['end']   = rng.randint(d['start'], max_val)
        d['step']  = rng.randint(1, 1000)
        d['op']    = rng.choice(['inc', 'dec'])
        
        return d

    def dump_var (self, var):
        return 'name: {:}, start: {:}, end: {:}, size: {:}, op: {:}, step {:}'.format(var['name'], var['start'], var['end'], var['size'], var['op'], var['step'])


    def get_streams (self, direction = 0, **kwargs):
      
        rng = random.Random(kwargs.get('seed', 1))

        # base offset
        pkt_offset = 42
        vm = []
        print("\nusing the following vars:\n")
        for i in range(10):
            var = self.generate_var(rng, i)
            print("at offset {:} - var: {:}".format(pkt_offset, self.dump_var(var)))
            vm += [STLVmFlowVar(name      = var['name'],
                                min_value = var['start'],
                                max_value = var['end'],
                                size      = var['size'],
                                op        = var['op']),
                    STLVmWrFlowVar(fv_name = var['name'], pkt_offset = pkt_offset),
                   ]
            pkt_offset += var['size']


 

        print("\n")
        # create imix streams
        return [self.create_stream(x['size'], x['pps'],x['isg'] , vm) for x in self.streams_def]



# dynamic load - used for trex console or simulator
def register():
    return STLMultiCore()



