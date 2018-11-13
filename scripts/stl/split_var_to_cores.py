from trex_stl_lib.api import *

class STLS1(object):

    def create_stream (self, direction):

        base_pkt =  Ether()/IP()/UDP(dport=12,sport=1025)

        ip_range = {'src': {'start': "10.0.0.1", 'end': "10.0.0.254"},
                    'dst': {'start': "8.0.0.1",  'end': "8.0.0.254"}}

        if (direction == 0):
            src = ip_range['src']
            dst = ip_range['dst']
        else:
            src = ip_range['dst']
            dst = ip_range['src']


        vm = STLVM()

        vm.var(name="src", min_value=src['start'], max_value=src['end'], size=4, op='inc', split_to_cores = False)
        vm.var(name="dst", min_value=dst['start'], max_value=dst['end'], size=4, op='inc')
        vm.write(fv_name='src', pkt_offset='IP.src')
        vm.write(fv_name='dst', pkt_offset='IP.dst')
        vm.fix_chksum(offset='IP')
        return STLStream(packet = STLPktBuilder(pkt = base_pkt, vm = vm),
                         mode = STLTXCont())


    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream(direction) ]


# dynamic load - used for trex console or simulator
def register():
    return STLS1()



