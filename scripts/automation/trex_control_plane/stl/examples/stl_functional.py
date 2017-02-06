import stl_path
from trex_stl_lib.api import *

"""
An example on how to use TRex for functional tests

It can be used for various tasks and can replace simple Pagent/Scapy
low rate tests
"""

def test_dot1q (c, rx_port, tx_port):
   
    # activate service mode on RX code
    c.set_service_mode(ports = rx_port)

    # generate a simple Dot1Q
    pkt = Ether() / Dot1Q(vlan = 100) / IP()

    # start a capture
    capture = c.start_capture(rx_ports = rx_port)

    # push the Dot1Q packet to TX port... we need 'force' because this is under service mode
    print('\nSending 1 Dot1Q packet(s) on port {}'.format(tx_port))

    c.push_packets(ports = tx_port, pkts = pkt, force = True)
    c.wait_on_traffic(ports = tx_port)

    rx_pkts = []
    c.stop_capture(capture_id = capture['id'], output = rx_pkts)

    print('\nRecived {} packets on port {}:\n'.format(len(rx_pkts), rx_port))
    
    c.set_service_mode(ports = rx_port, enabled = False)

    # got back one packet
    assert(len(rx_pkts) == 1)
    rx_scapy_pkt = Ether(rx_pkts[0]['binary'])

    # it's a Dot1Q with the same VLAN
    assert('Dot1Q' in rx_scapy_pkt)
    assert(rx_scapy_pkt.vlan == 100)

    
    rx_scapy_pkt.show2()

    
# test a echo request / echo reply
def test_ping (c, tx_port, rx_port):
    
    # activate service mode on RX code
    c.set_service_mode(ports = [tx_port, rx_port])

    # fetch the config
    tx_port_attr = c.get_port_attr(port = tx_port)
    rx_port_attr = c.get_port_attr(port = rx_port)
    
    assert(tx_port_attr['layer_mode'] == 'IPv4')
    assert(rx_port_attr['layer_mode'] == 'IPv4')
    
    pkt = Ether() / IP(src = tx_port_attr['src_ipv4'], dst = rx_port_attr['src_ipv4']) / ICMP(type = 8)

    # start a capture on the sending port
    capture = c.start_capture(rx_ports = tx_port)
    
    print('\nSending ping request on port {}'.format(tx_port))

    # send the ping packet
    c.push_packets(ports = tx_port, pkts = pkt, force = True)
    c.wait_on_traffic(ports = tx_port)

    # fetch the packet
    rx_pkts = []
    c.stop_capture(capture_id = capture['id'], output = rx_pkts)

    print('\nRecived {} packets on port {}:\n'.format(len(rx_pkts), tx_port))
    
    c.set_service_mode(ports = rx_port, enabled = False)

    # got back one packet
    assert(len(rx_pkts) == 1)
    rx_scapy_pkt = Ether(rx_pkts[0]['binary'])

    # check for ICMP reply
    assert('ICMP' in rx_scapy_pkt)
    assert(rx_scapy_pkt['ICMP'].type == 0)
    
    rx_scapy_pkt.show2()
    
def main ():
    
    # create a client
    c = STLClient()

    try:
        # connect to the server
        c.connect()

        # there should be at least two ports connected
        tx_port, rx_port = stl_map_ports(c)['bi'][0]
        c.reset(ports = [tx_port, rx_port])

        # test 1
        test_dot1q(c, tx_port, rx_port)
        
        # test 2
        test_ping(c, tx_port, rx_port)
 
    except STLError as e:
        print(e)
    
    finally:
        c.stop()
        c.remove_all_captures()
        c.set_service_mode(enabled = False)
        c.disconnect()



if __name__ == '__main__':
    main()

