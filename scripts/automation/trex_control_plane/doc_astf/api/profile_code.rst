
Traffic profile modules 
========================

The TRex ASTFProfile traffic profile define how traffic should be generated for example what is the client ip ranges, server ranges, traffic pattern etc 

Example1::

    def get_profile(self):
        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], 
                                 distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], 
                                  distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        return ASTFProfile(default_ip_gen=ip_gen,
                            cap_list=[ASTFCapInfo(file="../avl/delay_10_http_browsing_0.pcap",cps=1)])


Example2::

    def create_profile(self):
        # client commands
        prog_c = ASTFProgram()
        prog_c.send(http_req)
        prog_c.recv(len(http_response))

        prog_s = ASTFProgram()
        prog_s.recv(len(http_req))
        prog_s.send(http_response)

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        tcp_params = ASTFTCPInfo(window=32768)

        # template
        temp_c = ASTFTCPClientTemplate(program=prog_c, tcp_info=tcp_params, ip_gen=ip_gen)
        temp_s = ASTFTCPServerTemplate(program=prog_s, tcp_info=tcp_params)  # using default association
        template = ASTFTemplate(client_template=temp_c, server_template=temp_s)

        # profile
        profile = ASTFProfile(default_ip_gen=ip_gen, templates=template)
        return profile

Example3::


    def create_profile(self):
        ip_gen_c1 = ASTFIPGenDist(ip_range=["16.0.0.1", "16.0.0.255"], distribution="seq")
        ip_gen_s1 = ASTFIPGenDist(ip_range=["48.0.0.1", "48.0.255.255"], distribution="seq")
        ip_gen1 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c1,
                            dist_server=ip_gen_s1)

        ip_gen_c2 = ASTFIPGenDist(ip_range=["10.0.0.1", "10.0.0.255"], distribution="seq")
        ip_gen_s2 = ASTFIPGenDist(ip_range=["20.0.0.1", "20.0.255.255"], distribution="seq")
        ip_gen2 = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                            dist_client=ip_gen_c2,
                            dist_server=ip_gen_s2)

        profile = ASTFProfile(default_ip_gen=ip_gen1, cap_list=[
            ASTFCapInfo(file="../cap2/http_get.pcap"),
            ASTFCapInfo(file="../cap2/http_get.pcap", ip_gen=ip_gen2, port=8080)
            ])

        return profile


ASTFProfile class
-----------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFProfile
    :members: 
    :member-order: bysource


ASTFTemplate class
------------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFTemplate
    :members: 
    :member-order: bysource

ASTFCapInfo class
-----------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFCapInfo
    :members: 
    :member-order: bysource

ASTFTCPServerTemplate class
---------------------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFTCPServerTemplate
    :members: 
    :member-order: bysource

ASTFTCPClientTemplate class
---------------------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFTCPClientTemplate
    :members: 
    :member-order: bysource


ASTFIPGen class
---------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFIPGen
    :members: 
    :member-order: bysource


ASTFIPGenGlobal class
---------------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFIPGenGlobal
    :members: 
    :member-order: bysource


ASTFIPGenDist class
-------------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFIPGenDist
    :members: 
    :member-order: bysource

ASTFProgram class
-----------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFProgram
    :members: 
    :member-order: bysource

ASTFAssociation class
---------------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFAssociation
    :members: 
    :member-order: bysource

ASTFAssociationRule class
-------------------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFAssociationRule
    :members: 
    :member-order: bysource

ASTFGlobal class
----------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFGlobal
    :members: 
    :member-order: bysource

ASTFTCPInfo class
-----------------

.. autoclass:: trex_astf_lib.trex_astf_client.ASTFTCPInfo
    :members: 
    :member-order: bysource


