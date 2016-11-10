#
# Copyright 2016 Midokura SARL
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
try:
    from trex_stl_lib.api import *
except ImportError:
    print "You need to add <TREX_ROOT>/scripts/automation/trex_control_plane/stl to PYTHONPATH"
    sys.exit(-1)

import uuid

# RFC 7348 - Virtual eXtensible Local Area Network (VXLAN):
# A Framework for Overlaying Virtualized Layer 2 Networks over Layer 3 Networks
# http://tools.ietf.org/html/rfc7348
_VXLAN_FLAGS = ['R' for i in range(0, 24)] + ['R', 'R', 'R', 'I', 'R', 'R', 'R', 'R', 'R'] 

class VXLAN(Packet):
    name = "VXLAN"
    fields_desc = [FlagsField("flags", 0x08000000, 32, _VXLAN_FLAGS),
                   X3BytesField("vni", 0),
                   XByteField("reserved", 0x00)]

    def mysummary(self):
        return self.sprintf("VXLAN (vni=%VXLAN.vni%)")

bind_layers(UDP, VXLAN, dport=4789)
bind_layers(UDP, VXLAN, dport=6677)
bind_layers(VXLAN, Ether)

def tunnel_stream(stream_id,
                  trex_underlay_mac, agent_underlay_mac,
                  agent_underlay_ip, agent_uplink_tunnelkey,
                  agent_uplink_mac, trex_uplink_mac,
                  flow_rate):
    vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src",
                                    min_value="10.10.10.100",
                                    max_value="10.10.10.200",
                                    size=4, op="inc"),
                       STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),
                       STLVmFixIpv4(offset = "IP"), # fix checksum
                   ], split_by_field = "ip_src", cache_size=0)
    pkt =  Ether(src=trex_underlay_mac,dst=agent_underlay_mac)\
           /IP(dst=agent_underlay_ip)/UDP(sport=50007,dport=6677,chksum=0)\
           /VXLAN(vni=agent_uplink_tunnelkey)\
           /Ether(src=agent_uplink_mac, dst=trex_uplink_mac)\
           /IP(src="5.5.5.3", dst="5.5.5.2")/UDP()/('x'*200)
    pkt.show2()

    return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = vm),
                     mode = STLTXCont( pps=flow_rate ),
                     flow_stats = STLFlowLatencyStats(
                         pg_id = stream_id))

def uplink_stream(stream_id,
                  trex_uplink_mac, agent_uplink_mac,
                  uplink_stream_target_ip,
                  flow_rate):
    vm = STLScVmRaw( [ STLVmFlowVar(name="ip_src",
                                    min_value="1.1.1.1",
                                    max_value="1.1.254.254",
                                    size=4, op="inc"),
                       STLVmFlowVar(name="src_port",
                                    min_value=1025,
                                    max_value=65000,
                                    size=2, op="random"),
                       STLVmWrFlowVar(fv_name="ip_src", pkt_offset= "IP.src" ),
                       STLVmFixIpv4(offset = "IP"), # fix checksum
                       STLVmWrFlowVar(fv_name="src_port",
                                      pkt_offset= "UDP.sport") # fix udp len
                   ], split_by_field = "ip_src", cache_size =0)
    pkt = Ether(src=trex_uplink_mac,dst=agent_uplink_mac)\
          /IP(dst=uplink_stream_target_ip)/UDP(chksum=0)/('x'*200)
    pkt.show2()

    return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = vm),
                     mode = STLTXCont( pps=flow_rate ),
                     flow_stats = STLFlowLatencyStats(
                         pg_id = stream_id))

def flowstate_stream(gateway_count,
                     trex_underlay_mac,
                     agent_underlay_mac,
                     agent_underlay_ip,
                     agent_uplink_port_uuid,
                     protobuf_flowstate,
                     flow_rate):
    # this is a flow state with 1 conntrack entry
    payload = None
    vm = []
    pkt_without_payload = \
            Ether(src=trex_underlay_mac,dst=agent_underlay_mac)\
            /IP(src="10.10.10.3",dst=agent_underlay_ip,tos=0xb8)\
            /UDP(sport=50007,dport=6677,chksum=0)/VXLAN(vni=0xffffffff)\
            /Ether(src="ac:ca:ba:00:15:01", dst="ac:ca:ba:00:15:02")\
            /IP(src="169.254.15.1", dst="169.254.15.2")/UDP(sport=2925,dport=2925)

    if protobuf_flowstate:
        payload = binascii.unhexlify('5d0801121209e04e99994b2caffb11f62bbba4c4c9cfa6180022430a2d081112070800108f8a942818352207080010b384840828b30a3212090b43e061191ba3021194075d75ddc8b89a1a1209604d06c2b900e2e611d8cfae26671f6a9a')
    else:
        payload = binascii.unhexlify('1000010001000100704ebfaea93ce0eabdabd895d1ab43bf370001f' 
                                     + 'a497e8b2ffe7cd0ea23c04547fb218a050505050000000000000000'
                                     + '00000000bce501010000000000000000000000003500e4951101014'
                                     + 'b0000450000100000100001ce46ae37ede90e4b128f5dd96bc44b9f'
                                     + '100001f04c2f9bfbc04b94da10477caae64c97')
        portid = uuid.UUID(agent_uplink_port_uuid) # uplink on gateway
        payload = payload[:0x5e] + portid.bytes_le + payload[0x6e:]
        vm = STLScVmRaw( [ STLVmFlowVar(name="key_ip_src",
                                        min_value="10.0.0.1",
                                        max_value="10.254.254.254",
                                        size=4, op="inc"),
                           STLVmWrFlowVar(fv_name="key_ip_src", pkt_offset=len(pkt_without_payload)+0x2b),
                           STLVmFixIpv4(offset = "IP"), # fix checksum 
                       ], cache_size = 1000)


    pkt = pkt_without_payload/payload

    rate = flow_rate*gateway_count
    return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = vm),
                     mode = STLTXCont( pps=(rate) ))


def tunnel_stream_elephant(stream_id,
                           trex_underlay_mac, agent_underlay_mac,
                           agent_underlay_ip, agent_uplink_tunnelkey,
                           agent_uplink_mac, trex_uplink_mac, bit_rate):
    pkt =  Ether(src=trex_underlay_mac,dst=agent_underlay_mac)\
           /IP(dst=agent_underlay_ip,src="10.10.10.100")/UDP(sport=50007,dport=6677,chksum=0)\
           /VXLAN(vni=agent_uplink_tunnelkey)\
           /Ether(src=agent_uplink_mac, dst=trex_uplink_mac)\
           /IP(src="5.5.5.3", dst="5.5.5.2")/UDP()/('x'*200)
    pkt.show2()

    return STLStream(packet = STLPktBuilder(pkt = pkt, vm = []),
                     mode = STLTXCont( bps_L1 = bit_rate ),
                     flow_stats = STLFlowLatencyStats(
                         pg_id = stream_id))

def uplink_stream_elephant(stream_id,
                           trex_uplink_mac, agent_uplink_mac,
                           uplink_stream_target_ip,
                           bit_rate):
    pkt = Ether(src=trex_uplink_mac,dst=agent_uplink_mac)\
          /IP(src="1.1.1.1",dst=uplink_stream_target_ip)\
          /UDP(sport=1024,dport=80,chksum=0)/('x'*200)
    pkt.show2()

    return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = []),
                     mode = STLTXCont( bps_L1 = bit_rate ),
                     flow_stats = STLFlowLatencyStats(
                         pg_id = stream_id))
