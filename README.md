This document describes how to set up TRex to run a test load against
a MidoNet agent.

# Overview

TRex is a load testing tool from Cisco. It generates a load and sends
it over the network to a device under test (DUT) should send it back
so that TRex can calculate packet latency and drop rates.

<pre>
           +=========+
           |         |
           |  TRex   |
      +----|         |<<--+
      |    +=========+    |
      |                   |
  outbound             inbound
  traffic              traffic
      |                   |
      |                   |
      |    +=========+    |
      |    |  (DUT)  |    |
      +-->>| MidoNet |----+
           |  Agent) |
           +=========+
</pre>
		   
To simulate a gateway load, we need to simulate traffic on two
physical ports of the gateway machine. The uplink port receives
traffic from the internet and forwards it, encapsulated in VxLan, to
compute nodes via the underlay port. The underlay port receives
encapsulated VxLan traffic, and flow state traffic. The encapsulated
traffic is decapsulated and sent out the uplink. The flow state
traffic terminates at the MidoNet agent.

# Physical Setup

For this testing you need two physical machines, one for TRex and one
for MidoNet (hereafter referred to as the DUT machine).

The TRex needs to have one port on the same L2 segment as the gateway
uplink port and one port on the same L2 segment as the gateway
underlay port.

You probably need to rewire stuff. Both network interfaces need to be
of the same type. This has only been tested with Intel 10-Gigabit
X540-AT2, and will only work with interfaces that use the same chip.

Once the interfaces are on the correct L2 segments, take note of the
PCI IDs and Mac addresses of each interface. They'll be needed later
for configuration.

<pre>
   +========+          +=========+
   |        |          |  (DUT)  |
   |  TRex  |          | MidoNet |
   |        |          |  Agent) |
   +==+===+=+          +==+====+=+
      |   |               |    |
      |   |               |    |
      |   | Overlay Network    |
      +-------------------+    |
          |     Uplink Network |
          +--------------------+
</pre>
		  
# Software Setup

## Compiling TRex

Testing MidoNet with TRex requires a special version of TRex that can
correlate VxLan encapsulated packets with non-encapsulation
packets. This modification is available in this repo.

To build TRex, the following are required:
 - Ubuntu 14.04 LTS
 - APT packages: build-essential, zlib1g-dev, git

Build using the following commands:
```
git clone https://github.com/midonet/trex-core
cd trex-core/linux_dpdk
./b configure
./b
```

This will create the TRex server binary in ```trex-core/scripts```.

TRex runs as a server, which is controlled by scripts via a rpc
mechanism. To run the server, you first need to create a configuration
file.

## Configuring TRex

The TRex configuration file is ```/etc/trex_cfg.yaml```.

The only field that needs to be changed is the interfaces. interfaces
should contain the 2 PCI IDs of the interfaces on the correct L2
segments.

You can change the mac addresses, but they are overridden by the load
generation script in any case.

```
- port_limit      : 2         # this option can limit the number of port of the platform
  version         : 2
  interfaces    : ["02:00.0","02:00.1"] # the interfaces using ./dpdk_setup_ports.py -s
  port_info       :  # set eh mac addr
          - dest_mac        :   [0xac,0xca,0xba,0x32,0x9f,0x3f]    # router mac addr should be taken from router
            src_mac         :   [0xa0,0x36,0x9f,0x60,0x7e,0x6c]  # source mac-addr - taken from ifconfig
          - dest_mac        :   [0xa0,0x36,0x9f,0x60,0x7f,0x34]  # router mac addr  taken from router
            src_mac         :   [0xa0,0x36,0x9f,0x60,0x7e,0x6e]  #source mac-addr  taken from ifconfig
```

## Running the server

The server should be run from ```trex-core/scripts```.

```
trex-core/scripts # ./t-rex-64 -i
```

This will start a console application that displays the counters for
the two attached interfaces.

## Load generation scripts

The load generation script for a gateway load is in ```trex-core/midotrex```.
First install the required python dependencies.

```
trex-core/midotrex # pip install -r dependencies.txt
```

The trex library needs to be added to python path.
```
export PYTHONPATH=/trex-core/scripts/automation/trex_control_plane/stl
```

The script needs a configuration profile. You can find a sample in ```sa5-5.2.yaml```.

## Profile configuration fields

The following fields need to be changed for your specific setup.

| Field                | Description
|----------------------|-----------------------------------------
| tunnel_port_index    | The index, in trex_cfg.yaml's interface list, of the port on the underlay L2 segment. Zero based.
| uplink_port_index    | The index of the port on the underlay L2 list, of the port on the uplink L2 segment. Zero based.
| trex_underlay_mac    | The mac address of the interface on the underlay L2 segment on the TRex machine.
| trex_uplink_mac      | The mac address of the interface on the uplink L2 segment on the TRex machine.
| agent_underlay_mac   | The mac address of the interface on the underlay L2 segment on the DUT machine.
| agent_uplink_mac     | The mac address of the interface on the uplink L2 segment on the DUT machine.
| agent_underlay_ip    | The IP address of the DUT machine interface on the underlay network.
| agent_uplink_tunnelkey  | The tunnel key of the uplink port in the neutron topology (see below).
| agent_uplink_port_uuid  | The UUID of the uplink port in the neutron topology (see below).
| uplink_stream_target_ip | The floating IP that traffic coming in from the uplink should be directed to.

The load generation script can send statistics to influxdb as it
runs. This doesn't need to be configured, but it makes it easier to
correlate performance changes with system events. If this has an
invalid configuration a summary of the run will be printed out when it
finishes.

| Field               | Description
|---------------------|------------------------------------------
| influxdb_host       | The influxDB host to send statistic to.
| influxdb_port       | The port on the influxDB host to send statistics to.
| influxdb_database   | The database in influxDB in which the statistics should be stored.

The following fields are not setup specific, and are best left as they
are.

| Field              | Description
|--------------------|------------------------------------------
| uplink_stream_id   | A unique identifier for the TRex uplink stream.
| tunnel_stream_id   | A unique identifier for the TRex underlay stream.
| protobuf_flowstate | Whether to use protobuf based flow state. Should be true for testing 1.9 agents, false otherwise.

## Neutron topology creation

For the DUT to be able to process the packets it needs a neutron
topology with the following characteristics.

 - A provider router with an uplink port
 - The uplink port bound to the port on the DUT which is on the uplink
   L2 segment.
 - A public network, with subnet for allocating floating IPs.
 - A tenant router and tenant network.
 - A port on the tenant network, with an associated floating IP, and
   open security group (i.e. allow all inbound traffic).

The tenant port needs to be bound to a fake port on a fake agent.

For the load generator configuration you need to the tunnel key for
the uplink port. This can only be found by digging into
ZooKeeper. Find the uuid of uplink port. Then look for the value of
the "tunnel_key" field in ```/midonet/zoom/0/models/Port/<UPLINK_PORT_UUID>```.

## Fake Agent

For traffic to be able to traverse the neutron topology, from the
uplink to the tenant network port, the tenant port must be bound and
active. For this we use the "Fake Agent".

The fake agent watches for any ports that are bound to it and
automatically sets them as active.

There are two fake agents, one for 1.9 and one for 5.x. In this
instance, I'll only describe how to build and run it for 5.x.

The fake agent exists in the MidoNet repo. To build.
```
git clone https://github.com/midonet/midonet
cd midonet
./gradlew :midolman:shadowJar
```

This will build a superjar with all dependencies. To run the fake
agent...
```
java -cp midolman/build/libs/midolman-<version>-all.jar \
    org.midonet.services.FakeAgent                      \
    -zk <zookeeper> -hostIdFile fake-agent.id
```

The fake-agent.id will store the UUID of the fake agent, so that it
can survive a reboot of the process.

A new agent should appear in the host list in midonet-cli. Add it to
your tunnel zone, and bind the tenant port it to. The interface in the
binding can be junk. It's unused.

```
midonet> host list
...
host host3 name sa-lab-03 alive true flooding-proxy-weight 1 container-weight 1 container-limit no-limit enforce-container-limit false
...

midonet> tunnel-zone tzone0 add member host host3 address 192.168.200.3

midonet> host host3 add binding port <tenant_port_uuid> interface foobar
```

Finally, you need to add a static arp entry to the gateway so that it
will send traffic back to TRex. Use the address you use to add the
fake agent to the tunnel zone, and set the mac address the address of
the TRex machines interface on the underlay network.

```
ip neigh change 192.168.200.2 lladdr de:ad:be:ef:00:01 dev ens21
```

## Running the load

You should be finally able to run the load generation script.
```
trex-core/midotrex # python gateway-load.py --help
usage: gateway-load.py [-h] [--duration DURATION] [--flowrate FLOWRATE]
                       [--gateways GATEWAYS] [--warmup WARMUP]
                       [--profile PROFILE]

Run midonet trex workload

optional arguments:
  -h, --help           show this help message and exit
  --duration DURATION
  --flowrate FLOWRATE
  --gateways GATEWAYS
  --warmup WARMUP
  --profile PROFILE

trex-core/midotrex # python gateway-load.py --duration 30 --warmup 0 --flowrate 100 --profile sa5-5.2.yaml
###[ Ethernet ]###
  dst       = fa:16:3e:46:9a:87
  src       = a0:36:9f:60:7e:6e
  type      = 0x800
###[ IP ]###
     version   = 4L
     ihl       = 5L

...
...

```

As the load runs, you should see opackets and ipackets changes for
both ports. If not something is broken in your setup.

