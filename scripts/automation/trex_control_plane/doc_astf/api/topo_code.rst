
ASTF topology Module 
====================

Using the topology object you can build a multiple networks and associate each client to a network 
       

ASTFTopology class
------------------

.. autoclass:: trex.astf.topo.ASTFTopology
    :members: 
    :inherited-members:
    :member-order: bysource
    

ASTFClient snippet
------------------
TODO add example for load topo from file/save to file etc 

.. code-block:: python

        c = self.astf_trex
        traffic_dist = c.get_traffic_distribution('16.0.0.0', '16.0.0.255', '1.0.0.0', False)
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
            dst = '45:45:45:45:45:45',    
        )
            
        try:
            c.topo_load(topo)
            c.topo_resolve()
            c.load_profile(os.path.join(CTRexScenario.scripts_path, 'astf', 'udp1.py'))
            c.start(mult = 10000, duration = 1)
            c.wait_on_traffic()
        finally:
            c.stop()
            c.topo_clear()

        matched = [c.get_capture_status()[k]['matched'] for k in caps.keys()]




