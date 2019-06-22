
ASTF topology Module
====================

Using the topology object you can build a multiple networks and associate each client to a network


ASTFTopology class
------------------

.. autoclass:: trex.astf.topo.ASTFTopology
    :members:
    :member-order: bysource


TopoGW class
-----------------

.. autoclass:: trex.astf.topo.TopoGW
    :members:
    :member-order: bysource


TopoVIF class
-----------------

.. autoclass:: trex.astf.topo.TopoVIF
    :members:
    :member-order: bysource


ASTFClient snippet
------------------

Basic topology usage:

.. code-block:: python

    c = self.astf_trex
    topo = ASTFTopology()
    topo.add_vif(
       port_id = '0.2',
       src_mac = '12:12:12:12:12:12',
       src_ipv4 = '5.5.5.5',
       vlan = 30)

    topo.add_gw(
       port_id = '0.2',
        src_start = '16.0.0.0',
        src_end = '16.0.0.2',
        dst = '45:45:45:45:45:45')

    try:
        c.topo_load(topo)
        c.topo_resolve() # get MACs from IPs. On success, upload topology to server
        c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'udp1.py'))
        c.start(mult = 10000, duration = 1)
        c.wait_on_traffic()
    finally:
        c.stop()
        c.topo_clear()

Load/Save to file:

.. code-block:: python

    c = self.astf_trex
    c.topo_load('some_topology.py', {'count': 3})
    c.topo_save('/tmp/topo_copy.py')

Topology Python file:

.. code-block:: python

    from trex.astf.topo import *

    # reserved name "get_topo", will be called by "load" function
    def get_topo(**kw):
        print('Creating profile with variables: %s' % kw)

        topo = ASTFTopology()
        topo.add_vif(
            port_id = '0.1',
            src_mac = '12:12:12:12:12:12')
        topo.add_gw(
            port_id = '0.1',
            dst = '15:15:15:15:15:15',
            src_start = '10.0.0.1',
            src_end = '10.0.0.16')
        return topo


