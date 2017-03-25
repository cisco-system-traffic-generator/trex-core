from trex_stl_lib.api import *

class STLBench(object):
    ip_range = {}
    ip_range['src'] = {'start': '16.0.0.0', 'end': '16.0.255.255'}
    ip_range['dst'] = {'start': '48.0.0.0', 'end': '48.0.255.255'}
    ports = {'min': 1234, 'max': 65500}
    pkt_size = {'min': 64, 'max': 9216}
    imix_table = [ {'size': 60,   'pps': 28,  'isg':0 },
                   {'size': 590,  'pps': 20,  'isg':0.1 },
                   {'size': 1514, 'pps': 4,   'isg':0.2 } ]

    def create_stream (self, size, vm, src, dst, pps = 1, isg = 0):
        # Create base packet and pad it to size
        base_pkt = Ether()/IP(src = src, dst = dst)/UDP()
        pad = max(0, size - len(base_pkt) - 4) * 'x'

        pkt = STLPktBuilder(pkt = base_pkt/pad,
                            vm = vm)

        return STLStream(packet = pkt,
                         mode = STLTXCont(pps = pps),
                         isg = isg)


    def get_streams (self, size=64, vm=None, direction=0, **kwargs):
        if direction == 0:
            src, dst = self.ip_range['src'], self.ip_range['dst']
        else:
            src, dst = self.ip_range['dst'], self.ip_range['src']

        if not vm or vm == 'none':
            vm_var = None
        elif vm == 'var1':
            vm_var =[
                STLVmFlowVar(name = 'src', min_value = src['start'], max_value = src['end'], size = 4, op = 'inc'),
                STLVmWrFlowVar(fv_name = 'src', pkt_offset = 'IP.src'),
                STLVmFixIpv4(offset = 'IP')
            ]
        elif vm == 'var2':
            vm_var =[
                STLVmFlowVar(name = 'src', min_value = src['start'], max_value = src['end'], size = 4, op = 'inc'),
                STLVmWrFlowVar(fv_name = 'src', pkt_offset = 'IP.src'),
                STLVmFlowVar(name = 'dst', min_value = dst['start'], max_value = dst['end'], size = 4, op = 'inc'),
                STLVmWrFlowVar(fv_name = 'dst', pkt_offset = 'IP.dst'),
                STLVmFixIpv4(offset = 'IP')
            ]
        elif vm == 'random':
            vm_var =[
                STLVmFlowVar(name = 'src', min_value = src['start'], max_value = src['end'], size = 4, op = 'random'),
                STLVmWrFlowVar(fv_name = 'src', pkt_offset = 'IP.src'),
                STLVmFixIpv4(offset = 'IP')
            ]
        elif vm == 'tuple':
            vm_var =[
                STLVmTupleGen(ip_min = src['start'], ip_max = src['end'], port_min = self.ports['min'], port_max = self.ports['max'], name = 'tuple'),
                STLVmWrFlowVar(fv_name = 'tuple.ip', pkt_offset =  'IP.src'),
                STLVmWrFlowVar(fv_name = 'tuple.port', pkt_offset =  'UDP.sport'),
                STLVmFixIpv4(offset = 'IP')
            ]
        elif vm == 'size':
            if size == 'imix':
                raise STLError("Can't use VM of type 'size' with IMIX.")
            size = self.pkt_size['max']
            l3_len_fix = -len(Ether())
            l4_len_fix = l3_len_fix - len(IP())
            vm_var = [
                STLVmFlowVar(name = 'fv_rand', min_value = (self.pkt_size['min'] - 4), max_value = (self.pkt_size['max'] - 4), size = 2, op = 'random'),
                STLVmTrimPktSize('fv_rand'),
                STLVmWrFlowVar(fv_name = 'fv_rand', pkt_offset = 'IP.len', add_val = l3_len_fix),
                STLVmWrFlowVar(fv_name = 'fv_rand', pkt_offset = 'UDP.len', add_val = l4_len_fix),
                STLVmFixIpv4(offset = 'IP')
            ]
        elif vm == 'cached':
            vm_raw =[
                STLVmFlowVar(name = 'src', min_value = src['start'], max_value = src['end'], size = 4, op = 'inc'),
                STLVmWrFlowVar(fv_name = 'src', pkt_offset = 'IP.src'),
                STLVmFixIpv4(offset = 'IP')
            ]
            vm_var = STLScVmRaw(vm_raw, cache_size = 255);
        else:
            raise Exception("VM '%s' not available" % vm)
        if size == 'imix':
            return [self.create_stream(p['size'], vm_var, src = src['start'], dst = dst['start'], pps = p['pps'], isg = p['isg']) for p in self.imix_table]
        return [self.create_stream(size, vm_var, src = src['start'], dst = dst['start'])]



# dynamic load - used for trex console or simulator
def register():
    return STLBench()




