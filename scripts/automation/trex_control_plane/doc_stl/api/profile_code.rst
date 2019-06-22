
Traffic profile modules 
=======================

The TRex STLProfile traffic profile includes a number of streams. The profile is a ``program`` of related streams.
Each stream can trigger another stream. Each stream can be named. For a full set of examples, see Manual_.

..   _Manual: ../trex_stateless.html


Example::

    def create_stream (self):

        # create a base packet and pad it to size
        size = self.fsize - 4; # no FCS
        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt1 =  Ether()/IP(src="16.0.0.2",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt2 =  Ether()/IP(src="16.0.0.3",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'


        return STLProfile( [ STLStream( isg = 1.0, # star in delay in usec 
                                        packet = STLPktBuilder(pkt = base_pkt/pad),
                                        mode = STLTXCont( pps = 10),
                                        ), 

                             STLStream( isg = 2.0,
                                        packet  = STLPktBuilder(pkt = base_pkt1/pad),
                                        mode    = STLTXCont( pps = 20),
                                        ),

                             STLStream(  isg = 3.0,
                                         packet = STLPktBuilder(pkt = base_pkt2/pad),
                                         mode    = STLTXCont( pps = 30)
                                         
                                        )
                            ]).get_streams()


STLProfile class
----------------

.. autoclass:: trex.stl.trex_stl_streams.STLProfile
    :members: 
    :member-order: bysource

STLStream 
---------

.. autoclass:: trex.stl.trex_stl_streams.STLStream
    :members: 
    :member-order: bysource
    

STLStream modes
----------------

.. autoclass:: trex.stl.trex_stl_streams.STLTXMode
    :members: 
    :member-order: bysource

.. autoclass:: trex.stl.trex_stl_streams.STLTXCont
    :members: 
    :member-order: bysource

.. autoclass:: trex.stl.trex_stl_streams.STLTXSingleBurst
    :members: 
    :member-order: bysource

.. autoclass:: trex.stl.trex_stl_streams.STLTXMultiBurst
    :members: 
    :member-order: bysource

.. autoclass:: trex.stl.trex_stl_streams.STLFlowStats
    :members: 
    :member-order: bysource

.. autoclass:: trex.stl.trex_stl_streams.STLFlowLatencyStats
    :members: 
    :member-order: bysource
    



STLProfile snippet
------------------


.. code-block:: python

    # STLProfile Example1


        size = self.fsize - 4; # no FCS
        base_pkt =  Ether()/IP(src="16.0.0.1",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt1 =  Ether()/IP(src="16.0.0.2",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        base_pkt2 =  Ether()/IP(src="16.0.0.3",dst="48.0.0.1")/UDP(dport=12,sport=1025)
        pad = max(0, size - len(base_pkt)) * 'x'


        return STLProfile( [ STLStream( isg = 10.0, # star in delay 
                                        name    ='S0',
                                        packet = STLPktBuilder(pkt = base_pkt/pad),
                                        mode = STLTXSingleBurst( pps = 10, total_pkts = 10),
                                        next = 'S1'), # point to next stream 

                             STLStream( self_start = False, # stream is  disabled enable trow S0
                                        name    ='S1',
                                        packet  = STLPktBuilder(pkt = base_pkt1/pad),
                                        mode    = STLTXSingleBurst( pps = 10, total_pkts = 20),
                                        next    = 'S2' ),

                             STLStream(  self_start = False, # stream is  disabled enable trow S0
                                         name   ='S2',
                                         packet = STLPktBuilder(pkt = base_pkt2/pad),
                                         mode = STLTXSingleBurst( pps = 10, total_pkts = 30 )
                                        )
                            ]).get_streams()


.. code-block:: python

    # STLProfile Example2


        class STLS1(object):
        
            def get_streams (self, direction = 0):
                return [STLStream(packet = STLPktBuilder(pkt ="stl/yaml/udp_64B_no_crc.pcap"), 
                                  mode = STLTXCont(pps=1000),
                                  flow_stats = STLFlowStats(pg_id = 7)),
        
                        STLStream(packet = STLPktBuilder(pkt ="stl/yaml/udp_594B_no_crc.pcap"),
                                  mode = STLTXCont(pps=5000),
                                  flow_stats = STLFlowStats(pg_id = 12))
                       ]
        


