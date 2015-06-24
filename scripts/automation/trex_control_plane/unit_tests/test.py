from mininet.topo import Topo
from mininet.link import TCLink
from mininet.net import Mininet
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.util import dumpNodeConnections
from mininet.log import setLogLevel

class MyTopo( Topo ):
    "Simple topology example."

    def __init__( self ):
        "Create custom topo."

        # Initialize topology
        Topo.__init__( self )

        # Add hosts and switches
        leftHost = self.addHost( 'h1' )
        rightHost = self.addHost( 'h2' )
        Switch = self.addSwitch( 's1' )

        # Add links
        self.addLink( leftHost, Switch ,bw=10, delay='5ms')
        self.addLink( Switch, rightHost  )


topos = { 'mytopo': ( lambda: MyTopo() ) }

# 1. http server example
# 
#mininet> h1 python -m SimpleHTTPServer 80 &
#mininet> h2 wget -O - h1
# 2. limit mss example 
#decrease the MTU ifconfig eth0 mtu 488

