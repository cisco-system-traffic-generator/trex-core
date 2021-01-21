from trex_stl_lib.api import *
import argparse

class STLBench(object):
    """
    tunables:
        - vm : type (string)
            - var1: creates packets with one variable of src ip in the vm in the range 16.0.0.0 - 16.0.255.255 with auto increment
            - var2: creates packets with two variable, src ip and dst ip, in the vm with auto increment. The src ip range is
                    16.0.0.0 - 16.0.255.255 and the dst ip range is 48.0.0.0 - 48.0.255.255
            - random: create packets with random ip source address within the range 16.0.0.0 - 16.0.255.255
            - size: create packets with random size in the range 60-65490. This is done by the Field Engine so each
                    packet in the stream will have a random size.
            - tuple: make use of tuple variable in the vm. The tuple variable consist of (IP.src, Port.src),
                    where the ip.src values taken from the range 16.0.0.0 - 16.0.255.255 and the ports values taken 
                    taken from the range 1234 - 65500.
            - cached: make use of cache with size 255
        size : type (int)
             - define the packet's size in the stream.
        flow : type (string)
            - fs: creates stream with flow stats
            - fsl: creates stream with latency
            - no-fs: creates stream without flow stats
        pg_id : type (int)
                default : 7
            - define the packet group ID
        direction type (int)
            - define the direction of the packets
            - 0: the direction is from src - dst
            - 1: the direction is from dst - src
    """
    ip_range = {}
    ip_range['src'] = {'start': '16.0.0.0', 'end': '16.0.255.255'}
    ip_range['dst'] = {'start': '48.0.0.0', 'end': '48.0.255.255'}
    ports = {'min': 1234, 'max': 65500}
    pkt_size = {'min': 64, 'max': 9216}
    imix_table = [ {'size': 60,   'pps': 28,  'isg': 0 },
                   {'size': 590,  'pps': 20,  'isg': 0.1 },
                   {'size': 1514, 'pps':  4,   'isg':0.2 } ]

    def __init__ (self):
        self.pg_id = 0

    def create_stream (self, stl_flow, size, vm, src, dst, pps = 1, isg = 0):
        # Create base packet and pad it to size
        base_pkt = Ether()/IP(src=src, dst=dst)/UDP(chksum=0)
        pad = max(0, size - len(base_pkt) - 4) * 'x'
        pkt = STLPktBuilder(pkt=base_pkt/pad,
                            vm=vm)
        return STLStream(packet=pkt,
                         mode=STLTXCont(pps=pps),
                         isg=isg,
                         flow_stats=stl_flow)

    def get_streams (self, port_id, direction, tunables, **kwargs):
        parser = argparse.ArgumentParser(description='Argparser for {}'.format(os.path.basename(__file__)), 
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        parser.add_argument('--size',
                            type=str,
                            default=64,
                            help="""define the packet's size in the stream.
                                    choose imix or positive integ
                                    imix - create streams with packets size 60, 590, 1514.
                                    positive integer number - the packets size in the stream.""")
        parser.add_argument('--vm',
                            type=str,
                            default=None,
                            choices={'cached', 'var1', 'var2', 'random', 'tuple', 'size'},
                            help='define the field engine behavior')
        parser.add_argument('--flow',
                            type=str,
                            default="no-fs",
                            choices={'no-fs', 'fs', 'fsl'},
                            help='''Set to fs/fsl if you wants stats per stream.
                                    fs - create streams with flow stats.
                                    fsl - create streams with latency.
                                    no-fs - streams without flow stats''')
        parser.add_argument('--pg_id',
                            type=int,
                            default=7,
                            help='define the packet group ID')

        args = parser.parse_args(tunables)

        size, vm, flow = args.size, args.vm, args.flow
        if size != "imix":
            size = int(size)
        self.pg_id = args.pg_id + port_id
        if direction == 0:
            src, dst = self.ip_range['src'], self.ip_range['dst']
        else:
            src, dst = self.ip_range['dst'], self.ip_range['src']

        vm_var = STLVM()
        if not vm or vm == 'none':
            pass

        elif vm == 'var1':
            vm_var.var(name='src', min_value=src['start'], max_value=src['end'], size=4, op='inc')
            vm_var.write(fv_name='src', pkt_offset='IP.src')
            vm_var.fix_chksum()

        elif vm == 'var2':
            vm_var.var(name='src', min_value=src['start'], max_value=src['end'], size=4, op='inc')
            vm_var.var(name='dst', min_value=dst['start'], max_value=dst['end'], size=4, op='inc')
            vm_var.write(fv_name='src', pkt_offset='IP.src')
            vm_var.write(fv_name='dst', pkt_offset='IP.dst')
            vm_var.fix_chksum()

        elif vm == 'random':
            vm_var.var(name='src', min_value=src['start'], max_value=src['end'], size=4, op='random')
            vm_var.write(fv_name='src', pkt_offset='IP.src')
            vm_var.fix_chksum()

        elif vm == 'tuple':
            vm_var.tuple_var(ip_min=src['start'], ip_max=src['end'], port_min=self.ports['min'], port_max=self.ports['max'], name='tuple')
            vm_var.write(fv_name='tuple.ip', pkt_offset='IP.src')
            vm_var.write(fv_name='tuple.port', pkt_offset='UDP.sport')
            vm_var.fix_chksum()

        elif vm == 'size':
            if size == 'imix':
                raise STLError("Can't use VM of type 'size' with IMIX.")

            size = self.pkt_size['max']
            l3_len_fix = -len(Ether())
            l4_len_fix = l3_len_fix - len(IP())
            vm_var.var(name='fv_rand', min_value=(self.pkt_size['min'] - 4), max_value=(self.pkt_size['max'] - 4), size=2, op='random')
            vm_var.trim(fv_name='fv_rand')
            vm_var.write(fv_name='fv_rand', pkt_offset='IP.len', add_val=l3_len_fix)
            vm_var.write(fv_name='fv_rand', pkt_offset='UDP.len', add_val=l4_len_fix)
            vm_var.fix_chksum()

        elif vm == 'cached':
            vm_var.var(name='src', min_value=src['start'], max_value=src['end'], size=4, op='inc')
            vm_var.write(fv_name='src', pkt_offset='IP.src')
            vm_var.fix_chksum()
            # set VM as cached with 255 cache size of 255
            vm_var.set_cached(255)

        else:
            raise Exception("VM '%s' not available" % vm)


        if flow == 'no-fs':
            stl_flow = None

        elif flow == 'fs':
            stl_flow = STLFlowStats(pg_id=self.pg_id)

        elif flow == 'fsl':
             stl_flow = STLFlowLatencyStats(pg_id=self.pg_id)

        else:
            raise Exception("FLOW '%s' not available" % flow)


        if size == 'imix':
            return [self.create_stream(stl_flow, p['size'], vm_var, src=src['start'], dst=dst['start'], pps=p['pps'], isg=p['isg']) for p in self.imix_table]


        return [self.create_stream(stl_flow, size, vm_var, src=src['start'], dst=dst['start'])]


# dynamic load - used for trex console or simulator
def register():
    return STLBench()




